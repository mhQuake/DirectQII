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
// cl_view.c -- player rendering positioning

#include "client.h"

//=============
//
// development tools for weapons
//
int			gun_frame;
struct model_s	*gun_model;

//=============

cvar_t		*crosshair;
cvar_t		*cl_testparticles;
cvar_t		*cl_testentities;
cvar_t		*cl_testlights;
cvar_t		*cl_testblend;

cvar_t		*cl_stats;

cvar_t		*intensity;


int			r_numdlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numentities;
entity_t	r_entities[MAX_ENTITIES];

int			r_numparticles;
particle_t	r_particles[MAX_PARTICLES];

float		r_lightstyles[MAX_LIGHTSTYLES];

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene (void)
{
	r_numdlights = 0;
	r_numentities = 0;
	r_numparticles = 0;
}


/*
=====================
V_AddEntity

=====================
*/
void V_AddEntity (entity_t *ent)
{
	if (r_numentities >= MAX_ENTITIES)
		return;

	// and add it
	r_entities[r_numentities] = *ent;

	// clear any render fx not defined by the engine in case mods misuse this for their own internal crap.
	// this allows us to define our own in-engine effects above RF_USE_DISGUISE and not have them stomped on.
	// it's safe to do this now without stomping on mods ourselves because the entity has been copied.
	// using 0x79FFF instead of 0x7FFFF because there's a gap between (1 << 12) and (1 << 15)
	r_entities[r_numentities].flags &= 0x79FFF;

	// go to the next entity
	r_numentities++;
}


/*
=====================
V_AddParticle

=====================
*/
void V_AddParticle (vec3_t org, vec3_t vel, vec3_t accel, float time, int color, float alpha)
{
	if (r_numparticles >= MAX_PARTICLES)
		return;
	else
	{
		particle_t *p = &r_particles[r_numparticles];

		VectorCopy (org, p->origin);
		VectorCopy (vel, p->velocity);
		VectorCopy (accel, p->acceleration);

		p->time = time;
		p->color = color;
		p->alpha = alpha;

		r_numparticles++;
	}
}

/*
=====================
V_AddLight

=====================
*/
void V_AddLight (vec3_t org, float radius, float r, float g, float b)
{
	if (r_numdlights >= MAX_DLIGHTS)
		return;
	else
	{
		dlight_t *dl = &r_dlights[r_numdlights];

		VectorCopy (org, dl->origin);
		dl->radius = radius;

		dl->color[0] = r;
		dl->color[1] = g;
		dl->color[2] = b;

		// normalize lighting to 1, 1, 1 scale
		VectorNormalize (dl->color);

		// scale by intensity
		dl->color[0] *= intensity->value;
		dl->color[1] *= intensity->value;
		dl->color[2] *= intensity->value;

		r_numdlights++;
	}
}


/*
=====================
V_AddLightStyle

=====================
*/
void V_AddLightStyle (int style, float value)
{
	if (style < 0 || style > MAX_LIGHTSTYLES)
		Com_Error (ERR_DROP, "Bad light style %i", style);

	// scale lightstyles to the overbright range when adding to the refresh
	if (intensity->value > 1.0f)
		r_lightstyles[style] = value * intensity->value;
	else r_lightstyles[style] = value;
}


/*
================
V_TestParticles

If cl_testparticles is set, create 4096 particles in the view
================
*/
void V_TestParticles (void)
{
	int	i;

	// begin with 0 particles
	r_numparticles = 0;

	// add them all
	for (i = 0; i < MAX_PARTICLES; i++)
	{
		float d = i * 0.25;
		float r = 4 * ((i & 7) - 3.5);
		float u = 4 * (((i >> 3) & 7) - 3.5);

		float org[3] = {
			cl.refdef.vieworg[0] + cl.v_forward[0] * d + cl.v_right[0] * r + cl.v_up[0] * u,
			cl.refdef.vieworg[1] + cl.v_forward[1] * d + cl.v_right[1] * r + cl.v_up[1] * u,
			cl.refdef.vieworg[2] + cl.v_forward[2] * d + cl.v_right[2] * r + cl.v_up[2] * u
		};

		V_AddParticle (org, vec3_origin, vec3_origin, 0, 8, cl_testparticles->value);
	}
}


