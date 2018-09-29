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
// cl_fx.c -- entity effects parsing and management

#include "client.h"

void CL_LogoutEffect (vec3_t org, int type);

/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/

typedef struct clightstyle_s
{
	int		length;
	float	value;
	float	map[MAX_QPATH];
} clightstyle_t;

clightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
int			lastofs;

/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles (void)
{
	memset (cl_lightstyle, 0, sizeof (cl_lightstyle));
	lastofs = -1;
}

/*
================
CL_RunLightStyles
================
*/
void CL_RunLightStyles (void)
{
	int		ofs;
	int		i;
	clightstyle_t	*ls;

	ofs = cl.time / 100;

	if (ofs == lastofs)
		return;

	lastofs = ofs;

	for (i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++)
	{
		if (!ls->length)
			ls->value = 1.0;
		else if (ls->length == 1)
			ls->value = ls->map[0];
		else ls->value = ls->map[ofs % ls->length];
	}
}


void CL_SetLightstyle (int i)
{
	char	*s;
	int		j, k;

	s = cl.configstrings[i + CS_LIGHTS];

	j = strlen (s);
	if (j >= MAX_QPATH)
		Com_Error (ERR_DROP, "svc_lightstyle length=%i", j);

	cl_lightstyle[i].length = j;

	for (k = 0; k < j; k++)
		cl_lightstyle[i].map[k] = (float) (s[k] - 'a') / (float) ('m' - 'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles (void)
{
	int		i;
	clightstyle_t	*ls;

	for (i = 0, ls = cl_lightstyle; i < MAX_LIGHTSTYLES; i++, ls++)
		V_AddLightStyle (i, ls->value);
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

cdlight_t cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights (void)
{
	memset (cl_dlights, 0, sizeof (cl_dlights));
}


void CL_SetDlightColour (cdlight_t *dl, int r, int g, int b)
{
	dl->rgb[0] = (float) r / 256.0f;
	dl->rgb[1] = (float) g / 256.0f;
	dl->rgb[2] = (float) b / 256.0f;
}


void CL_NewDlight (cdlight_t *dl, int key)
{
	memset (dl, 0, sizeof (*dl));

	dl->minlight = 0;
	dl->key = key;
	dl->time = cl.time;

	CL_SetDlightColour (dl, DL_COLOR_WHITE);
}


/*
===============
CL_AllocDlight

===============
*/
cdlight_t *CL_AllocDlight (int key)
{
	int		i;
	cdlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;

		for (i = 0; i < MAX_DLIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				CL_NewDlight (dl, key);
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;

	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			CL_NewDlight (dl, key);
			return dl;
		}
	}

	// just take the first one
	dl = &cl_dlights[0];
	CL_NewDlight (dl, key);

	return dl;
}


/*
==============
CL_ParseMuzzleFlash
==============
*/
void CL_ParseMuzzleFlash (void)
{
	vec3_t		fv, rv;
	cdlight_t	*dl;
	int			i, weapon;
	centity_t	*pl;
	int			silenced;
	float		volume;
	char		soundname[64];

	i = MSG_ReadShort (&net_message);
	if (i < 1 || i >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash: bad entity");

	weapon = MSG_ReadByte (&net_message);
	silenced = weapon & MZ_SILENCED;
	weapon &= ~MZ_SILENCED;

	pl = &cl_entities[i];

	dl = CL_AllocDlight (i);
	VectorCopy (pl->current.origin, dl->origin);
	AngleVectors (pl->current.angles, fv, rv, NULL);
	VectorMA (dl->origin, 18, fv, dl->origin);
	VectorMA (dl->origin, 16, rv, dl->origin);

	if (silenced)
		dl->radius = 100 + (rand () & 31);
	else
		dl->radius = 200 + (rand () & 31);

	dl->minlight = 32;
	dl->die = cl.time + 100;

	if (silenced)
		volume = 0.2;
	else
		volume = 1;

	switch (weapon)
	{
	case MZ_BLASTER:
		CL_SetDlightColour (dl, DL_COLOR_YELLOW);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_BLUEHYPERBLASTER:
		CL_SetDlightColour (dl, DL_COLOR_BLUE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_HYPERBLASTER:
		CL_SetDlightColour (dl, DL_COLOR_YELLOW);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_MACHINEGUN:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		break;
	case MZ_SHOTGUN:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/shotgf1b.wav"), volume, ATTN_NORM, 0);
		S_StartSound (NULL, i, CHAN_AUTO, S_RegisterSound ("weapons/shotgr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case MZ_SSHOTGUN:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/sshotf1b.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_CHAINGUN1:
		dl->radius = 200 + (rand () & 31);
		CL_SetDlightColour (dl, DL_COLOR_RED);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		break;
	case MZ_CHAINGUN2:
		dl->radius = 225 + (rand () & 31);
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		dl->die = cl.time + 100;	// long delay
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0.05);
		break;
	case MZ_CHAINGUN3:
		dl->radius = 250 + (rand () & 31);
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		dl->die = cl.time + 100;	// long delay
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0.033);
		Com_sprintf (soundname, sizeof (soundname), "weapons/machgf%ib.wav", (rand () % 5) + 1);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0.066);
		break;
	case MZ_RAILGUN:
		CL_SetDlightColour (dl, DL_COLOR_CYAN);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/railgf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_ROCKET:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/rocklf1a.wav"), volume, ATTN_NORM, 0);
		S_StartSound (NULL, i, CHAN_AUTO, S_RegisterSound ("weapons/rocklr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case MZ_GRENADE:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), volume, ATTN_NORM, 0);
		S_StartSound (NULL, i, CHAN_AUTO, S_RegisterSound ("weapons/grenlr1b.wav"), volume, ATTN_NORM, 0.1);
		break;
	case MZ_BFG:
		CL_SetDlightColour (dl, DL_COLOR_GREEN);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/bfg__f1y.wav"), volume, ATTN_NORM, 0);
		break;

	case MZ_LOGIN:
		CL_SetDlightColour (dl, DL_COLOR_GREEN);
		dl->die = cl.time + 1000;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	case MZ_LOGOUT:
		CL_SetDlightColour (dl, DL_COLOR_RED);
		dl->die = cl.time + 1000;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (pl->current.origin, weapon);
		break;
	case MZ_RESPAWN:
		CL_SetDlightColour (dl, DL_COLOR_YELLOW);
		dl->die = cl.time + 1000;
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (pl->current.origin, weapon);
		break;
		// RAFAEL
	case MZ_PHALANX:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/plasshot.wav"), volume, ATTN_NORM, 0);
		break;
		// RAFAEL
	case MZ_IONRIPPER:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/rippfire.wav"), volume, ATTN_NORM, 0);
		break;

		// ======================
		// PGM
	case MZ_ETF_RIFLE:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/nail1.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_SHOTGUN2:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/shotg2.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_HEATBEAM:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		dl->die = cl.time + 100;
		//		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound("weapons/bfg__l1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_BLASTER2:
		CL_SetDlightColour (dl, DL_COLOR_GREEN);
		// FIXME - different sound for blaster2 ??
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_TRACKER:
		// negative flashes handled the same in gl/soft until CL_AddDLights
		CL_SetDlightColour (dl, DL_COLOR_NEGATIVE);
		S_StartSound (NULL, i, CHAN_WEAPON, S_RegisterSound ("weapons/disint2.wav"), volume, ATTN_NORM, 0);
		break;
	case MZ_NUKE1:
		CL_SetDlightColour (dl, DL_COLOR_RED);
		dl->die = cl.time + 100;
		break;
	case MZ_NUKE2:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		dl->die = cl.time + 100;
		break;
	case MZ_NUKE4:
		CL_SetDlightColour (dl, DL_COLOR_BLUE);
		dl->die = cl.time + 100;
		break;
	case MZ_NUKE8:
		CL_SetDlightColour (dl, DL_COLOR_CYAN);
		dl->die = cl.time + 100;
		break;
		// PGM
		// ======================
	}
}


/*
==============
CL_ParseMuzzleFlash2
==============
*/
void CL_ParseMuzzleFlash2 (void)
{
	int			ent;
	vec3_t		origin;
	int			flash_number;
	cdlight_t	*dl;
	vec3_t		forward, right;
	char		soundname[64];

	ent = MSG_ReadShort (&net_message);
	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash2: bad entity");

	flash_number = MSG_ReadByte (&net_message);

	// locate the origin
	AngleVectors (cl_entities[ent].current.angles, forward, right, NULL);
	origin[0] = cl_entities[ent].current.origin[0] + forward[0] * monster_flash_offset[flash_number][0] + right[0] * monster_flash_offset[flash_number][1];
	origin[1] = cl_entities[ent].current.origin[1] + forward[1] * monster_flash_offset[flash_number][0] + right[1] * monster_flash_offset[flash_number][1];
	origin[2] = cl_entities[ent].current.origin[2] + forward[2] * monster_flash_offset[flash_number][0] + right[2] * monster_flash_offset[flash_number][1] + monster_flash_offset[flash_number][2];

	dl = CL_AllocDlight (ent);
	VectorCopy (origin, dl->origin);
	dl->radius = 200 + (rand () & 31);
	dl->minlight = 32;
	dl->die = cl.time + 100;

	switch (flash_number)
	{
	case MZ2_INFANTRY_MACHINEGUN_1:
	case MZ2_INFANTRY_MACHINEGUN_2:
	case MZ2_INFANTRY_MACHINEGUN_3:
	case MZ2_INFANTRY_MACHINEGUN_4:
	case MZ2_INFANTRY_MACHINEGUN_5:
	case MZ2_INFANTRY_MACHINEGUN_6:
	case MZ2_INFANTRY_MACHINEGUN_7:
	case MZ2_INFANTRY_MACHINEGUN_8:
	case MZ2_INFANTRY_MACHINEGUN_9:
	case MZ2_INFANTRY_MACHINEGUN_10:
	case MZ2_INFANTRY_MACHINEGUN_11:
	case MZ2_INFANTRY_MACHINEGUN_12:
	case MZ2_INFANTRY_MACHINEGUN_13:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_MACHINEGUN_1:
	case MZ2_SOLDIER_MACHINEGUN_2:
	case MZ2_SOLDIER_MACHINEGUN_3:
	case MZ2_SOLDIER_MACHINEGUN_4:
	case MZ2_SOLDIER_MACHINEGUN_5:
	case MZ2_SOLDIER_MACHINEGUN_6:
	case MZ2_SOLDIER_MACHINEGUN_7:
	case MZ2_SOLDIER_MACHINEGUN_8:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("soldier/solatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_MACHINEGUN_1:
	case MZ2_GUNNER_MACHINEGUN_2:
	case MZ2_GUNNER_MACHINEGUN_3:
	case MZ2_GUNNER_MACHINEGUN_4:
	case MZ2_GUNNER_MACHINEGUN_5:
	case MZ2_GUNNER_MACHINEGUN_6:
	case MZ2_GUNNER_MACHINEGUN_7:
	case MZ2_GUNNER_MACHINEGUN_8:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("gunner/gunatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_ACTOR_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_1:
	case MZ2_SUPERTANK_MACHINEGUN_2:
	case MZ2_SUPERTANK_MACHINEGUN_3:
	case MZ2_SUPERTANK_MACHINEGUN_4:
	case MZ2_SUPERTANK_MACHINEGUN_5:
	case MZ2_SUPERTANK_MACHINEGUN_6:
	case MZ2_TURRET_MACHINEGUN:			// PGM
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_BOSS2_MACHINEGUN_L1:
	case MZ2_BOSS2_MACHINEGUN_L2:
	case MZ2_BOSS2_MACHINEGUN_L3:
	case MZ2_BOSS2_MACHINEGUN_L4:
	case MZ2_BOSS2_MACHINEGUN_L5:
	case MZ2_CARRIER_MACHINEGUN_L1:		// PMM
	case MZ2_CARRIER_MACHINEGUN_L2:		// PMM
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("infantry/infatck1.wav"), 1, ATTN_NONE, 0);
		break;

	case MZ2_SOLDIER_BLASTER_1:
	case MZ2_SOLDIER_BLASTER_2:
	case MZ2_SOLDIER_BLASTER_3:
	case MZ2_SOLDIER_BLASTER_4:
	case MZ2_SOLDIER_BLASTER_5:
	case MZ2_SOLDIER_BLASTER_6:
	case MZ2_SOLDIER_BLASTER_7:
	case MZ2_SOLDIER_BLASTER_8:
	case MZ2_TURRET_BLASTER:			// PGM
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("soldier/solatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLYER_BLASTER_1:
	case MZ2_FLYER_BLASTER_2:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("flyer/flyatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_MEDIC_BLASTER_1:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("medic/medatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_HOVER_BLASTER_1:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("hover/hovatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_FLOAT_BLASTER_1:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("floater/fltatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SOLDIER_SHOTGUN_1:
	case MZ2_SOLDIER_SHOTGUN_2:
	case MZ2_SOLDIER_SHOTGUN_3:
	case MZ2_SOLDIER_SHOTGUN_4:
	case MZ2_SOLDIER_SHOTGUN_5:
	case MZ2_SOLDIER_SHOTGUN_6:
	case MZ2_SOLDIER_SHOTGUN_7:
	case MZ2_SOLDIER_SHOTGUN_8:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("soldier/solatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_BLASTER_1:
	case MZ2_TANK_BLASTER_2:
	case MZ2_TANK_BLASTER_3:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_MACHINEGUN_1:
	case MZ2_TANK_MACHINEGUN_2:
	case MZ2_TANK_MACHINEGUN_3:
	case MZ2_TANK_MACHINEGUN_4:
	case MZ2_TANK_MACHINEGUN_5:
	case MZ2_TANK_MACHINEGUN_6:
	case MZ2_TANK_MACHINEGUN_7:
	case MZ2_TANK_MACHINEGUN_8:
	case MZ2_TANK_MACHINEGUN_9:
	case MZ2_TANK_MACHINEGUN_10:
	case MZ2_TANK_MACHINEGUN_11:
	case MZ2_TANK_MACHINEGUN_12:
	case MZ2_TANK_MACHINEGUN_13:
	case MZ2_TANK_MACHINEGUN_14:
	case MZ2_TANK_MACHINEGUN_15:
	case MZ2_TANK_MACHINEGUN_16:
	case MZ2_TANK_MACHINEGUN_17:
	case MZ2_TANK_MACHINEGUN_18:
	case MZ2_TANK_MACHINEGUN_19:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		Com_sprintf (soundname, sizeof (soundname), "tank/tnkatk2%c.wav", 'a' + rand () % 5);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound (soundname), 1, ATTN_NORM, 0);
		break;

	case MZ2_CHICK_ROCKET_1:
	case MZ2_TURRET_ROCKET:			// PGM
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("chick/chkatck2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_TANK_ROCKET_1:
	case MZ2_TANK_ROCKET_2:
	case MZ2_TANK_ROCKET_3:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("tank/tnkatck1.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_SUPERTANK_ROCKET_1:
	case MZ2_SUPERTANK_ROCKET_2:
	case MZ2_SUPERTANK_ROCKET_3:
	case MZ2_BOSS2_ROCKET_1:
	case MZ2_BOSS2_ROCKET_2:
	case MZ2_BOSS2_ROCKET_3:
	case MZ2_BOSS2_ROCKET_4:
	case MZ2_CARRIER_ROCKET_1:
		//	case MZ2_CARRIER_ROCKET_2:
		//	case MZ2_CARRIER_ROCKET_3:
		//	case MZ2_CARRIER_ROCKET_4:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("tank/rocket.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GUNNER_GRENADE_1:
	case MZ2_GUNNER_GRENADE_2:
	case MZ2_GUNNER_GRENADE_3:
	case MZ2_GUNNER_GRENADE_4:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("gunner/gunatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_GLADIATOR_RAILGUN_1:
		// PMM
	case MZ2_CARRIER_RAILGUN:
	case MZ2_WIDOW_RAIL:
		// pmm
		CL_SetDlightColour (dl, DL_COLOR_CYAN);
		break;

		// --- Xian's shit starts ---
	case MZ2_MAKRON_BFG:
		CL_SetDlightColour (dl, DL_COLOR_GREEN);
		//S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound("makron/bfg_fire.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_MAKRON_BLASTER_1:
	case MZ2_MAKRON_BLASTER_2:
	case MZ2_MAKRON_BLASTER_3:
	case MZ2_MAKRON_BLASTER_4:
	case MZ2_MAKRON_BLASTER_5:
	case MZ2_MAKRON_BLASTER_6:
	case MZ2_MAKRON_BLASTER_7:
	case MZ2_MAKRON_BLASTER_8:
	case MZ2_MAKRON_BLASTER_9:
	case MZ2_MAKRON_BLASTER_10:
	case MZ2_MAKRON_BLASTER_11:
	case MZ2_MAKRON_BLASTER_12:
	case MZ2_MAKRON_BLASTER_13:
	case MZ2_MAKRON_BLASTER_14:
	case MZ2_MAKRON_BLASTER_15:
	case MZ2_MAKRON_BLASTER_16:
	case MZ2_MAKRON_BLASTER_17:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("makron/blaster.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_JORG_MACHINEGUN_L1:
	case MZ2_JORG_MACHINEGUN_L2:
	case MZ2_JORG_MACHINEGUN_L3:
	case MZ2_JORG_MACHINEGUN_L4:
	case MZ2_JORG_MACHINEGUN_L5:
	case MZ2_JORG_MACHINEGUN_L6:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("boss3/xfire.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_JORG_MACHINEGUN_R1:
	case MZ2_JORG_MACHINEGUN_R2:
	case MZ2_JORG_MACHINEGUN_R3:
	case MZ2_JORG_MACHINEGUN_R4:
	case MZ2_JORG_MACHINEGUN_R5:
	case MZ2_JORG_MACHINEGUN_R6:
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		break;

	case MZ2_JORG_BFG_1:
		CL_SetDlightColour (dl, DL_COLOR_GREEN);
		break;

	case MZ2_BOSS2_MACHINEGUN_R1:
	case MZ2_BOSS2_MACHINEGUN_R2:
	case MZ2_BOSS2_MACHINEGUN_R3:
	case MZ2_BOSS2_MACHINEGUN_R4:
	case MZ2_BOSS2_MACHINEGUN_R5:
	case MZ2_CARRIER_MACHINEGUN_R1:			// PMM
	case MZ2_CARRIER_MACHINEGUN_R2:			// PMM

		CL_SetDlightColour (dl, DL_COLOR_ORANGE);

		CL_ParticleEffect (origin, vec3_origin, 0, 40);
		CL_SmokeAndFlash (origin);
		break;

		// ======
		// ROGUE
	case MZ2_STALKER_BLASTER:
	case MZ2_DAEDALUS_BLASTER:
	case MZ2_MEDIC_BLASTER_2:
	case MZ2_WIDOW_BLASTER:
	case MZ2_WIDOW_BLASTER_SWEEP1:
	case MZ2_WIDOW_BLASTER_SWEEP2:
	case MZ2_WIDOW_BLASTER_SWEEP3:
	case MZ2_WIDOW_BLASTER_SWEEP4:
	case MZ2_WIDOW_BLASTER_SWEEP5:
	case MZ2_WIDOW_BLASTER_SWEEP6:
	case MZ2_WIDOW_BLASTER_SWEEP7:
	case MZ2_WIDOW_BLASTER_SWEEP8:
	case MZ2_WIDOW_BLASTER_SWEEP9:
	case MZ2_WIDOW_BLASTER_100:
	case MZ2_WIDOW_BLASTER_90:
	case MZ2_WIDOW_BLASTER_80:
	case MZ2_WIDOW_BLASTER_70:
	case MZ2_WIDOW_BLASTER_60:
	case MZ2_WIDOW_BLASTER_50:
	case MZ2_WIDOW_BLASTER_40:
	case MZ2_WIDOW_BLASTER_30:
	case MZ2_WIDOW_BLASTER_20:
	case MZ2_WIDOW_BLASTER_10:
	case MZ2_WIDOW_BLASTER_0:
	case MZ2_WIDOW_BLASTER_10L:
	case MZ2_WIDOW_BLASTER_20L:
	case MZ2_WIDOW_BLASTER_30L:
	case MZ2_WIDOW_BLASTER_40L:
	case MZ2_WIDOW_BLASTER_50L:
	case MZ2_WIDOW_BLASTER_60L:
	case MZ2_WIDOW_BLASTER_70L:
	case MZ2_WIDOW_RUN_1:
	case MZ2_WIDOW_RUN_2:
	case MZ2_WIDOW_RUN_3:
	case MZ2_WIDOW_RUN_4:
	case MZ2_WIDOW_RUN_5:
	case MZ2_WIDOW_RUN_6:
	case MZ2_WIDOW_RUN_7:
	case MZ2_WIDOW_RUN_8:
		CL_SetDlightColour (dl, DL_COLOR_GREEN);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_WIDOW_DISRUPTOR:
		CL_SetDlightColour (dl, DL_COLOR_NEGATIVE);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("weapons/disint2.wav"), 1, ATTN_NORM, 0);
		break;

	case MZ2_WIDOW_PLASMABEAM:
	case MZ2_WIDOW2_BEAMER_1:
	case MZ2_WIDOW2_BEAMER_2:
	case MZ2_WIDOW2_BEAMER_3:
	case MZ2_WIDOW2_BEAMER_4:
	case MZ2_WIDOW2_BEAMER_5:
	case MZ2_WIDOW2_BEAM_SWEEP_1:
	case MZ2_WIDOW2_BEAM_SWEEP_2:
	case MZ2_WIDOW2_BEAM_SWEEP_3:
	case MZ2_WIDOW2_BEAM_SWEEP_4:
	case MZ2_WIDOW2_BEAM_SWEEP_5:
	case MZ2_WIDOW2_BEAM_SWEEP_6:
	case MZ2_WIDOW2_BEAM_SWEEP_7:
	case MZ2_WIDOW2_BEAM_SWEEP_8:
	case MZ2_WIDOW2_BEAM_SWEEP_9:
	case MZ2_WIDOW2_BEAM_SWEEP_10:
	case MZ2_WIDOW2_BEAM_SWEEP_11:
		dl->radius = 300 + (rand () & 100);
		CL_SetDlightColour (dl, DL_COLOR_ORANGE);
		dl->die = cl.time + 200;
		break;
		// ROGUE
		// ======

		// --- Xian's shit ends ---
	}
}


//=============
//=============
void CL_Flashlight (int ent, vec3_t pos)
{
	cdlight_t	*dl = CL_AllocDlight (ent);

	VectorCopy (pos, dl->origin);
	dl->radius = 400;
	dl->minlight = 250;
	dl->die = cl.time + 100;

	CL_SetDlightColour (dl, DL_COLOR_WHITE);
}


/*
======
CL_ColorFlash - flash of light
======
*/
void CL_ColorFlash (vec3_t pos, int ent, int intensity, float r, float g, float b)
{
	cdlight_t	*dl = CL_AllocDlight (ent);

	VectorCopy (pos, dl->origin);
	dl->radius = intensity;
	dl->minlight = 250;
	dl->die = cl.time + 100;

	dl->rgb[0] = r;
	dl->rgb[1] = g;
	dl->rgb[2] = b;
}


/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights (void)
{
	int			i;
	cdlight_t	*dl = cl_dlights;

	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
			continue;
		else
		{
			float radius = dl->radius - ((float) (cl.time - dl->time) * dl->decay * 0.001f);

			// fixed up minlight so it now works as expected
			if (radius > dl->minlight)
				V_AddLight (dl->origin, radius, dl->rgb[0], dl->rgb[1], dl->rgb[2]);
			else dl->die = 0;
		}
	}
}



