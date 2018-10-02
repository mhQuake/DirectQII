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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

/*

full screen console
put up loading plaque
blanked background with loading plaque
blanked background with menu
cinematics
full screen image for quit and victory

end of unit intermissions

*/

#include "client.h"

float		scr_con_current;	// aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

qboolean	scr_initialized;		// ready to draw

int			scr_draw_loading;


cvar_t		*scr_viewsize;
cvar_t		*scr_conspeed;
cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_printspeed;

cvar_t		*scr_netgraph;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;

char		crosshair_pic[MAX_QPATH];
int			crosshair_width, crosshair_height;

void SCR_TimeRefresh_f (void);
void SCR_Loading_f (void);


/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
CL_AddNetgraph

A new packet was just parsed
==============
*/
void CL_AddNetgraph (void)
{
	int		i;
	int		in;
	int		ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if (scr_debuggraph->value || scr_timegraph->value)
		return;

	for (i = 0; i < cls.netchan.dropped; i++)
		SCR_DebugGraph (30, 0x40);

	for (i = 0; i < cl.surpressCount; i++)
		SCR_DebugGraph (30, 0xdf);

	// see what the latency was on this packet
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP - 1);
	ping = cls.realtime - cl.cmd_time[in];
	ping /= 30;
	if (ping > 30)
		ping = 30;
	SCR_DebugGraph (ping, 0xd0);
}


