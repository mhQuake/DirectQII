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
// cl_inv.c -- client inventory screen

#include "client.h"

/*
================
CL_ParseInventory
================
*/
void CL_ParseInventory (void)
{
	int		i;

	for (i = 0; i < MAX_ITEMS; i++)
		cl.inventory[i] = MSG_ReadShort (&net_message);
}


void Inv_DrawString (int x, int y, char *string)
{
	while (*string)
	{
		re.DrawChar (x, y, *string);
		x += 8;
		string++;
	}

	re.DrawString ();
}


void Inv_DrawAltString (int x, int y, char *string)
{
	while (*string)
	{
		re.DrawChar (x, y, *string + 128);
		x += 8;
		string++;
	}

	re.DrawString ();
}


void SetStringHighBit (char *s)
{
	while (*s)
	{
		*s = ((*s) + 128) & 255;
		s++;
	}
}

/*
================
CL_DrawInventory
================
*/
#define	DISPLAY_ITEMS	17

void CL_DrawInventory (void)
{
	int		i, j;
	int		num, selected_num, item;
	int		index[MAX_ITEMS];
	char	string[1024];
	int		x, y;
	char	binding[1024];
	char	*bind;
	int		selected;
	int		top;

	selected = cl.frame.playerstate.stats[STAT_SELECTED_ITEM];

	num = 0;
	selected_num = 0;
	for (i = 0; i < MAX_ITEMS; i++)
	{
		if (i == selected)
			selected_num = num;
		if (cl.inventory[i])
		{
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS / 2;
	if (num - top < DISPLAY_ITEMS)
		top = num - DISPLAY_ITEMS;
	if (top < 0)
		top = 0;

	x = (viddef.conwidth - 256) / 2;
	y = (viddef.conheight - 240) / 2;

	re.DrawPic (x, y + 8, "inventory");

	y += 24;
	x += 24;
	Inv_DrawString (x, y, "hotkey ### item");
	Inv_DrawString (x, y + 8, "------ --- ----");
	y += 16;

	for (i = top; i < num && i < top + DISPLAY_ITEMS; i++)
	{
		item = index[i];

		// search for a binding
		Com_sprintf (binding, sizeof (binding), "use %s", cl.configstrings[CS_ITEMS + item]);
		bind = "";

		for (j = 0; j < 256; j++)
		{
			if (keybindings[j] && !Q_stricmp (keybindings[j], binding))
			{
				bind = Key_KeynumToString (j);
				break;
			}
		}

		Com_sprintf (string, sizeof (string), "%6s %3i %s", bind, cl.inventory[item], cl.configstrings[CS_ITEMS + item]);

		if (item == selected)
		{
			// draw a blinky cursor by the selected item
			// the original code was written for a float second timer; adjust to int ms and time it for consistency with the input cursor
			// the upcoming Inv_DrawString will complete this
			if (Com_CursorTime ())
				re.DrawChar (x - 8, y, 15);

			Inv_DrawString (x, y, string);
		}
		else Inv_DrawAltString (x, y, string);

		y += 8;
	}
}


void DrawHUDString (char *string, int x, int y, int centerwidth, int mask)
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
		else x = margin;

		for (i = 0; i < width; i++)
		{
			re.DrawChar (x, y, line[i] | mask);
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
			char *statpic = NULL;
			int statnum;

			// draw a pic from a stat number
			token = COM_Parse (&s);
			statnum = atoi (token);
			value = cl.frame.playerstate.stats[statnum];

			// bounds-check value
			if (value < 0) Com_Error (ERR_DROP, "Pic < 0");
			if (value >= MAX_IMAGES) Com_Error (ERR_DROP, "Pic >= MAX_IMAGES");

			// this is to show the face icon for the selected skin in the sbar instead of the regular health icon
			// according to an old interview with Paul Steed, this was the original intention but the artists made
			// these icons at the wrong size so they used a health icon instead, then decided to anonymize the
			// player.  i'm just putting it back the way Steed wanted it.
			if (statnum == STAT_HEALTH_ICON)
			{
				extern cvar_t *skin;
				int w = 24;
				int h = 24;

				// check first to see if the pic exists
				char *facepic = va ("/players/%s_i.pcx", skin->string);

				if (re.DrawGetPicSize (&w, &h, facepic))
				{
					// draw it at the correct scaled size for the HUD health pic (which may not be 24 x 24)
					statpic = cl.configstrings[CS_IMAGES + value];

					if (statpic && re.DrawGetPicSize (&w, &h, statpic))
					{
						re.DrawStretchPic (x, y, w, h, facepic);
						continue;
					}
				}
			}

			if ((statpic = cl.configstrings[CS_IMAGES + value]) != NULL)
				re.DrawPic (x, y, statpic);

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


