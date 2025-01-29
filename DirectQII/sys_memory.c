/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sys_win.h

#include "ref.h"
#include <windows.h>
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

/*
==============================================================================

ZONE MEMORY ALLOCATION

using heap allocations to prevent a crash from a bad Z_Free when calling gi.FreeTags
via ge->SpawnEntities during loading of eou7.cin (Z_FreeTags now just throws away
the full heap rather than using individual Z_Free calls which makes it safe)

==============================================================================
*/

// it's possible for games to exceed this (neither baseq2 nor ctf do) which would cause an error meaning we need to increase it
#define MAX_ZONETAGS	1024

typedef struct zone_s {
	HANDLE hZone;
	int tag;
	int count;
	int bytes;
} zone_t;

zone_t z_zones[MAX_ZONETAGS];

/*
========================
Z_Free
========================
*/
void Z_Free (void *ptr)
{
	// this exists but is not actually used directly by game code, so we can simplify greatly by just making it a nop
}


/*
========================
Z_FreeTags
========================
*/
void Z_FreeTags (int tag)
{
	// this should never happen; if it does then we need to know
	if (tag < 0 || tag >= MAX_ZONETAGS)
	{
		Com_Error (ERR_FATAL, "Z_FreeTags: bad tag");
		return;
	}

	// Com_Printf ("Z_FreeTags : Freeing %i kb in %i allocations from tag %i\n", (z_zones[tag].bytes + 512) / 1024, z_zones[tag].count, tag);

	// destroy and fully clear the zone
	HeapDestroy (z_zones[tag].hZone);
	memset (&z_zones[tag], 0, sizeof (z_zones[tag]));
}


/*
========================
Z_TagAlloc
========================
*/
void *Z_TagAlloc (int size, int tag)
{
	if (tag < 0 || tag >= MAX_ZONETAGS)
	{
		Com_Error (ERR_FATAL, "Z_TagAlloc: bad tag");
		return NULL;
	}
	else
	{
		// get the correct zone
		zone_t *z = &z_zones[tag];

		// create the heap if necessary
		if (!z->hZone) z->hZone = HeapCreate (0, 0, 0);
		if (!z->hZone) Com_Error (ERR_FATAL, "Z_TagAlloc: failed on HeapCreate");

		// counts
		z->bytes += size;
		z->count++;

		// return what we got
		return HeapAlloc (z->hZone, HEAP_ZERO_MEMORY, size);
	}
}


void Z_Init (void)
{
	memset (z_zones, 0, sizeof (z_zones));
}


/*
==============================================================================

for use in the engine - these just wrap standard HeapAlloc/HeapFree for the
convenience of modules that don't include windows.h

==============================================================================
*/
void *Zone_Alloc (int size)
{
	return HeapAlloc (GetProcessHeap (), HEAP_ZERO_MEMORY, size);
}


void Zone_Free (void *ptr)
{
	HeapFree (GetProcessHeap (), 0, ptr);
	HeapCompact (GetProcessHeap (), 0);
}


/*
==============================================================================

LOAD MEMORY ALLOCATION

Temporary memory used for loading is a 64mb buffer; call Load_AllocMemory to draw down memory from the buffer, Load_FreeMemory when completed to reset the buffer

==============================================================================
*/

#define LOAD_BUFFER_SIZE 0x4000000

byte load_buffer[LOAD_BUFFER_SIZE];
int load_buffer_mark = 0;


void Hunk_FreeAll (void)
{
	if (load_buffer_mark > 0)
	{
		// future attempts to access the contents of the buffer should be errors
		memset (load_buffer, 0, load_buffer_mark);
		load_buffer_mark = 0;
	}
}


void *Hunk_Alloc (int size)
{
	// 16-align all allocations
	size = (size + 15) & ~15;

	if (load_buffer_mark + size >= LOAD_BUFFER_SIZE)
	{
		Sys_Error ("Load_AllocMemory overflow");
		return NULL;
	}
	else
	{
		byte *buf = &load_buffer[load_buffer_mark];
		load_buffer_mark += size;
		memset (buf, 0, size);
		return buf;
	}
}


int	Hunk_LowMark (void)
{
	return load_buffer_mark;
}


void Hunk_FreeToLowMark (int mark)
{
	if (mark < 0 || mark > load_buffer_mark)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %i", mark);

	memset (&load_buffer[mark], 0, load_buffer_mark - mark);
	load_buffer_mark = mark;
}


void Sys_SetupMemoryRefImports (refimport_t	*ri)
{
	// so that we don't have namespace pollution with externing the Hunk_* funcs we register the OS-specific memory allocation functions separately here
	ri->Hunk_FreeAll = Hunk_FreeAll;
	ri->Hunk_Alloc = Hunk_Alloc;
	ri->Hunk_FreeToLowMark = Hunk_FreeToLowMark;
	ri->Hunk_LowMark = Hunk_LowMark;
}


