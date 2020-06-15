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

#include "client.h"
#include "qmenu.h"


/*
=============================================================================

COMMON STUFF

=============================================================================
*/

#define	MAX_SAVEGAMES	15

char		m_savestrings[MAX_SAVEGAMES][32];
qboolean	m_savevalid[MAX_SAVEGAMES];

void MakeGreen (char *str)
{
	for (int i = 0;; i++)
	{
		if (!str[i]) break;
		str[i] |= 0x80;
	}
}


void Create_Savestrings (void)
{
	int		i;
	FILE	*f;
	char	name[MAX_OSPATH];

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		Com_sprintf (name, sizeof (name), "%s/save/save%i/server.ssv", FS_Gamedir (), i);
		f = fopen (name, "rb");

		if (!f)
		{
			strcpy (m_savestrings[i], "<EMPTY>");
			MakeGreen (m_savestrings[i]);
			m_savevalid[i] = false;
		}
		else
		{
			FS_Read (m_savestrings[i], sizeof (m_savestrings[i]), f);

			if (i == 0)
				MakeGreen (&m_savestrings[i][9]);
			else MakeGreen (&m_savestrings[i][13]);

			fclose (f);
			m_savevalid[i] = true;
		}
	}
}


void M_DisplaySave (char *savename)
{
	int i;
	char date[10];
	char time[10];
	char *map = &savename[13];

	// copy off the date
	strncpy (date, &savename[6], 7);
	date[5] = 0;

	// copy off the time
	strncpy (time, savename, 7);
	time[5] = 0;

	// pretty up for display
	for (i = 0;; i++)
	{
		if (!date[i]) break;
		if (date[i] == ' ') date[i] = '0';
	}

	// pretty up for display
	for (i = 0;; i++)
	{
		if (!time[i]) break;
		if (time[i] == ' ') time[i] = '0';
	}
}


/*
=============================================================================

LOADGAME MENU

=============================================================================
*/

static menuframework_s	s_savegame_menu;
static menuframework_s	s_loadgame_menu;

static menuaction_s		s_loadgame_actions[MAX_SAVEGAMES];

void LoadGameCallback (void *self)
{
	menuaction_s *a = (menuaction_s *) self;

	if (m_savevalid[a->generic.localdata[0]])
		Cbuf_AddText (va ("load save%i\n", a->generic.localdata[0]));
	M_ForceMenuOff ();
}

void LoadGame_MenuInit (void)
{
	int i;

	s_loadgame_menu.x = viddef.conwidth / 2 - 120;
	s_loadgame_menu.y = viddef.conheight / 2 - 58;
	s_loadgame_menu.nitems = 0;

	Create_Savestrings ();

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		s_loadgame_actions[i].generic.name = m_savestrings[i];
		s_loadgame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_loadgame_actions[i].generic.localdata[0] = i;
		s_loadgame_actions[i].generic.callback = LoadGameCallback;

		s_loadgame_actions[i].generic.x = 0;
		s_loadgame_actions[i].generic.y = (i) * 10;

		if (i > 0)	// separate from autosave
			s_loadgame_actions[i].generic.y += 10;

		s_loadgame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem (&s_loadgame_menu, &s_loadgame_actions[i]);
	}
}


void LoadGame_MenuDraw (void)
{
	M_Banner ("m_banner_load_game");
	//	Menu_AdjustCursor( &s_loadgame_menu, 1 );
	Menu_Draw (&s_loadgame_menu);
	M_DisplaySave (m_savestrings[1]);
}

const char *LoadGame_MenuKey (int key)
{
	if (key == K_ESCAPE || key == K_ENTER)
	{
		s_savegame_menu.cursor = s_loadgame_menu.cursor - 1;
		if (s_savegame_menu.cursor < 0)
			s_savegame_menu.cursor = 0;
	}

	return Default_MenuKey (&s_loadgame_menu, key);
}

void M_Menu_LoadGame_f (void)
{
	LoadGame_MenuInit ();
	M_PushMenu (LoadGame_MenuDraw, LoadGame_MenuKey);
}


/*
=============================================================================

SAVEGAME MENU

=============================================================================
*/

static menuframework_s	s_savegame_menu;
static menuaction_s		s_savegame_actions[MAX_SAVEGAMES];

void SaveGameCallback (void *self)
{
	menuaction_s *a = (menuaction_s *) self;

	Cbuf_AddText (va ("save save%i\n", a->generic.localdata[0]));
	re.Mapshot (va ("save%i\n", a->generic.localdata[0]));
	M_ForceMenuOff ();
}


void SaveGame_MenuDraw (void)
{
	M_Banner ("m_banner_save_game");
	Menu_AdjustCursor (&s_savegame_menu, 1);
	Menu_Draw (&s_savegame_menu);
}

void SaveGame_MenuInit (void)
{
	int i;

	s_savegame_menu.x = viddef.conwidth / 2 - 120;
	s_savegame_menu.y = viddef.conheight / 2 - 58;
	s_savegame_menu.nitems = 0;

	Create_Savestrings ();

	// don't include the autosave slot
	for (i = 0; i < MAX_SAVEGAMES - 1; i++)
	{
		s_savegame_actions[i].generic.name = m_savestrings[i + 1];
		s_savegame_actions[i].generic.localdata[0] = i + 1;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;

		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = (i) * 10;

		s_savegame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem (&s_savegame_menu, &s_savegame_actions[i]);
	}
}


const char *SaveGame_MenuKey (int key)
{
	if (key == K_ENTER || key == K_ESCAPE)
	{
		s_loadgame_menu.cursor = s_savegame_menu.cursor - 1;

		if (s_loadgame_menu.cursor < 0)
			s_loadgame_menu.cursor = 0;
	}

	return Default_MenuKey (&s_savegame_menu, key);
}

void M_Menu_SaveGame_f (void)
{
	if (!Com_ServerState ())
		return;		// not playing a game

	SaveGame_MenuInit ();
	M_PushMenu (SaveGame_MenuDraw, SaveGame_MenuKey);
	Create_Savestrings ();
}


