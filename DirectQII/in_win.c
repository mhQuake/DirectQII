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
// in_win.c -- windows 95 mouse and joystick code

#include "client.h"
#include "winquake.h"

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>

#pragma comment (lib, "dinput8.lib")

// directinput objects
IDirectInput8 *di_Object = NULL;
IDirectInputDevice8 *di_Mouse = NULL;


extern	unsigned	sys_msg_time;

qboolean	in_appactive;


/*
============================================================

MOUSE CONTROL

============================================================
*/

// mouse variables
cvar_t		*in_mouse;

qboolean	mlooking;
int			mouse_oldbuttonstate;
qboolean	mouseinitialized;


void IN_MLookDown (void) { mlooking = true; }

void IN_MLookUp (void)
{
	mlooking = false;
	if (!freelook->value && lookspring->value)
		IN_CenterView ();
}


/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse (void)
{
	if (di_Mouse)
	{
		di_Mouse->lpVtbl->Release (di_Mouse);
		di_Mouse = NULL;
	}

	if (di_Object)
	{
		di_Object->lpVtbl->Release (di_Object);
		di_Object = NULL;
	}

	// give the mouse back to the OS
	ClipCursor (NULL);
	ReleaseCapture ();
}


void IN_CenterCursor (void)
{
	RECT wr;
	int cx, cy;

	GetWindowRect (cl_hwnd, &wr);

	cx = (wr.right + wr.left) / 2;
	cy = (wr.top + wr.bottom) / 2;

	SetCapture (cl_hwnd);
	ClipCursor (&wr);
	SetCursorPos (cx, cy);
}


/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse (void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse->value)
	{
		IN_DeactivateMouse ();
		return;
	}

	// don't activate if it was already activated
	if (di_Mouse && di_Object) return;

	// if it was partially activated complete the destruction (this should never happen)
	IN_DeactivateMouse ();

	// center the cursor in the window so that we don't accidentally click outside
	IN_CenterCursor ();

	// everything must succeed
	if (SUCCEEDED (DirectInput8Create (GetModuleHandle (NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8, (void **) &di_Object, NULL)))
		if (SUCCEEDED (di_Object->lpVtbl->CreateDevice (di_Object, &GUID_SysMouse, &di_Mouse, NULL)))
			if (SUCCEEDED (di_Mouse->lpVtbl->SetDataFormat (di_Mouse, &c_dfDIMouse2)))
				if (SUCCEEDED (di_Mouse->lpVtbl->SetCooperativeLevel (di_Mouse, cl_hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE)))
					if (SUCCEEDED (di_Mouse->lpVtbl->Acquire (di_Mouse)))
						return;

	// something failed so deactivate it
	IN_DeactivateMouse ();
}


/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse (void)
{
	cvar_t *cv = Cvar_Get ("in_initmouse", "1", CVAR_NOSET);

	if (!cv->value)
		return;

	mouseinitialized = true;
}


/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent (int mstate)
{
	int		i;

	// perform button actions
	for (i = 0; i < 8; i++)
	{
		if ((mstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i))) Key_Event (K_MOUSE1 + i, true, sys_msg_time);
		if (!(mstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i))) Key_Event (K_MOUSE1 + i, false, sys_msg_time);
	}

	mouse_oldbuttonstate = mstate;
}


void IN_MouseWheelEvent (signed int dir)
{
	// explicit cast to signed int so that callers can know that it needs a signed datatype
	if (dir > 0)
	{
		Key_Event (K_MWHEELUP, true, sys_msg_time);
		Key_Event (K_MWHEELUP, false, sys_msg_time);
	}
	else if (dir < 0)
	{
		Key_Event (K_MWHEELDOWN, true, sys_msg_time);
		Key_Event (K_MWHEELDOWN, false, sys_msg_time);
	}
}


qboolean IN_ReadDirectInput (DIMOUSESTATE2 *di_State)
{
	memset (di_State, 0, sizeof (DIMOUSESTATE2));

	switch (di_Mouse->lpVtbl->GetDeviceState (di_Mouse, sizeof (DIMOUSESTATE2), di_State))
	{
	case DIERR_INPUTLOST:
	case DIERR_NOTACQUIRED:
		// device was lost; reacquire it
		di_Mouse->lpVtbl->Acquire (di_Mouse);
		return false;

	case DI_OK:
		// we can read now
		break;

	default:
		// something else went wrong; deactivate the mouse and it will activate again on the next frame
		IN_DeactivateMouse ();
		return false;
	}

	// the mouse is good for reading now
	return true;
}


