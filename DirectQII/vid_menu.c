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

#define REF_SOFT	0
#define REF_OPENGL	1

extern cvar_t *vid_ref;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;
extern cvar_t *scr_viewsize;

static cvar_t *gl_mode;
static cvar_t *gl_finish;

extern void M_ForceMenuOff (void);

/*
====================================================================

MENU INTERACTION

====================================================================
*/
#define WINDOWED_MENU 0
#define FULLSCREEN_MENU   1

static menuframework_s  s_windowed_menu;
static menuframework_s	s_fullscreen_menu;
static menuframework_s *s_current_menu;
static int				s_current_menu_index;

static menulist_s		s_mode_list;
static menuslider_s		s_screensize_slider;
static menuslider_s		s_brightness_slider;
static menulist_s  		s_fs_box;
static menulist_s  		s_finish_box;
static menuaction_s		s_cancel_action;
static menuaction_s		s_defaults_action;


static void ScreenSizeCallback (void *s)
{
	menuslider_s *slider = (menuslider_s *) s;
	Cvar_SetValue ("viewsize", slider->curvalue * 10);
}


static void BrightnessCallback (void *s)
{
	// this now works without needing a vid_restart
	menuslider_s *slider = (menuslider_s *) s;
	float gamma = (0.8 - (slider->curvalue / 10.0 - 0.5)) + 0.5;

	Cvar_SetValue ("vid_gamma", gamma);
}


static void ResetDefaults (void *unused)
{
	VID_MenuInit ();
}


static void ApplyChanges (void *unused)
{
	// keep consistent
	BrightnessCallback (&s_brightness_slider);

	// other cvars
	Cvar_SetValue ("vid_fullscreen", s_fs_box.curvalue);
	Cvar_SetValue ("gl_finish", s_finish_box.curvalue);
	Cvar_SetValue ("gl_mode", s_mode_list.curvalue);

	M_ForceMenuOff ();
}


static void CancelChanges (void *unused)
{
	extern void M_PopMenu (void);

	M_PopMenu ();
}