typedef struct graphsamp_s
{
	float	value;
	int		color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, int color)
{
	values[current & 1023].value = value;
	values[current & 1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;
	int		color;

	// draw the graph
	w = viddef.conwidth;
	x = 0;
	y = viddef.conheight;

	re.DrawFill (x, y - scr_graphheight->value, w, scr_graphheight->value, 8);

	for (a = 0; a < w; a++)
	{
		i = (current - 1 - a + 1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v * scr_graphscale->value + scr_graphshift->value;

		if (v < 0)
			v += scr_graphheight->value * (1 + (int) (-v / scr_graphheight->value));

		h = (int) v % (int) scr_graphheight->value;
		re.DrawFill (x + w - 1 - a, y - h, 1, h, color);
	}
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	char	*s;
	char	line[64];
	int		i, j, l;

	strncpy (scr_centerstring, str, sizeof (scr_centerstring) - 1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = str;

	while (*s)
	{
		if (*s == '\n')
			scr_center_lines++;
		s++;
	}

	// echo it to the console
	Com_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	s = str;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (s[l] == '\n' || !s[l])
				break;

		for (i = 0; i < (40 - l) / 2; i++)
			line[i] = ' ';

		for (j = 0; j < l; j++)
			line[i++] = s[j];

		line[i] = '\n';
		line[i + 1] = 0;

		Com_Printf ("%s", line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;

		s++;		// skip the \n
	} while (1);

	Com_Printf ("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify ();
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

	// the finale prints the characters one at a time
	remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;
	y = viddef.conheight * 0.35 - (scr_center_lines * 4);

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;

		x = (viddef.conwidth - l * 8) / 2;

		for (j = 0; j < l; j++, x += 8)
		{
			re.DrawChar (x, y, start[j]);

			if (!remaining--)
			{
				re.DrawString ();
				return;
			}
		}

		re.DrawString ();
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cls.frametime;

	if (scr_centertime_off <= 0)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize", scr_viewsize->value + 10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize", scr_viewsize->value - 10);
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed
=================
*/
void SCR_Sky_f (void)
{
	float	rotate;
	vec3_t	axis;

	if (Cmd_Argc () < 2)
	{
		Com_Printf ("Usage: sky <basename> <rotate> <axis x y z>\n");
		return;
	}
	if (Cmd_Argc () > 2)
		rotate = atof (Cmd_Argv (2));
	else
		rotate = 0;
	if (Cmd_Argc () == 6)
	{
		axis[0] = atof (Cmd_Argv (3));
		axis[1] = atof (Cmd_Argv (4));
		axis[2] = atof (Cmd_Argv (5));
	}
	else
	{
		axis[0] = 0;
		axis[1] = 0;
		axis[2] = 1;
	}

	re.SetSky (Cmd_Argv (1), rotate, axis);
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);
	scr_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	scr_showturtle = Cvar_Get ("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", 0);
	scr_netgraph = Cvar_Get ("netgraph", "0", 0);
	scr_timegraph = Cvar_Get ("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get ("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get ("graphheight", "32", 0);
	scr_graphscale = Cvar_Get ("graphscale", "1", 0);
	scr_graphshift = Cvar_Get ("graphshift", "0", 0);

	// register our commands
	Cmd_AddCommand ("timerefresh", SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading", SCR_Loading_f);
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);
	Cmd_AddCommand ("sky", SCR_Sky_f);

	scr_initialized = true;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP - 1)
		return;

	re.DrawPic (64, 0, "net");
}


/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	int		w, h;

	if (!scr_showpause->value)		// turn off for screenshots
		return;

	if (!cl_paused->value)
		return;

	re.DrawGetPicSize (&w, &h, "pause");
	re.DrawPic ((viddef.conwidth - w) / 2, viddef.conheight / 2 + 8, "pause");
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	int		w, h;

	if (!scr_draw_loading)
		return;

	scr_draw_loading = false;
	re.DrawGetPicSize (&w, &h, "loading");
	re.DrawPic ((viddef.conwidth - w) / 2, (viddef.conheight - h) / 2, "loading");
}

//=============================================================================

/*
==================
SCR_RunConsole

Scroll it up or down
==================
*/
void SCR_RunConsole (void)
{
	static int con_oldtime = -1;
	float con_frametime;

	// check for first call
	if (con_oldtime < 0) con_oldtime = cls.realtime;

	// get correct frametime
	con_frametime = (float) (cls.realtime - con_oldtime) * 0.001f;
	con_oldtime = cls.realtime;

	// decide on the height of the console
	if (cls.key_dest == key_console)
		scr_conlines = 0.5;		// half screen
	else scr_conlines = 0;		// none visible

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed->value * (100.0f / 200.0f) * con_frametime;

		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;
	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed->value * (100.0f / 200.0f) * con_frametime;

		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}
}


/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	Con_CheckResize ();

	if (cls.state == ca_disconnected || cls.state == ca_connecting)
	{
		// forced full screen console
		if (cls.key_dest == key_menu)
			re.DrawFill (0, 0, viddef.conwidth, viddef.conheight, 0);
		else Con_DrawConsole (1.0f, 255);
		return;
	}

	if (cls.state != ca_active || !cl.refresh_prepped)
	{
		// connected, but can't render
		Con_DrawConsole (0.5f, 255);
		re.DrawFill (0, viddef.conheight / 2, viddef.conwidth, viddef.conheight / 2, 0);
		return;
	}

	if (scr_con_current)
	{
		if (cls.key_dest == key_menu && scr_con_current > 0.999f)
			re.DrawFill (0, 0, viddef.conwidth, viddef.conheight, 0);
		else if (cls.key_dest != key_menu)
			Con_DrawConsole (scr_con_current, (int) (scr_con_current * 320.0f));
	}
	else
	{
		if (cls.key_dest == key_game || cls.key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}

//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds ();
	cl.sound_prepped = false;		// don't play ambients
	CDAudio_Stop ();
	if (cls.disable_screen)
		return;
	if (developer->value)
		return;
	if (cls.state == ca_disconnected)
		return;	// if at console, don't bring up the plaque
	if (cls.key_dest == key_console)
		return;
	if (cl.cinematictime > 0)
		scr_draw_loading = 2;	// clear to balack first
	else
		scr_draw_loading = 1;
	SCR_UpdateScreen ();
	cls.disable_screen = Sys_Milliseconds ();
	cls.disable_servercount = cl.servercount;
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
	cls.disable_screen = 0;
	Con_ClearNotify ();
}

/*
================
SCR_Loading_f
================
*/
void SCR_Loading_f (void)
{
	SCR_BeginLoadingPlaque ();
}

/*
================
SCR_TimeRefresh_f
================
*/
int entitycmpfnc (const entity_t *a, const entity_t *b)
{
	/*
	** all other models are sorted by model then skin
	*/
	if (a->model == b->model)
	{
		return ((int) a->skin - (int) b->skin);
	}
	else
	{
		return ((int) a->model - (int) b->model);
	}
}

void SCR_TimeRefresh_f (void)
{
	int		i;
	int		start, stop;
	float	startangle, time;
	int		timeRefreshTime = 1800;

	if (cls.state != ca_active)
		return;

	startangle = cl.refdef.viewangles[1];
	start = Sys_Milliseconds ();

	// do a 360 in 1.8 seconds
	for (i = 0; ; i++)
	{
		cl.refdef.viewangles[1] = startangle + (float) (Sys_Milliseconds () - start) * (360.0f / timeRefreshTime);

		re.BeginFrame (&viddef);
		re.RenderFrame (&cl.refdef);
		re.EndFrame (false);

		if ((time = Sys_Milliseconds () - start) >= timeRefreshTime) break;
	}

	stop = Sys_Milliseconds ();
	cl.refdef.viewangles[1] = startangle;
	time = (stop - start) / 1000.0;
	Com_Printf ("%i frames, %f seconds (%f fps)\n", i, time, (float) i / time);
}


//===============================================================


/*
================
SizeHUDString

Allow embedded \n in the string
================
*/
void SizeHUDString (char *string, int *w, int *h)
{
	int		lines, width, current;

	lines = 1;
	width = 0;

	current = 0;
	while (*string)
	{
		if (*string == '\n')
		{
			lines++;
			current = 0;
		}
		else
		{
			current++;
			if (current > width)
				width = current;
		}
		string++;
	}

	*w = width * 8;
	*h = lines * 8;
}

void DrawHUDString (char *string, int x, int y, int centerwidth, int xor)
{
	int		margin;
	char	line[1024];
	int		width;
	int		i;

	margin = x;

	while (*string)
	{
		// scan out one line of text from the string
		width = 0;
		while (*string && *string != '\n')
			line[width++] = *string++;
		line[width] = 0;

		if (centerwidth)
			x = margin + (centerwidth - width * 8) / 2;
		else
			x = margin;

		for (i = 0; i < width; i++)
		{
			re.DrawChar (x, y, line[i] ^ xor);
			x += 8;
		}

		re.DrawString ();

		if (*string)
		{
			string++;	// skip the \n
			x = margin;
			y += 8;
		}
	}
}


/*
===============
SCR_TouchPics

Allows rendering code to cache all needed sbar graphics
===============
*/
void SCR_TouchPics (void)
{
	if (crosshair->value)
	{
		if (crosshair->value > 3 || crosshair->value < 0)
			crosshair->value = 3;

		Com_sprintf (crosshair_pic, sizeof (crosshair_pic), "ch%i", (int) (crosshair->value));
		re.DrawGetPicSize (&crosshair_width, &crosshair_height, crosshair_pic);

		if (!crosshair_width)
			crosshair_pic[0] = 0;
	}
}

/*
================
SCR_ExecuteLayoutString

================
*/
void SCR_ExecuteLayoutString (char *s)
{
	int		x, y;
	int		value;
	char	*token;
	int		width;
	int		index;
	clientinfo_t	*ci;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if (!s[0])
		return;

	x = 0;
	y = 0;
	width = 3;

	while (s)
	{
		token = COM_Parse (&s);
		if (!strcmp (token, "xl"))
		{
			token = COM_Parse (&s);
			x = atoi (token);
			continue;
		}
		if (!strcmp (token, "xr"))
		{
			token = COM_Parse (&s);
			x = viddef.conwidth + atoi (token);
			continue;
		}
		if (!strcmp (token, "xv"))
		{
			token = COM_Parse (&s);
			x = viddef.conwidth / 2 - 160 + atoi (token);
			continue;
		}

		if (!strcmp (token, "yt"))
		{
			token = COM_Parse (&s);
			y = atoi (token);
			continue;
		}
		if (!strcmp (token, "yb"))
		{
			token = COM_Parse (&s);
			y = viddef.conheight + atoi (token);
			continue;
		}
		if (!strcmp (token, "yv"))
		{
			token = COM_Parse (&s);
			y = viddef.conheight / 2 - 120 + atoi (token);
			continue;
		}

		if (!strcmp (token, "pic"))
		{
			int statnum;

			// draw a pic from a stat number
			token = COM_Parse (&s);
			statnum = atoi (token);
			value = cl.frame.playerstate.stats[statnum];

			if (value >= MAX_IMAGES)
				Com_Error (ERR_DROP, "Pic >= MAX_IMAGES");

			if (cl.configstrings[CS_IMAGES + value])
				re.DrawPic (x, y, cl.configstrings[CS_IMAGES + value]);

			if (statnum == STAT_SELECTED_ICON)
			{
				if (cl.lastitem == -1)
				{
					// wipe on entry
					cl.itemtime = cl.time;
					cl.lastitem = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];
				}

				if (cl.frame.playerstate.stats[STAT_SELECTED_ITEM] != cl.lastitem)
				{
					// item changed
					cl.itemtime = cl.time + 1500;
					cl.lastitem = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];
				}

				// display
				if (cl.time < cl.itemtime)
				{
					// this is a bit hacky cos a mod's custom layout could stomp it
					DrawString (x + 32, y + 8, cl.configstrings[CS_ITEMS + cl.frame.playerstate.stats[STAT_SELECTED_ITEM]]);
				}
			}

			continue;
		}

		if (!strcmp (token, "client"))
		{
			// draw a deathmatch client block
			int		score, ping, time;

			token = COM_Parse (&s);
			x = viddef.conwidth / 2 - 160 + atoi (token);
			token = COM_Parse (&s);
			y = viddef.conheight / 2 - 120 + atoi (token);

			token = COM_Parse (&s);
			value = atoi (token);

			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");

			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi (token);

			token = COM_Parse (&s);
			ping = atoi (token);

			token = COM_Parse (&s);
			time = atoi (token);

			DrawAltString (x + 32, y, ci->name);
			DrawString (x + 32, y + 8, "Score: ");
			DrawAltString (x + 32 + 7 * 8, y + 8, va ("%i", score));
			DrawString (x + 32, y + 16, va ("Ping:  %i", ping));
			DrawString (x + 32, y + 24, va ("Time:  %i", time));

			if (!ci->icon)
				ci = &cl.baseclientinfo;
			re.DrawPic (x, y, ci->iconname);
			continue;
		}

		if (!strcmp (token, "ctf"))
		{
			// draw a ctf client block
			int		score, ping;
			char	block[80];

			token = COM_Parse (&s);
			x = viddef.conwidth / 2 - 160 + atoi (token);
			token = COM_Parse (&s);
			y = viddef.conheight / 2 - 120 + atoi (token);

			token = COM_Parse (&s);
			value = atoi (token);

			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");

			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi (token);

			token = COM_Parse (&s);
			ping = atoi (token);

			if (ping > 999)
				ping = 999;

			sprintf (block, "%3d %3d %-12.12s", score, ping, ci->name);

			if (value == cl.playernum)
				DrawAltString (x, y, block);
			else
				DrawString (x, y, block);
			continue;
		}

		if (!strcmp (token, "picn"))
		{
			// draw a pic from a name
			token = COM_Parse (&s);
			re.DrawPic (x, y, token);
			continue;
		}

		if (!strcmp (token, "num"))
		{
			// draw a number
			token = COM_Parse (&s);
			width = atoi (token);
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi (token)];
			re.DrawField (x, y, 0, width, value);
			continue;
		}

		if (!strcmp (token, "hnum"))
		{
			// health number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];
			if (value > 25)
				color = 0;	// green
			else if (value > 0)
				color = (cl.frame.serverframe >> 2) & 1;		// flash
			else
				color = 1;

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 1)
				re.DrawPic (x, y, "field_3");

			re.DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp (token, "anum"))
		{
			// ammo number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];
			if (value > 5)
				color = 0;	// green
			else if (value >= 0)
				color = (cl.frame.serverframe >> 2) & 1;		// flash
			else
				continue;	// negative number = don't show

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 4)
				re.DrawPic (x, y, "field_3");

			re.DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp (token, "rnum"))
		{
			// armor number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];
			if (value < 1)
				continue;

			color = 0;	// green

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 2)
				re.DrawPic (x, y, "field_3");

			re.DrawField (x, y, color, width, value);
			continue;
		}


		if (!strcmp (token, "stat_string"))
		{
			token = COM_Parse (&s);
			index = atoi (token);
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");
			index = cl.frame.playerstate.stats[index];
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");
			DrawString (x, y, cl.configstrings[index]);
			continue;
		}

		if (!strcmp (token, "cstring"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320, 0);
			continue;
		}

		if (!strcmp (token, "string"))
		{
			token = COM_Parse (&s);
			DrawString (x, y, token);
			continue;
		}

		if (!strcmp (token, "cstring2"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320, 0x80);
			continue;
		}

		if (!strcmp (token, "string2"))
		{
			token = COM_Parse (&s);
			DrawAltString (x, y, token);
			continue;
		}

		if (!strcmp (token, "if"))
		{
			// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi (token)];
			if (!value)
			{
				// skip to endif
				while (s && strcmp (token, "endif"))
				{
					token = COM_Parse (&s);
				}
			}

			continue;
		}
	}
}