/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove (usercmd_t *cmd)
{
	int i;
	float mouse_x, mouse_y;
	DIMOUSESTATE2 di_State;
	int di_ButtonState = 0;

	if (!di_Object) return;
	if (!di_Mouse) return;

	// read
	if (!IN_ReadDirectInput (&di_State)) return;

	// run buttons
	for (i = 0; i < 8; i++)
		if (di_State.rgbButtons[i] & 0x80)
			di_ButtonState |= (1 << i);

	// run buttons
	IN_MouseEvent (di_ButtonState);

	// run wheel
	IN_MouseWheelEvent ((signed int) di_State.lZ);

	// run movement
	// dinput has lower sensitivity and no ballistics so we need to boost it a little
	mouse_x = (float) di_State.lX * sensitivity->value * 1.75;
	mouse_y = (float) di_State.lY * sensitivity->value * 1.75;

	// add mouse X/Y movement to cmd
	if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
		cmd->sidemove += m_side->value * mouse_x;
	else cl.viewangles[1] -= m_yaw->value * mouse_x;

	if ((mlooking || freelook->value) && !(in_strafe.state & 1))
		cl.viewangles[0] += m_pitch->value * mouse_y;
	else cmd->forwardmove -= m_forward->value * mouse_y;
}


/*
=========================================================================

VIEW CENTERING

=========================================================================
*/

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;


/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	// mouse variables
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);

	// centering
	v_centermove = Cvar_Get ("v_centermove", "0.15", 0);
	v_centerspeed = Cvar_Get ("v_centerspeed", "500", 0);

	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	IN_StartupMouse ();
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse ();
}


/*
===========
IN_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void IN_Activate (qboolean active)
{
	IN_DeactivateMouse (); // the mouse will activate on the next frame if appropriate
	in_appactive = active;
}


/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame (void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse || !in_appactive)
	{
		IN_DeactivateMouse ();
		return;
	}

	// if grabbing keys for bindings in the menu the mouse must activate
	if (cls.bind_grab)
	{
		IN_ActivateMouse ();
		return;
	}

	if (!cl.refresh_prepped || cls.key_dest == key_console || cls.key_dest == key_menu)
	{
		// temporarily deactivate if not in fullscreen
		if (Cvar_VariableValue ("vid_fullscreen") == 0)
		{
			IN_DeactivateMouse ();
			return;
		}
	}

	IN_ActivateMouse ();
}


/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates (void)
{
	mouse_oldbuttonstate = 0;
}


int IN_MapKey (int key)
{
	int result;
	int modified = (key >> 16) & 255;
	qboolean is_extended = false;

	static byte scantokey[128] = {
		// scancode to quake key table
		// 0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f
		0x00, 0x1b, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2d, 0x3d, 0x7f, 0x09,		// 0x0
		0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6f, 0x70, 0x5b, 0x5d, 0x0d, 0x85, 0x61, 0x73,		// 0x1
		0x64, 0x66, 0x67, 0x68, 0x6a, 0x6b, 0x6c, 0x3b, 0x27, 0x60, 0x86, 0x5c, 0x7a, 0x78, 0x63, 0x76,		// 0x2
		0x62, 0x6e, 0x6d, 0x2c, 0x2e, 0x2f, 0x86, 0x2a, 0x84, 0x20, 0x99, 0x87, 0x88, 0x89, 0x8a, 0x8b,		// 0x3
		0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0xff, 0x00, 0x97, 0x80, 0x96, 0x2d, 0x82, 0x35, 0x83, 0x2b, 0x98,		// 0x4
		0x81, 0x95, 0x93, 0x94, 0x00, 0x00, 0x00, 0x91, 0x92, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// 0x5
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		// 0x6
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00		// 0x7
	};

	if (modified > 127)
		return 0;

	if (key & (1 << 24))
		is_extended = true;

	result = scantokey[modified];

	if (!is_extended)
	{
		switch (result)
		{
		case K_HOME: return K_KP_HOME;
		case K_UPARROW: return K_KP_UPARROW;
		case K_PGUP: return K_KP_PGUP;
		case K_LEFTARROW: return K_KP_LEFTARROW;
		case K_RIGHTARROW: return K_KP_RIGHTARROW;
		case K_END: return K_KP_END;
		case K_DOWNARROW: return K_KP_DOWNARROW;
		case K_PGDN: return K_KP_PGDN;
		case K_INS: return K_KP_INS;
		case K_DEL: return K_KP_DEL;
		default: return result;
		}
	}
	else
	{
		switch (result)
		{
		case 0x0D: return K_KP_ENTER;
		case 0x2F: return K_KP_SLASH;
		case 0xAF: return K_KP_PLUS;
		default: return result;
		}
	}
}