/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
void V_TestEntities (void)
{
	int			i, j;
	float		f, r;
	entity_t	*ent;

	r_numentities = 32;
	memset (r_entities, 0, sizeof (r_entities));

	for (i = 0; i < r_numentities; i++)
	{
		ent = &r_entities[i];

		r = 64 * ((i % 4) - 1.5);
		f = 64 * (i / 4) + 128;

		for (j = 0; j < 3; j++)
			ent->currorigin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f + cl.v_right[j] * r;

		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights (void)
{
	int			i, j;
	float		f, r;
	dlight_t	*dl;

	r_numdlights = MAX_DLIGHTS;
	memset (r_dlights, 0, sizeof (r_dlights));

	for (i = 0; i < r_numdlights; i++)
	{
		dl = &r_dlights[i];

		r = 64 * ((i % 4) - 1.5);
		f = 64 * (i / 4) + 128;

		for (j = 0; j < 3; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f + cl.v_right[j] * r;

		dl->color[0] = ((i % 6) + 1) & 1;
		dl->color[1] = (((i % 6) + 1) & 2) >> 1;
		dl->color[2] = (((i % 6) + 1) & 4) >> 2;

		// normalize lighting to 1, 1, 1 scale
		VectorNormalize (dl->color);

		// scale by intensity
		dl->color[0] *= intensity->value;
		dl->color[1] *= intensity->value;
		dl->color[2] *= intensity->value;

		dl->radius = 200;
	}
}

//===================================================================

/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh (void)
{
	char		mapname[32];
	int			i;
	char		name[MAX_QPATH];
	float		rotate;
	vec3_t		axis;

	if (!cl.configstrings[CS_MODELS + 1][0])
		return;		// no map loaded

	// let the render dll load the map
	strcpy (mapname, cl.configstrings[CS_MODELS + 1] + 5);	// skip "maps/"
	mapname[strlen (mapname) - 4] = 0;		// cut off ".bsp"

	// register models, pics, and skins
	Com_Printf ("Map: %s\r", mapname);
	SCR_UpdateScreen (SCR_NO_VSYNC);
	re.BeginRegistration (mapname);
	Com_Printf ("                                     \r");

	// precache status bar pics
	Com_Printf ("pics\r");
	SCR_UpdateScreen (SCR_NO_VSYNC);
	SCR_TouchPics ();
	Com_Printf ("                                     \r");

	CL_RegisterTEntModels ();

	num_cl_weaponmodels = 1;
	strcpy (cl_weaponmodels[0], "weapon.md2");

	for (i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++)
	{
		strcpy (name, cl.configstrings[CS_MODELS + i]);
		name[37] = 0;	// never go beyond one line
		if (name[0] != '*')
			Com_Printf ("%s\r", name);
		SCR_UpdateScreen (SCR_NO_VSYNC);
		Sys_SendKeyEvents ();	// pump message loop

		if (name[0] == '#')
		{
			// special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				strncpy (cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS + i] + 1,
					sizeof (cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
		}
		else
		{
			cl.model_draw[i] = re.RegisterModel (cl.configstrings[CS_MODELS + i]);
			if (name[0] == '*')
				cl.model_clip[i] = CM_InlineModel (cl.configstrings[CS_MODELS + i]);
			else
				cl.model_clip[i] = NULL;
		}
		if (name[0] != '*')
			Com_Printf ("                                     \r");
	}

	Com_Printf ("images\r", i);
	SCR_UpdateScreen (SCR_NO_VSYNC);
	for (i = 1; i < MAX_IMAGES && cl.configstrings[CS_IMAGES + i][0]; i++)
	{
		cl.image_precache[i] = re.RegisterPic (cl.configstrings[CS_IMAGES + i]);
		Sys_SendKeyEvents ();	// pump message loop
	}

	Com_Printf ("                                     \r");
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS + i][0])
			continue;
		Com_Printf ("client %i\r", i);
		SCR_UpdateScreen (SCR_NO_VSYNC);
		Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
		Com_Printf ("                                     \r");
	}

	CL_LoadClientinfo (&cl.baseclientinfo, "unnamed\\male/grunt");

	// set sky textures and speed
	Com_Printf ("sky\r", i);
	SCR_UpdateScreen (SCR_NO_VSYNC);
	rotate = atof (cl.configstrings[CS_SKYROTATE]);
	sscanf (cl.configstrings[CS_SKYAXIS], "%f %f %f",
		&axis[0], &axis[1], &axis[2]);
	re.SetSky (cl.configstrings[CS_SKY], rotate, axis);
	Com_Printf ("                                     \r");

	// the renderer can now free unneeded stuff
	re.EndRegistration ();

	// clear any lines of console text
	Con_ClearNotify ();
	SCR_ClearCenterString ();

	SCR_UpdateScreen (SCR_DEFAULT);
	cl.refresh_prepped = true;
	cl.force_refdef = true;	// make sure we have a valid refdef

	// start the cd track
	CDAudio_Play (atoi (cl.configstrings[CS_CDTRACK]), true);
}

/*
====================
CalcFov
====================
*/
float SCR_CalcFovX (float fov_y, float width, float height)
{
	// bound, don't crash
	if (fov_y < 1) fov_y = 1;
	if (fov_y > 179) fov_y = 179;

	return (atan (width / (height / tan ((fov_y * M_PI) / 360.0f))) * 360.0f) / M_PI;
}


float SCR_CalcFovY (float fov_x, float width, float height)
{
	// bound, don't crash
	if (fov_x < 1) fov_x = 1;
	if (fov_x > 179) fov_x = 179;

	return (atan (height / (width / tan ((fov_x * M_PI) / 360.0f))) * 360.0f) / M_PI;
}


void SCR_SetFOV (fov_t *fov, float fovvar, int width, int height)
{
	float aspect = (float) height / (float) width;

	// set up relative to a baseline aspect of 640x480
#define BASELINE_W	640.0f
#define BASELINE_H	480.0f

	// http://www.gamedev.net/topic/431111-perspective-math-calculating-horisontal-fov-from-vertical/
	// horizontalFov = atan (tan (verticalFov) * aspectratio)
	// verticalFov = atan (tan (horizontalFov) / aspectratio)
	if (aspect > (BASELINE_H / BASELINE_W))
	{
		// use the same calculation as GLQuake did (horizontal is constant, vertical varies)
		fov->x = fovvar;
		fov->y = SCR_CalcFovY (fov->x, width, height);
	}
	else
	{
		// alternate calculation (vertical is constant, horizontal varies)
		// consistent with http://www.emsai.net/projects/widescreen/fovcalc/
		// note that the gun always uses this calculation irrespective of the aspect)
		fov->y = SCR_CalcFovY (fovvar, BASELINE_W, BASELINE_H);
		fov->x = SCR_CalcFovX (fov->y, width, height);
	}
}


//============================================================================

// gun frame debugging functions
void V_Gun_Next_f (void)
{
	gun_frame++;
	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Prev_f (void)
{
	gun_frame--;
	if (gun_frame < 0)
		gun_frame = 0;
	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Model_f (void)
{
	char	name[MAX_QPATH];

	if (Cmd_Argc () != 2)
	{
		gun_model = NULL;
		return;
	}

	Com_sprintf (name, sizeof (name), "models/%s/tris.md2", Cmd_Argv (1));
	gun_model = re.RegisterModel (name);
}

//============================================================================


/*
=================
SCR_DrawCrosshair
=================
*/
void SCR_DrawCrosshair (void)
{
	if (cls.state != ca_active)
		return;

	if (!cl.refresh_prepped)
		return;			// still loading

	if (!crosshair->value)
		return;

	if (crosshair->modified)
	{
		crosshair->modified = false;
		SCR_TouchPics ();
	}

	if (!crosshair_pic[0])
		return;

	re.DrawPic ((viddef.conwidth - crosshair_width) >> 1, (viddef.conheight - crosshair_height) >> 1, crosshair_pic);
}


/*
==================
V_RenderView

==================
*/
void V_RenderView (void)
{
	extern int entitycmpfnc (const entity_t *, const entity_t *);

	if (cls.state != ca_active)
		return;

	if (!cl.refresh_prepped)
		return;			// still loading

	if (cl_timedemo->value)
	{
		if (!cl.timedemo_start)
			cl.timedemo_start = Sys_Milliseconds ();
		cl.timedemo_frames++;
	}

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if (cl.frame.valid && (cl.force_refdef || !cl_paused->value))
	{
		cl.force_refdef = false;

		V_ClearScene ();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// v_forward, etc.
		CL_AddEntities ();

		if (cl_testparticles->value) V_TestParticles ();
		if (cl_testentities->value) V_TestEntities ();
		if (cl_testlights->value) V_TestLights ();

		if (cl_testblend->value)
		{
			cl.refdef.blend[0] = 1;
			cl.refdef.blend[1] = 0.5;
			cl.refdef.blend[2] = 0.25;
			cl.refdef.blend[3] = 0.5;
		}

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		cl.refdef.vieworg[0] += 1.0 / 16;
		cl.refdef.vieworg[1] += 1.0 / 16;
		cl.refdef.vieworg[2] += 1.0 / 16;

		cl.refdef.x = 0;
		cl.refdef.y = 0;
		cl.refdef.width = viddef.width;
		cl.refdef.height = viddef.height;

		SCR_SetFOV (&cl.refdef.main_fov, cl.refdef.main_fov.x, cl.refdef.width, cl.refdef.height);

		// compute a separate FOV for the gun so that values > 90 cna be handled without it looking like you're playing Wipeout
		if (cl.refdef.main_fov.x > 90)
			SCR_SetFOV (&cl.refdef.gun_fov, 90, cl.refdef.width, cl.refdef.height);
		else SCR_SetFOV (&cl.refdef.gun_fov, cl.refdef.main_fov.x, cl.refdef.width, cl.refdef.height);

		cl.refdef.time = cl.time * 0.001;
		cl.refdef.areabits = cl.frame.areabits;

		if (!cl_add_entities->value) r_numentities = 0;
		if (!cl_add_particles->value) r_numparticles = 0;
		if (!cl_add_lights->value) r_numdlights = 0;

		if (!cl_add_blend->value)
		{
			VectorClear (cl.refdef.blend);
		}

		cl.refdef.num_entities = r_numentities;
		cl.refdef.entities = r_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;
		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;

		// sort entities for better cache locality
		qsort (cl.refdef.entities, cl.refdef.num_entities, sizeof (cl.refdef.entities[0]), (int (*) (const void *, const void *)) entitycmpfnc);
	}

	re.RenderFrame (&cl.refdef);

	if (cl_stats->value)
		Com_Printf ("ent:%i  lt:%i  part:%i\n", r_numentities, r_numdlights, r_numparticles);
}


/*
=============
V_Viewpos_f
=============
*/
void V_Viewpos_f (void)
{
	Com_Printf ("(%i %i %i) : %i\n", (int) cl.refdef.vieworg[0], (int) cl.refdef.vieworg[1], (int) cl.refdef.vieworg[2], (int) cl.refdef.viewangles[1]);
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("gun_next", V_Gun_Next_f);
	Cmd_AddCommand ("gun_prev", V_Gun_Prev_f);
	Cmd_AddCommand ("gun_model", V_Gun_Model_f);

	Cmd_AddCommand ("viewpos", V_Viewpos_f);

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE);

	cl_testblend = Cvar_Get ("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get ("cl_testlights", "0", CVAR_CHEAT);

	cl_stats = Cvar_Get ("cl_stats", "0", 0);

	intensity = Cvar_Get ("intensity", "2", 0);
}
