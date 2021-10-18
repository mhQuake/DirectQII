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

#ifdef RENDERER
#include "q_shared.h"
#include "qfiles.h"

// make qsort prettier
typedef int (*sortfunc_t) (const void *, const void *);
#else
#include "qcommon.h"
#endif

typedef struct viddef_s {
	// main 3d view and cinematics
	int		width;
	int		height;

	// 2d gui objects
	int		conwidth;
	int		conheight;
} viddef_t;


// for passing video mode stuff to the video menu
typedef struct vidmenu_s {
	int *widths;
	int numwidths;

	int *heights;
	int numheights;

	char **fsmodes;
	int numfsmodes;
} vidmenu_t;


#define	MAX_DLIGHTS		64
#define	MAX_ENTITIES	256
#define	MAX_PARTICLES	32768
#define	MAX_LIGHTSTYLES	256

#define POWERSUIT_SCALE		4.0F

#define SHELL_RED_COLOR		0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR		0xDC
//#define SHELL_RB_COLOR		0x86
#define SHELL_RB_COLOR		0x68
#define SHELL_BG_COLOR		0x78

//ROGUE
#define SHELL_DOUBLE_COLOR	0xDF // 223
#define	SHELL_HALF_DAM_COLOR	0x90
#define SHELL_CYAN_COLOR	0x72
//ROGUE

#define SHELL_WHITE_COLOR	0xD7

typedef struct entity_s {
	struct model_s		*model;			// opaque type outside refresh
	float				angles[3];

	// most recent data
	float				currorigin[3];		// also used as RF_BEAM's "from"
	int					currframe;			// also used as RF_BEAM's diameter

	// previous data for lerping
	float				prevorigin[3];	// also used as RF_BEAM's "to"
	int					prevframe;

	// misc
	float	backlerp;				// 0.0 = current, 1.0 = old
	int		skinnum;				// also used as RF_BEAM's palette index

	int		lightstyle;				// for flashing entities
	float	alpha;					// ignore if RF_TRANSLUCENT isn't set

	struct image_s	*skin;			// NULL for inline skin
	int		flags;

} entity_t;

#define ENTITY_FLAGS  68

typedef struct dlight_s {
	// this layout allows us to use the dlight struct directly in a cbuffer
	vec3_t	origin;
	float	radius;
	vec3_t	color;
	int		numsurfaces;	// cbuffer padding; count of surfaces with light
} dlight_t;

typedef struct particle_s {
	vec3_t	origin;
	vec3_t	velocity;
	vec3_t	acceleration;
	float	time;
	int		color;
	float	alpha;
	int		size;
} particle_t;

typedef struct fov_s {
	float x, y;
} fov_t;

typedef struct refdef_s {
	int			x, y, width, height; // in virtual screen coordinates
	float		fovvar;		// raw FOV value before adjustment for widescreen/etc
	fov_t		fov;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];			// rgba 0-1 full screen blend
	float		time;				// time is uesed to auto animate
	int			rdflags;			// RDF_UNDERWATER, etc

	byte		*areabits;			// if not NULL, only areas with set bits will be drawn

	float		*lightstyles;	// [MAX_LIGHTSTYLES]

	int			num_entities;
	entity_t	*entities;

	int			num_dlights;
	dlight_t	*dlights;

	int			num_particles;
	particle_t	*particles;
} refdef_t;



#define	API_VERSION			4

// flags which control aspects of the behaviour of the refresh
#define SCR_DEFAULT			(0)			// default update with all cvars and options respected
#define SCR_NO_GAMMA		(1 << 0)	// ignore the value of the vid_gamma cvar
#define SCR_NO_BRIGHTNESS	(1 << 1)	// ignore the value of the vid_brightness cvar
#define SCR_NO_VSYNC		(1 << 2)	// force no vsync, irrespective of the value of the vid_vsync cvar
#define SCR_NO_PRESENT		(1 << 3)	// draw a full screen but do not swapbuffers
#define SCR_SYNC_PIPELINE	(1 << 4)	// forces a pipeline sync
#define SCR_NO_2D_UI		(1 << 5)	// draw 3d view only (use for mapshots, custom screenshots, etc); doesn't override cinematics or the loading screen