/*
** VID_MenuInit
*/
void VID_MenuInit (void)
{
	static const char *resolutions[] =
	{
		"[320 240  ]",
		"[400 300  ]",
		"[512 384  ]",
		"[640 480  ]",
		"[800 600  ]",
		"[960 720  ]",
		"[1024 600 ]",
		"[1152 864 ]",
		"[1280 960 ]",
		"[1600 1200]",
		0
	};

	static const char *vid_refs[] =
	{
		"[software      ]",
		"[default OpenGL]",
		0
	};

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	if (!gl_mode)
		gl_mode = Cvar_Get ("gl_mode", "3", 0);
	if (!gl_finish)
		gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);

	s_mode_list.curvalue = gl_mode->value;

	if (!scr_viewsize)
		scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);

	s_screensize_slider.curvalue = scr_viewsize->value / 10;

	if (strcmp (vid_ref->string, "soft") == 0)
	{
		s_current_menu_index = WINDOWED_MENU;
	}
	else if (strcmp (vid_ref->string, "gl") == 0)
	{
		s_current_menu_index = FULLSCREEN_MENU;
	}

	s_windowed_menu.x = viddef.conwidth * 0.50;
	s_windowed_menu.nitems = 0;
	s_fullscreen_menu.x = viddef.conwidth * 0.50;
	s_fullscreen_menu.nitems = 0;

	s_screensize_slider.generic.type = MTYPE_SLIDER;
	s_screensize_slider.generic.x = 0;
	s_screensize_slider.generic.y = 20;
	s_screensize_slider.generic.name = "screen size";
	s_screensize_slider.minvalue = 3;
	s_screensize_slider.maxvalue = 12;
	s_screensize_slider.generic.callback = ScreenSizeCallback;

	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.x = 0;
	s_brightness_slider.generic.y = 30;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 5;
	s_brightness_slider.maxvalue = 13;
	s_brightness_slider.curvalue = (1.3 - vid_gamma->value + 0.5) * 10;

	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to defaults";
	s_defaults_action.generic.x = 0;
	s_defaults_action.generic.y = 90;
	s_defaults_action.generic.callback = ResetDefaults;

	s_cancel_action.generic.type = MTYPE_ACTION;
	s_cancel_action.generic.name = "cancel";
	s_cancel_action.generic.x = 0;
	s_cancel_action.generic.y = 100;
	s_cancel_action.generic.callback = CancelChanges;

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = 10;
	s_mode_list.itemnames = resolutions;

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.x = 0;
	s_fs_box.generic.y = 40;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.itemnames = yesno_names;
	s_fs_box.curvalue = vid_fullscreen->value;

	s_finish_box.generic.type = MTYPE_SPINCONTROL;
	s_finish_box.generic.x = 0;
	s_finish_box.generic.y = 80;
	s_finish_box.generic.name = "sync every frame";
	s_finish_box.curvalue = gl_finish->value;
	s_finish_box.itemnames = yesno_names;

	Menu_AddItem (&s_windowed_menu, (void *) &s_mode_list);
	Menu_AddItem (&s_windowed_menu, (void *) &s_screensize_slider);
	Menu_AddItem (&s_windowed_menu, (void *) &s_brightness_slider);
	Menu_AddItem (&s_windowed_menu, (void *) &s_fs_box);
	Menu_AddItem (&s_windowed_menu, (void *) &s_finish_box);

	Menu_AddItem (&s_fullscreen_menu, (void *) &s_mode_list);
	Menu_AddItem (&s_fullscreen_menu, (void *) &s_screensize_slider);
	Menu_AddItem (&s_fullscreen_menu, (void *) &s_brightness_slider);
	Menu_AddItem (&s_fullscreen_menu, (void *) &s_fs_box);
	Menu_AddItem (&s_fullscreen_menu, (void *) &s_finish_box);

	Menu_AddItem (&s_windowed_menu, (void *) &s_defaults_action);
	Menu_AddItem (&s_windowed_menu, (void *) &s_cancel_action);
	Menu_AddItem (&s_fullscreen_menu, (void *) &s_defaults_action);
	Menu_AddItem (&s_fullscreen_menu, (void *) &s_cancel_action);

	Menu_Center (&s_windowed_menu);
	Menu_Center (&s_fullscreen_menu);

	s_fullscreen_menu.x -= 8;
	s_windowed_menu.x -= 8;
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	int w, h;

	if (s_current_menu_index == 0)
		s_current_menu = &s_windowed_menu;
	else
		s_current_menu = &s_fullscreen_menu;

	// draw the banner
	re.DrawGetPicSize (&w, &h, "m_banner_video");
	re.DrawPic (viddef.conwidth / 2 - w / 2, viddef.conheight / 2 - 110, "m_banner_video");

	// move cursor to a reasonable starting position
	Menu_AdjustCursor (s_current_menu, 1);

	// draw the menu
	Menu_Draw (s_current_menu);
}

/*
================
VID_MenuKey
================
*/
const char *VID_MenuKey (int key)
{
	menuframework_s *m = s_current_menu;
	static const char *sound = "misc/menu1.wav";

	switch (key)
	{
	case K_ESCAPE:
		ApplyChanges (NULL);
		return NULL;
	case K_KP_UPARROW:
	case K_UPARROW:
		m->cursor--;
		Menu_AdjustCursor (m, -1);
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		m->cursor++;
		Menu_AdjustCursor (m, 1);
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		Menu_SlideItem (m, -1);
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		Menu_SlideItem (m, 1);
		break;
	case K_KP_ENTER:
	case K_ENTER:
		if (!Menu_SelectItem (m))
			ApplyChanges (NULL);
		break;
	}

	return sound;
}


void M_Menu_Video_f (void)
{
	VID_MenuInit ();
	M_PushMenu (VID_MenuDraw, VID_MenuKey);
}

