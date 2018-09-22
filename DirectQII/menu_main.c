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

#include <ctype.h>
#include <io.h>
#include "client.h"
#include "qmenu.h"

static int	m_main_cursor;

/*
=======================================================================

MAIN MENU

=======================================================================
*/
#define	MAIN_ITEMS	5


void M_Main_Draw (void)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];

	char *names[] =
	{
		"m_main_game",
		"m_main_multiplayer",
		"m_main_options",
		"m_main_video",
		"m_main_quit",
		0
	};

	for (i = 0; names[i] != 0; i++)
	{
		re.DrawGetPicSize (&w, &h, names[i]);

		if (w > widest)
			widest = w;
		totalheight += (h + 12);
	}

	ystart = (viddef.height / 2 - 110);
	xoffset = (viddef.width - widest + 70) / 2;

	for (i = 0; names[i] != 0; i++)
	{
		if (i != m_main_cursor)
			re.DrawPic (xoffset, ystart + i * 40 + 13, names[i]);
	}

	strcpy (litname, names[m_main_cursor]);
	strcat (litname, "_sel");
	re.DrawPic (xoffset, ystart + m_main_cursor * 40 + 13, litname);

	M_DrawCursor (xoffset - 25, ystart + m_main_cursor * 40 + 11, (int) (cls.realtime / 100) % NUM_CURSOR_FRAMES);

	re.DrawGetPicSize (&w, &h, "m_main_plaque");
	re.DrawPic (xoffset - 30 - w, ystart, "m_main_plaque");

	re.DrawPic (xoffset - 30 - w, ystart + h + 5, "m_main_logo");
}


const char *M_Main_Key (int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	case K_ESCAPE:
		M_PopMenu ();
		break;

	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		return sound;

	case K_KP_UPARROW:
	case K_UPARROW:
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		return sound;

	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_Game_f ();
			break;

		case 1:
			M_Menu_Multiplayer_f ();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Video_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}

	return NULL;
}


void M_Menu_Main_f (void)
{
	M_PushMenu (M_Main_Draw, M_Main_Key);
}

