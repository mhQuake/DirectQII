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

void CL_WriteConfiguration (void);

/*
=======================================================================

OPTIONS MENU

=======================================================================
*/
static menuframework_s	s_options_menu;
static menuaction_s		s_options_defaults_action;
static menuaction_s		s_options_writeconfig_action;
static menuaction_s		s_options_customize_options_action;
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;
static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookspring_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_sfxvolume_slider;
static menuslider_s		s_options_cdvolume_slider;
static menulist_s		s_options_quality_list;
static menulist_s		s_options_console_action;

static void CrosshairFunc (void *unused)
{
	Cvar_SetValue ("crosshair", s_options_crosshair_box.curvalue);
}

static void CustomizeControlsFunc (void *unused)
{
	M_Menu_Keys_f ();
}

static void AlwaysRunFunc (void *unused)
{
	Cvar_SetValue ("cl_run", s_options_alwaysrun_box.curvalue);
}

static void FreeLookFunc (void *unused)
{
	Cvar_SetValue ("freelook", s_options_freelook_box.curvalue);
}

static void MouseSpeedFunc (void *unused)
{
	Cvar_SetValue ("sensitivity", s_options_sensitivity_slider.curvalue / 2.0F);
}

static void ControlsSetMenuItemValues (void)
{
	s_options_sfxvolume_slider.curvalue = Cvar_VariableValue ("s_volume") * 10;
	s_options_cdvolume_slider.curvalue = Cvar_VariableValue ("bgmvolume") * 10;
	s_options_quality_list.curvalue = Cvar_VariableValue ("s_khz") < 22;
	s_options_sensitivity_slider.curvalue = (sensitivity->value) * 2;

	Cvar_SetValue ("cl_run", M_ClampCvar (0, 1, cl_run->value));
	s_options_alwaysrun_box.curvalue = cl_run->value;

	s_options_invertmouse_box.curvalue = m_pitch->value < 0;

	Cvar_SetValue ("freelook", M_ClampCvar (0, 1, freelook->value));
	s_options_freelook_box.curvalue = freelook->value;

	Cvar_SetValue ("crosshair", M_ClampCvar (0, 3, crosshair->value));
	s_options_crosshair_box.curvalue = crosshair->value;
}

static void WriteConfigurationFunc (void *unused)
{
	CL_WriteConfiguration ();
}

static void ControlsResetDefaultsFunc (void *unused)
{
	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_Execute ();

	ControlsSetMenuItemValues ();
}

static void InvertMouseFunc (void *unused)
{
	if (s_options_invertmouse_box.curvalue == 0)
	{
		Cvar_SetValue ("m_pitch", fabs (m_pitch->value));
	}
	else
	{
		Cvar_SetValue ("m_pitch", -fabs (m_pitch->value));
	}
}

static void UpdateVolumeFunc (void *unused)
{
	Cvar_SetValue ("s_volume", s_options_sfxvolume_slider.curvalue / 10);
}

static void UpdateCDVolumeFunc (void *unused)
{
	Cvar_SetValue ("bgmvolume", s_options_cdvolume_slider.curvalue / 10);
}

