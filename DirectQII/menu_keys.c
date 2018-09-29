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
=======================================================================

KEYS MENU

=======================================================================
*/
typedef struct keybind_s {
	char *command;
	char *bindname;
	qboolean separator_after;
} keybind_t;

keybind_t key_weapon_binds[] = {
	{"+attack", "attack", false},
	{"weapnext", "next weapon", false},
	{"weapprev", "prev weapon", true},
	{NULL, NULL}
};

keybind_t key_move_binds[] = {
	{"+forward", "walk forward", false},
	{"+back", "backpedal", false},
	{"+left", "turn left", false},
	{"+right", "turn right", false},
	{"+speed", "run", false},
	{"+moveleft", "step left", false},
	{"+moveright", "step right", true},
	{"+moveup", "up / jump", false},
	{"+movedown", "down / crouch", true},
	{NULL, NULL}
};

keybind_t key_misc_binds[] = {
	{"inven", "inventory", false},
	{"invuse", "use item", false},
	{"invdrop", "drop item", false},
	{"invprev", "prev item", false},
	{"invnext", "next item", true},
	{"cmd help", "help computer", false},
	{"echo Quick Saving...; wait; save quick", "quick save", false},
	{"echo Quick Loading...; wait; load quick", "quick load", false},
	{"screenshot", "screenshot", false},
	{NULL, NULL}
};


static menuframework_s s_keys_weapon_menu;
static menuaction_s s_keys_weapon_item[sizeof (key_weapon_binds) / sizeof (key_weapon_binds[0])];

static menuframework_s s_keys_move_menu;
static menuaction_s s_keys_move_item[sizeof (key_move_binds) / sizeof (key_move_binds[0])];

static menuframework_s s_keys_misc_menu;
static menuaction_s s_keys_misc_item[sizeof (key_misc_binds) / sizeof (key_misc_binds[0])];

static menuframework_s *s_current_keys_menu = &s_keys_weapon_menu;
static menuaction_s	*s_current_keys_item = s_keys_weapon_item;
static keybind_t *s_current_key_binds = key_weapon_binds;

static menulist_s		s_key_mode_list;


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
	menucommon_s *item = (menucommon_s *) Menu_ItemAtCursor (menu);

	if (menu->cursor == 0)
		re.DrawChar (menu->x, menu->y + item->y, 12 + ((int) (Sys_Milliseconds () / 250) & 1));
	else if (cls.bind_grab)
		re.DrawChar (menu->x, menu->y + item->y, '=');
	else
		re.DrawChar (menu->x, menu->y + item->y, 12 + ((int) (Sys_Milliseconds () / 250) & 1));

	re.DrawString ();
}


static void DrawKeyBindingFunc (void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *) self;

	M_FindKeysForCommand (s_current_key_binds[a->generic.localdata[0]].command, keys);

	if (keys[0] == -1)
		Menu_DrawString (a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, "???");
	else
	{
		const char *name = Key_KeynumToString (keys[0]);
		int x = strlen (name) * 8;

		Menu_DrawString (a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, name);

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

	M_FindKeysForCommand (s_current_key_binds[a->generic.localdata[0]].command, keys);

	if (keys[1] != -1)
		M_UnbindCommand (s_current_key_binds[a->generic.localdata[0]].command);

	cls.bind_grab = true;
}


static void Keys_MenuInitMenu (menuframework_s *menu, keybind_t *bindings, menuaction_s *items)
{
	int y = 0;
	int i = 0;

	menu->x = viddef.conwidth / 2;
	menu->y = viddef.conheight / 2 - 58;
	menu->nitems = 0;
	menu->cursordraw = KeyCursorDrawFunc;
	menu->saveCfgOnExit = true;

	Menu_AddItem (menu, (void *) &s_key_mode_list);
	y += 20;

	for (i = 0; ; i++)
	{
		// no more
		if (!bindings[i].command) break;
		if (!bindings[i].bindname) break;

		items[i].generic.type = MTYPE_ACTION;
		items[i].generic.flags = QMF_GRAYED;
		items[i].generic.x = 0;
		items[i].generic.y = y;
		items[i].generic.ownerdraw = DrawKeyBindingFunc;
		items[i].generic.localdata[0] = i;
		items[i].generic.name = bindings[i].bindname;

		Menu_AddItem (menu, (void *) &items[i]);

		if (bindings[i].separator_after)
			y += 20;
		else y += 10;
	}
}


static void Keys_MenuInit (void)
{
	static const char *keymode_names[] =
	{
		"weapons",
		"movement",
		"misc",
		0
	};

	// set up the key mode option which will be common to all
	s_key_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_key_mode_list.generic.name = "mode";
	s_key_mode_list.generic.x = 0;
	s_key_mode_list.generic.y = 0;
	s_key_mode_list.itemnames = keymode_names;

	Keys_MenuInitMenu (&s_keys_weapon_menu, key_weapon_binds, s_keys_weapon_item);
	Keys_MenuInitMenu (&s_keys_move_menu, key_move_binds, s_keys_move_item);
	Keys_MenuInitMenu (&s_keys_misc_menu, key_misc_binds, s_keys_misc_item);
}

static void Keys_MenuDraw (void)
{
	switch (s_key_mode_list.curvalue)
	{
	case 0:
	default:
		s_current_keys_menu = &s_keys_weapon_menu;
		s_current_key_binds = key_weapon_binds;
		s_current_keys_item = s_keys_weapon_item;
		break;

	case 1:
		s_current_keys_menu = &s_keys_move_menu;
		s_current_key_binds = key_move_binds;
		s_current_keys_item = s_keys_move_item;
		break;

	case 2:
		s_current_keys_menu = &s_keys_misc_menu;
		s_current_key_binds = key_misc_binds;
		s_current_keys_item = s_keys_misc_item;
		break;
	}

	// set the correct status bar
	/*
	if (s_current_keys_menu->cursor == 0)
		Menu_SetStatusBar (s_current_keys_menu, NULL);
	else if (cls.bind_grab)
		Menu_SetStatusBar (s_current_keys_menu, "press a key or button for this action");
	else Menu_SetStatusBar (s_current_keys_menu, "enter to change, backspace to clear");
	*/
	Menu_SetStatusBar (s_current_keys_menu, va ("cursor at %i", s_current_keys_menu->cursor));

	M_Banner ("m_banner_customize");
	Menu_AdjustCursor (s_current_keys_menu, 1);
	Menu_Draw (s_current_keys_menu);
}

static const char *Keys_MenuKey (int key)
{
	if (s_current_keys_menu->cursor == 0)
	{
		// the mode selection spin control just goes through the standard key func
		return Default_MenuKey (s_current_keys_menu, key);
	}
	else
	{
		// otherwise we're on a key-binding option
		menuaction_s *item = (menuaction_s *) Menu_ItemAtCursor (s_current_keys_menu);

		if (cls.bind_grab)
		{
			if (key != K_ESCAPE && key != '`')
				Cbuf_InsertText (va ("bind \"%s\" \"%s\"\n", Key_KeynumToString (key), s_current_key_binds[item->generic.localdata[0]].command));

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
			M_UnbindCommand (s_current_key_binds[item->generic.localdata[0]].command);
			return menu_out_sound;

		default:
			return Default_MenuKey (s_current_keys_menu, key);
		}
	}
}


void M_Menu_Keys_f (void)
{
	Keys_MenuInit ();
	M_PushMenu (Keys_MenuDraw, Keys_MenuKey);
}


