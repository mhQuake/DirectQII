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
=======================================================================

KEYS MENU

=======================================================================
*/
char *bindnames[][2] =
{
	{"+attack", "attack"},
	{"weapnext", "next weapon"},
	{"weapprev", "previous weapon"},
	{"+forward", "walk forward"},
	{"+back", "backpedal"},
	{"+left", "turn left"},
	{"+right", "turn right"},
	{"+speed", "run"},
	{"+moveleft", "step left"},
	{"+moveright", "step right"},
	{"+strafe", "sidestep"},
	{"+moveup", "up / jump"},
	{"+movedown", "down / crouch"},

	{"inven", "inventory"},
	{"invuse", "use item"},
	{"invdrop", "drop item"},
	{"invprev", "prev item"},
	{"invnext", "next item"},

	{"cmd help", "help computer"},
	{0, 0}
};


static menuaction_s s_keys_item[sizeof (bindnames) / sizeof (bindnames[0])];

int	keys_cursor;
static menuframework_s	s_keys_menu;

static void M_UnbindCommand (char *command)
{
	int	j;
	int l = strlen (command);

	for (j = 0; j < 256; j++)
	{
		char *b = keybindings[j];

		if (!b)
			continue;
		if (!strncmp (b, command, l))
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen (command);
	count = 0;

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l))
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc (menuframework_s *menu)
{
	if (cls.bind_grab)
		re.DrawChar (menu->x, menu->y + menu->cursor * 10, '=');
	else
		re.DrawChar (menu->x, menu->y + menu->cursor * 10, 12 + ((int) (Sys_Milliseconds () / 250) & 1));

	re.DrawString ();
}

static void DrawKeyBindingFunc (void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *) self;

	M_FindKeysForCommand (bindnames[a->generic.localdata[0]][0], keys);

	if (keys[0] == -1)
	{
		Menu_DrawString (a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, "???");
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString (keys[0]);

		Menu_DrawString (a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, name);

		x = strlen (name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawStringDark (a->generic.x + a->generic.parent->x + 24 + x, a->generic.y + a->generic.parent->y, "or");
			Menu_DrawString (a->generic.x + a->generic.parent->x + 48 + x, a->generic.y + a->generic.parent->y, Key_KeynumToString (keys[1]));
		}
	}
}

static void KeyBindingFunc (void *self)
{
	menuaction_s *a = (menuaction_s *) self;
	int keys[2];

	M_FindKeysForCommand (bindnames[a->generic.localdata[0]][0], keys);

	if (keys[1] != -1)
		M_UnbindCommand (bindnames[a->generic.localdata[0]][0]);

	cls.bind_grab = true;

	Menu_SetStatusBar (&s_keys_menu, "press a key or button for this action");
}


static void Keys_MenuInit (void)
{
	int y = 0;
	int i = 0;

	s_keys_menu.x = viddef.conwidth * 0.50;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

	for (i = 0; ; i++, y += 10)
	{
		// no more
		if (!bindnames[i][0]) break;
		if (!bindnames[i][1]) break;

		s_keys_item[i].generic.type = MTYPE_ACTION;
		s_keys_item[i].generic.flags = QMF_GRAYED;
		s_keys_item[i].generic.x = 0;
		s_keys_item[i].generic.y = y;
		s_keys_item[i].generic.ownerdraw = DrawKeyBindingFunc;
		s_keys_item[i].generic.localdata[0] = i;
		s_keys_item[i].generic.name = bindnames[i][1];

		Menu_AddItem (&s_keys_menu, (void *) &s_keys_item[i]);
	}

	Menu_SetStatusBar (&s_keys_menu, "enter to change, backspace to clear");
	Menu_Center (&s_keys_menu);
}

static void Keys_MenuDraw (void)
{
	Menu_AdjustCursor (&s_keys_menu, 1);
	Menu_Draw (&s_keys_menu);
}

static const char *Keys_MenuKey (int key)
{
	menuaction_s *item = (menuaction_s *) Menu_ItemAtCursor (&s_keys_menu);

	if (cls.bind_grab)
	{
		if (key != K_ESCAPE && key != '`')
		{
			char cmd[1024];

			Com_sprintf (cmd, sizeof (cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString (key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
		}

		Menu_SetStatusBar (&s_keys_menu, "enter to change, backspace to clear");
		cls.bind_grab = false;
		return menu_out_sound;
	}

	switch (key)
	{
	case K_KP_ENTER:
	case K_ENTER:
		KeyBindingFunc (item);
		return menu_in_sound;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
	case K_KP_DEL:
		M_UnbindCommand (bindnames[item->generic.localdata[0]][0]);
		return menu_out_sound;

	default:
		return Default_MenuKey (&s_keys_menu, key);
	}
}

void M_Menu_Keys_f (void)
{
	Keys_MenuInit ();
	M_PushMenu (Keys_MenuDraw, Keys_MenuKey);
}


