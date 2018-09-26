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

GAME MENU

=============================================================================
*/

static int		m_game_cursor;

static menuframework_s	s_game_menu;
static menuaction_s		s_easy_game_action;
static menuaction_s		s_medium_game_action;
static menuaction_s		s_hard_game_action;
static menuaction_s		s_nightmare_game_action;
static menuaction_s		s_load_game_action;
static menuaction_s		s_save_game_action;
static menuaction_s		s_credits_action;

static void StartGame (void)
{
	// disable updates and start the cinematic going
	cl.servercount = -1;
	M_ForceMenuOff ();
	Cvar_SetValue ("deathmatch", 0);
	Cvar_SetValue ("coop", 0);

	Cvar_SetValue ("gamerules", 0);		//PGM

	Cbuf_AddText ("loading ; killserver ; wait ; newgame\n");
	cls.key_dest = key_game;
}

static void GenericGameFunc (char *skill)
{
	Cvar_ForceSet ("skill", skill);
	StartGame ();
}

static void EasyGameFunc (void *junk) { GenericGameFunc ("0"); }
static void MediumGameFunc (void *junk) { GenericGameFunc ("1"); }
static void HardGameFunc (void *junk) { GenericGameFunc ("2"); }
static void NightmareGameFunc (void *junk) { GenericGameFunc ("3"); }

static void LoadGameFunc (void *unused)
{
	M_Menu_LoadGame_f ();
}

static void SaveGameFunc (void *unused)
{
	M_Menu_SaveGame_f ();
}

static void CreditsFunc (void *unused)
{
	M_Menu_Credits_f ();
}

void Game_MenuInit (void)
{
	s_game_menu.x = viddef.conwidth * 0.50;
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type = MTYPE_ACTION;
	s_easy_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x = 0;
	s_easy_game_action.generic.y = 0;
	s_easy_game_action.generic.name = "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type = MTYPE_ACTION;
	s_medium_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x = 0;
	s_medium_game_action.generic.y = 10;
	s_medium_game_action.generic.name = "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type = MTYPE_ACTION;
	s_hard_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x = 0;
	s_hard_game_action.generic.y = 20;
	s_hard_game_action.generic.name = "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

	s_nightmare_game_action.generic.type = MTYPE_ACTION;
	s_nightmare_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_nightmare_game_action.generic.x = 0;
	s_nightmare_game_action.generic.y = 30;
	s_nightmare_game_action.generic.name = "NIGHTMARE!";
	s_nightmare_game_action.generic.callback = NightmareGameFunc;

	s_load_game_action.generic.type = MTYPE_ACTION;
	s_load_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x = 0;
	s_load_game_action.generic.y = 50;
	s_load_game_action.generic.name = "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type = MTYPE_ACTION;
	s_save_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x = 0;
	s_save_game_action.generic.y = 60;
	s_save_game_action.generic.name = "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type = MTYPE_ACTION;
	s_credits_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x = 0;
	s_credits_action.generic.y = 70;
	s_credits_action.generic.name = "credits";
	s_credits_action.generic.callback = CreditsFunc;

	Menu_AddItem (&s_game_menu, (void *) &s_easy_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_medium_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_hard_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_nightmare_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_load_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_save_game_action);
	Menu_AddItem (&s_game_menu, (void *) &s_credits_action);

	Menu_Center (&s_game_menu);
}

void Game_MenuDraw (void)
{
	M_Banner ("m_banner_game");
	Menu_AdjustCursor (&s_game_menu, 1);
	Menu_Draw (&s_game_menu);
}

const char *Game_MenuKey (int key)
{
	return Default_MenuKey (&s_game_menu, key);
}

void M_Menu_Game_f (void)
{
	Game_MenuInit ();
	M_PushMenu (Game_MenuDraw, Game_MenuKey);
	m_game_cursor = 1;
}