/*
================
SCR_DrawStats

The status bar is a small layout program that
is based on the stats array
================
*/
void SCR_DrawStats (void)
{
	SCR_ExecuteLayoutString (cl.configstrings[CS_STATUSBAR]);
}


/*
================
SCR_DrawLayout

================
*/
#define	STAT_LAYOUTS		13

void SCR_DrawLayout (void)
{
	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;
	SCR_ExecuteLayoutString (cl.layout);
}

//=======================================================

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void CL_DrawFPS (void);

void SCR_UpdateScreen (void)
{
	// if the screen is disabled (loading plaque is up, or vid mode changing) do nothing at all
	if (cls.disable_screen)
	{
		if (Sys_Milliseconds () - cls.disable_screen > 120000)
		{
			cls.disable_screen = 0;
			Com_Printf ("Loading plaque timed out.\n");
		}
		return;
	}

	if (!scr_initialized || !con.initialized)
		return;				// not initialized yet

	re.BeginFrame (&viddef);

	if (scr_draw_loading == 2)
	{
		//  loading plaque over black screen
		int		w, h;

		re.CinematicSetPalette (NULL);
		scr_draw_loading = false;

		re.Set2D ();
		re.DrawFill (0, 0, viddef.conwidth, viddef.conheight, 0); // this was never done
		re.DrawGetPicSize (&w, &h, "loading");
		re.DrawPic ((viddef.conwidth - w) / 2, (viddef.conheight - h) / 2, "loading");
		re.End2D ();
	}
	else if (cl.cinematictime > 0)
	{
		re.Set2D ();

		// if a cinematic is supposed to be running, handle menus
		// and console specially
		if (cls.key_dest == key_menu)
		{
			if (cl.cinematicpalette_active)
			{
				re.CinematicSetPalette (NULL);
				cl.cinematicpalette_active = false;
			}

			M_Draw ();
		}
		else if (cls.key_dest == key_console)
		{
			if (cl.cinematicpalette_active)
			{
				re.CinematicSetPalette (NULL);
				cl.cinematicpalette_active = false;
			}

			SCR_DrawConsole ();
		}
		else
		{
			SCR_DrawCinematic ();
		}

		re.End2D ();
	}
	else
	{
		// make sure the game palette is active
		if (cl.cinematicpalette_active)
		{
			re.CinematicSetPalette (NULL);
			cl.cinematicpalette_active = false;
		}

		// do 3D refresh drawing, and then update the screen
		V_RenderView ();

		re.Set2D ();

		CL_DrawFPS ();

		SCR_DrawStats ();

		if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1) SCR_DrawLayout ();
		if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2) CL_DrawInventory ();

		SCR_DrawNet ();
		SCR_CheckDrawCenterString ();

		if (scr_timegraph->value)
			SCR_DebugGraph (cls.frametime * 300, 0);

		if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
			SCR_DrawDebugGraph ();

		SCR_DrawPause ();

		SCR_DrawConsole ();

		M_Draw ();

		SCR_DrawLoading ();

		re.End2D ();
	}

	// never vsync if we're in a timedemo
	if (cl_timedemo->value)
		re.EndFrame (false);
	else re.EndFrame (true);
}