static void ConsoleFunc (void *unused)
{
	// the proper way to do this is probably to have ToggleConsole_f accept a parameter
	extern void Key_ClearTyping (void);

	if (cl.attractloop)
	{
		Cbuf_AddText ("killserver\n");
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	M_ForceMenuOff ();
	cls.key_dest = key_console;
}

static void UpdateSoundQualityFunc (void *unused)
{
	if (s_options_quality_list.curvalue)
		Cvar_SetValue ("s_khz", 22);
	else Cvar_SetValue ("s_khz", 11);

	M_DrawTextBox (8, 120 - 48, 36, 3);
	M_Print (16 + 16, 120 - 48 + 8,  "Restarting the sound system. This");
	M_Print (16 + 16, 120 - 48 + 16, "could take up to a minute, so");
	M_Print (16 + 16, 120 - 48 + 24, "please be patient.");

	// the text box won't show up unless we do a buffer swap
	re.EndFrame (true);

	CL_Snd_Restart_f ();
}

void Options_MenuInit (void)
{
	static const char *cd_music_items[] =
	{
		"disabled",
		"enabled",
		0
	};
	static const char *quality_items[] =
	{
		"low", "high", 0
	};

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *crosshair_names[] =
	{
		"none",
		"cross",
		"dot",
		"angle",
		0
	};

	// configure controls menu and menu items
	s_options_menu.x = viddef.conwidth / 2;
	s_options_menu.y = viddef.conheight / 2 - 58;
	s_options_menu.nitems = 0;
	s_options_menu.saveCfgOnExit = true;

	s_options_sfxvolume_slider.generic.type = MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x = 0;
	s_options_sfxvolume_slider.generic.y = 0;
	s_options_sfxvolume_slider.generic.name = "effects volume";
	s_options_sfxvolume_slider.generic.callback = UpdateVolumeFunc;
	s_options_sfxvolume_slider.minvalue = 0;
	s_options_sfxvolume_slider.maxvalue = 10;
	s_options_sfxvolume_slider.curvalue = Cvar_VariableValue ("s_volume") * 10;

	s_options_cdvolume_slider.generic.type = MTYPE_SLIDER;
	s_options_cdvolume_slider.generic.x = 0;
	s_options_cdvolume_slider.generic.y = 10;
	s_options_cdvolume_slider.generic.name = "music volume";
	s_options_cdvolume_slider.generic.callback = UpdateCDVolumeFunc;
	s_options_cdvolume_slider.minvalue = 0;
	s_options_cdvolume_slider.maxvalue = 10;
	s_options_cdvolume_slider.curvalue = Cvar_VariableValue ("bgmvolume") * 10;

	s_options_quality_list.generic.type = MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x = 0;
	s_options_quality_list.generic.y = 20;
	s_options_quality_list.generic.name = "sound quality";
	s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
	s_options_quality_list.itemnames = quality_items;
	s_options_quality_list.curvalue = Cvar_VariableValue ("s_khz") < 22;

	s_options_sensitivity_slider.generic.type = MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x = 0;
	s_options_sensitivity_slider.generic.y = 40;
	s_options_sensitivity_slider.generic.name = "mouse speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue = 2;
	s_options_sensitivity_slider.maxvalue = 22;

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x = 0;
	s_options_alwaysrun_box.generic.y = 50;
	s_options_alwaysrun_box.generic.name = "always run";
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x = 0;
	s_options_invertmouse_box.generic.y = 60;
	s_options_invertmouse_box.generic.name = "invert mouse";
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x = 0;
	s_options_freelook_box.generic.y = 70;
	s_options_freelook_box.generic.name = "free look";
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x = 0;
	s_options_crosshair_box.generic.y = 90;
	s_options_crosshair_box.generic.name = "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

	s_options_customize_options_action.generic.type = MTYPE_ACTION;
	s_options_customize_options_action.generic.x = 0;
	s_options_customize_options_action.generic.y = 110;
	s_options_customize_options_action.generic.name = "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_writeconfig_action.generic.type = MTYPE_ACTION;
	s_options_writeconfig_action.generic.x = 0;
	s_options_writeconfig_action.generic.y = 120;
	s_options_writeconfig_action.generic.name = "save configuration";
	s_options_writeconfig_action.generic.callback = WriteConfigurationFunc;

	s_options_defaults_action.generic.type = MTYPE_ACTION;
	s_options_defaults_action.generic.x = 0;
	s_options_defaults_action.generic.y = 130;
	s_options_defaults_action.generic.name = "reset defaults";
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	s_options_console_action.generic.type = MTYPE_ACTION;
	s_options_console_action.generic.x = 0;
	s_options_console_action.generic.y = 140;
	s_options_console_action.generic.name = "go to console";
	s_options_console_action.generic.callback = ConsoleFunc;

	ControlsSetMenuItemValues ();

	Menu_AddItem (&s_options_menu, (void *) &s_options_sfxvolume_slider);
	Menu_AddItem (&s_options_menu, (void *) &s_options_cdvolume_slider);
	Menu_AddItem (&s_options_menu, (void *) &s_options_quality_list);

	Menu_AddItem (&s_options_menu, (void *) &s_options_sensitivity_slider);
	Menu_AddItem (&s_options_menu, (void *) &s_options_alwaysrun_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_invertmouse_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_freelook_box);
	Menu_AddItem (&s_options_menu, (void *) &s_options_crosshair_box);

	Menu_AddItem (&s_options_menu, (void *) &s_options_customize_options_action);
	Menu_AddItem (&s_options_menu, (void *) &s_options_writeconfig_action);
	Menu_AddItem (&s_options_menu, (void *) &s_options_defaults_action);
	Menu_AddItem (&s_options_menu, (void *) &s_options_console_action);
}

void Options_MenuDraw (void)
{
	M_Banner ("m_banner_options");
	Menu_AdjustCursor (&s_options_menu, 1);
	Menu_Draw (&s_options_menu);
}

const char *Options_MenuKey (int key)
{
	return Default_MenuKey (&s_options_menu, key);
}

void M_Menu_Options_f (void)
{
	Options_MenuInit ();
	M_PushMenu (Options_MenuDraw, Options_MenuKey);
}