// these are the functions exported by the refresh module
typedef struct refexport_s {
	// if api_version is different, the dll cannot be used
	int api_version;

	// called when the library is loaded
	int (*Init) (void *hinstance, void *wndproc);

	// called before the library is unloaded
	void (*Shutdown) (void);

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time if necessary.

	// EndRegistration will free any remaining data that wasn't registered.
	// Any model_s or skin_s pointers from before the BeginRegistration
	// are no longer valid after EndRegistration.

	// Skins and images need to be differentiated, because skins
	// are flood filled to eliminate mip map edge errors, and pics have
	// an implicit "pics/" prepended to the name. (a pic name that starts with a
	// slash will not use the "pics/" prefix or the ".pcx" postfix)
	void (*BeginRegistration) (char *map);
	struct model_s *(*RegisterModel) (char *name);
	struct image_s *(*RegisterSkin) (char *name);
	struct image_s *(*RegisterPic) (char *name);
	void (*SetSky) (char *name, float rotate, vec3_t axis);
	void (*EndRegistration) (void);

	void (*RenderFrame) (refdef_t *fd);

	void (*DrawConsoleBackground) (int x, int y, int w, int h, char *pic, int alpha);
	qboolean (*DrawGetPicSize) (int *w, int *h, char *name);	// will return 0 0 if not found
	void (*DrawStretchPic) (int x, int y, int w, int h, char *pic);
	void (*DrawPic) (int x, int y, char *name);
	void (*DrawFill) (int x, int y, int w, int h, int c);
	void (*DrawFadeScreen) (void);
	void (*Clear) (void);

	void (*DrawChar) (int x, int y, int num);
	void (*DrawString) (void);
	void (*DrawField) (int x, int y, int color, int width, int value);

	// Draw images for cinematic rendering (which can have a different palette). Note that calls
	void (*DrawStretchRaw) (int cols, int rows, byte *data, int frame, const unsigned char *palette);

	// video mode and refresh state management entry points
	void (*BeginFrame) (viddef_t *vd, int scrflags);
	void (*Set2D) (void);
	void (*EndFrame) (int scrflags);

	void (*AppActivate) (qboolean activate);
	void (*EnumerateVideoModes) (void);
	void (*CaptureScreenshot) (char *checkname);
	void (*Mapshot) (char *checkname);
} refexport_t;

// these are the functions imported by the refresh module
typedef struct refimport_s {
	void (*Sys_Error) (int err_level, char *str, ...);
	void (*SendKeyEvents) (void);
	void (*Mkdir) (char *path);

	// loading temp allocations
	void (*Load_FreeMemory) (void);
	void *(*Load_AllocMemory) (int size);

	void (*Cmd_AddCommand) (char *name, void (*cmd) (void));
	void (*Cmd_RemoveCommand) (char *name);
	int (*Cmd_Argc) (void);
	char *(*Cmd_Argv) (int i);
	void (*Cmd_ExecuteText) (int exec_when, char *text);

	void (*Con_Printf) (int print_level, char *str, ...);

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int (*FS_LoadFile) (char *name, void **buf);
	void (*FS_FreeFile) (void *buf);

	// gamedir will be the current directory that generated
	// files should be stored to, ie: "f:\quake\id1"
	char *(*FS_Gamedir) (void);

	cvar_t *(*Cvar_Get) (char *name, char *value, int flags, cvarcallback_t callback);
	cvar_t *(*Cvar_Set) (char *name, char *value);
	void (*Cvar_SetValue) (char *name, float value);

	void (*Vid_MenuInit) (void);
	void (*Vid_PrepVideoMenu) (vidmenu_t *md);
	void (*Vid_NewWindow) (void);
} refimport_t;


// this is the only function actually exported at the linker level
refexport_t GetRefAPI (refimport_t rimp);

