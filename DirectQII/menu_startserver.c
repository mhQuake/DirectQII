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

/*
=============================================================================

START SERVER MENU

=============================================================================
*/
static menuframework_s s_startserver_menu;
static char **mapnames;
static int	  nummaps;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

void DMOptionsFunc (void *self)
{
	if (s_rules_box.curvalue == 1)
		return;
	M_Menu_DMOptions_f ();
}

void RulesChangeFunc (void *self)
{
	// DM
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	else if (s_rules_box.curvalue == 1)		// coop				// PGM
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi (s_maxclients_field.buffer) > 4)
			strcpy (s_maxclients_field.buffer, "4");
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
	}
	//=====
	//PGM
	// ROGUE GAMES
	else if (Developer_searchpath (2) == 2)
	{
		if (s_rules_box.curvalue == 2)			// tag	
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
		/*
				else if(s_rules_box.curvalue == 3)		// deathball
				{
				s_maxclients_field.generic.statusbar = NULL;
				s_startserver_dmoptions_action.generic.statusbar = NULL;
				}
				*/
	}
	//PGM
	//=====
}

void StartServerActionFunc (void *self)
{
	char	startmap[1024];
	int		timelimit;
	int		fraglimit;
	int		maxclients;
	char	*spot;

	strcpy (startmap, strchr (mapnames[s_startmap_list.curvalue], '\n') + 1);

	maxclients = atoi (s_maxclients_field.buffer);
	timelimit = atoi (s_timelimit_field.buffer);
	fraglimit = atoi (s_fraglimit_field.buffer);

	Cvar_SetValue ("maxclients", M_ClampCvar (0, maxclients, maxclients));
	Cvar_SetValue ("timelimit", M_ClampCvar (0, timelimit, timelimit));
	Cvar_SetValue ("fraglimit", M_ClampCvar (0, fraglimit, fraglimit));
	Cvar_Set ("hostname", s_hostname_field.buffer);
	//	Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
	//	Cvar_SetValue ("coop", s_rules_box.curvalue );

	//PGM
	if ((s_rules_box.curvalue < 2) || (Developer_searchpath (2) != 2))
	{
		Cvar_SetValue ("deathmatch", !s_rules_box.curvalue);
		Cvar_SetValue ("coop", s_rules_box.curvalue);
		Cvar_SetValue ("gamerules", 0);
	}
	else
	{
		Cvar_SetValue ("deathmatch", 1);	// deathmatch is always true for rogue games, right?
		Cvar_SetValue ("coop", 0);			// FIXME - this might need to depend on which game we're running
		Cvar_SetValue ("gamerules", s_rules_box.curvalue);
	}
	//PGM

	spot = NULL;
	if (s_rules_box.curvalue == 1)		// PGM
	{
		if (Q_stricmp (startmap, "bunk1") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "mintro") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "fact1") == 0)
			spot = "start";
		else if (Q_stricmp (startmap, "power1") == 0)
			spot = "pstart";
		else if (Q_stricmp (startmap, "biggun") == 0)
			spot = "bstart";
		else if (Q_stricmp (startmap, "hangar1") == 0)
			spot = "unitstart";
		else if (Q_stricmp (startmap, "city1") == 0)
			spot = "unitstart";
		else if (Q_stricmp (startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState ())
			Cbuf_AddText ("disconnect\n");
		Cbuf_AddText (va ("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText (va ("map %s\n", startmap));
	}

	M_ForceMenuOff ();
}

void StartServer_MenuInit (void)
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};
	//=======
	//PGM
	static const char *dm_coop_names_rogue[] =
	{
		"deathmatch",
		"cooperative",
		"tag",
		//		"deathball",
		0
	};
	//PGM
	//=======
	char *buffer;
	char  mapsname[1024];
	char *s;
	int length;
	int i;
	FILE *fp;

	/*
	** load the list of map names
	*/
	Com_sprintf (mapsname, sizeof (mapsname), "%s/maps.lst", FS_Gamedir ());
	if ((fp = fopen (mapsname, "rb")) == 0)
	{
		if ((length = FS_LoadFile ("maps.lst", (void **) &buffer)) == -1)
			Com_Error (ERR_DROP, "couldn't find maps.lst\n");
	}
	else
	{
#ifdef _WIN32
		length = filelength (fileno (fp));
#else
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0, SEEK_SET);
#endif
		buffer = Z_Alloc (length);
		fread (buffer, length, 1, fp);
	}

	s = buffer;

	i = 0;
	while (i < length)
	{
		if (s[i] == '\r')
			nummaps++;
		i++;
	}

	if (nummaps == 0)
		Com_Error (ERR_DROP, "no maps in maps.lst\n");

	mapnames = Z_Alloc (sizeof (char *) * (nummaps + 1));
	memset (mapnames, 0, sizeof (char *) * (nummaps + 1));

	s = buffer;

	for (i = 0; i < nummaps; i++)
	{
		char  shortname[MAX_TOKEN_CHARS];
		char  longname[MAX_TOKEN_CHARS];
		char  scratch[200];
		int		j, l;

		strcpy (shortname, COM_Parse (&s));
		l = strlen (shortname);
		for (j = 0; j < l; j++)
			shortname[j] = toupper (shortname[j]);
		strcpy (longname, COM_Parse (&s));
		Com_sprintf (scratch, sizeof (scratch), "%s\n%s", longname, shortname);

		mapnames[i] = Z_Alloc (strlen (scratch) + 1);
		strcpy (mapnames[i], scratch);
	}

	mapnames[nummaps] = 0;

	if (fp != 0)
	{
		fp = 0;
		Z_Free (buffer);
	}
	else
	{
		FS_FreeFile (buffer);
	}

	/*
	** initialize the menu stuff
	*/
	s_startserver_menu.x = viddef.width * 0.50;
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x = 0;
	s_startmap_list.generic.y = 0;
	s_startmap_list.generic.name = "initial map";
	s_startmap_list.itemnames = mapnames;

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x = 0;
	s_rules_box.generic.y = 20;
	s_rules_box.generic.name = "rules";

	//PGM - rogue games only available with rogue DLL.
	if (Developer_searchpath (2) == 2)
		s_rules_box.itemnames = dm_coop_names_rogue;
	else
		s_rules_box.itemnames = dm_coop_names;
	//PGM

	if (Cvar_VariableValue ("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x = 0;
	s_timelimit_field.generic.y = 36;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy (s_timelimit_field.buffer, Cvar_VariableString ("timelimit"));

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x = 0;
	s_fraglimit_field.generic.y = 54;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy (s_fraglimit_field.buffer, Cvar_VariableString ("fraglimit"));

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is.
	** Clamping will be done when the server is actually started.
	*/
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x = 0;
	s_maxclients_field.generic.y = 72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;
	if (Cvar_VariableValue ("maxclients") == 1)
		strcpy (s_maxclients_field.buffer, "8");
	else
		strcpy (s_maxclients_field.buffer, Cvar_VariableString ("maxclients"));

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x = 0;
	s_hostname_field.generic.y = 90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy (s_hostname_field.buffer, Cvar_VariableString ("hostname"));

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name = " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x = 24;
	s_startserver_dmoptions_action.generic.y = 108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name = " begin";
	s_startserver_start_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x = 24;
	s_startserver_start_action.generic.y = 128;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem (&s_startserver_menu, &s_startmap_list);
	Menu_AddItem (&s_startserver_menu, &s_rules_box);
	Menu_AddItem (&s_startserver_menu, &s_timelimit_field);
	Menu_AddItem (&s_startserver_menu, &s_fraglimit_field);
	Menu_AddItem (&s_startserver_menu, &s_maxclients_field);
	Menu_AddItem (&s_startserver_menu, &s_hostname_field);
	Menu_AddItem (&s_startserver_menu, &s_startserver_dmoptions_action);
	Menu_AddItem (&s_startserver_menu, &s_startserver_start_action);

	Menu_Center (&s_startserver_menu);

	// call this now to set proper inital state
	RulesChangeFunc (NULL);
}

void StartServer_MenuDraw (void)
{
	Menu_Draw (&s_startserver_menu);
}

const char *StartServer_MenuKey (int key)
{
	if (key == K_ESCAPE)
	{
		if (mapnames)
		{
			int i;

			for (i = 0; i < nummaps; i++)
				Z_Free (mapnames[i]);
			Z_Free (mapnames);
		}

		mapnames = 0;
		nummaps = 0;
	}

	return Default_MenuKey (&s_startserver_menu, key);
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit ();
	M_PushMenu (StartServer_MenuDraw, StartServer_MenuKey);
}
