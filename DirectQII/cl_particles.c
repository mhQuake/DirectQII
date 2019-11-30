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
// fixme - this stuff is gone totally out of hand

#include "client.h"

static vec3_t avelocities[NUMVERTEXNORMALS] = {
	{2.55f, 2.49f, 1.48f}, {2.20f, 1.76f, 0.97f}, {0.55f, 1.65f, 1.19f}, {2.24f, 1.23f, 2.10f}, {0.69f, 1.84f, 1.12f}, {1.94f, 0.75f, 1.00f}, {1.13f, 2.43f, 0.96f}, {1.12f, 1.68f, 1.16f}, {1.63f, 1.42f, 2.13f},
	{1.57f, 0.98f, 2.31f}, {0.74f, 2.53f, 0.59f}, {1.75f, 0.58f, 1.48f}, {1.29f, 2.52f, 0.52f}, {0.66f, 2.26f, 2.16f}, {2.35f, 1.28f, 0.70f}, {1.48f, 0.56f, 0.78f}, {2.49f, 0.96f, 0.98f}, {1.08f, 0.13f, 0.25f},
	{1.56f, 2.05f, 2.50f}, {0.98f, 2.07f, 0.73f}, {1.36f, 2.46f, 1.54f}, {1.46f, 2.39f, 0.61f}, {1.52f, 1.05f, 0.60f}, {0.29f, 2.25f, 0.18f}, {2.46f, 0.18f, 2.11f}, {0.05f, 0.02f, 1.29f}, {0.18f, 2.30f, 1.96f},
	{0.73f, 2.05f, 1.83f}, {1.35f, 1.56f, 0.67f}, {0.77f, 0.31f, 0.13f}, {1.34f, 0.00f, 2.04f}, {2.33f, 1.27f, 1.67f}, {0.98f, 2.03f, 0.51f}, {1.96f, 1.67f, 1.07f}, {0.68f, 2.51f, 2.53f}, {1.83f, 1.87f, 2.01f},
	{0.65f, 0.37f, 0.04f}, {1.14f, 2.42f, 0.71f}, {0.58f, 0.59f, 1.50f}, {1.60f, 1.66f, 2.18f}, {0.89f, 1.82f, 1.30f}, {0.43f, 2.48f, 1.04f}, {0.02f, 0.36f, 2.21f}, {1.30f, 1.49f, 0.11f}, {1.40f, 1.43f, 1.04f},
	{2.30f, 0.61f, 2.38f}, {0.43f, 0.52f, 2.02f}, {0.40f, 0.25f, 2.18f}, {2.04f, 1.29f, 0.50f}, {0.96f, 0.01f, 2.05f}, {2.03f, 0.93f, 2.42f}, {1.33f, 0.73f, 0.48f}, {2.10f, 0.26f, 0.42f}, {2.41f, 1.68f, 1.37f},
	{0.69f, 1.68f, 1.77f}, {0.48f, 0.82f, 1.88f}, {2.55f, 2.24f, 1.72f}, {1.74f, 1.03f, 0.51f}, {0.86f, 0.06f, 1.54f}, {0.32f, 0.60f, 1.42f}, {1.46f, 2.04f, 1.88f}, {1.91f, 0.48f, 1.63f}, {1.01f, 1.21f, 0.35f},
	{1.68f, 2.55f, 2.47f}, {0.11f, 0.09f, 2.03f}, {0.01f, 2.22f, 2.47f}, {0.12f, 0.99f, 1.39f}, {1.80f, 1.97f, 1.59f}, {2.22f, 2.38f, 1.97f}, {2.23f, 1.64f, 0.75f}, {2.25f, 0.45f, 0.15f}, {0.69f, 1.29f, 2.30f},
	{0.92f, 0.56f, 0.94f}, {1.08f, 1.78f, 1.85f}, {1.37f, 0.28f, 0.78f}, {0.42f, 1.76f, 1.28f}, {2.21f, 1.39f, 1.57f}, {1.97f, 0.41f, 1.47f}, {2.55f, 1.11f, 2.48f}, {0.00f, 1.09f, 0.75f}, {0.49f, 0.43f, 1.68f},
	{1.65f, 0.65f, 2.23f}, {0.32f, 1.88f, 1.89f}, {1.07f, 1.86f, 1.15f}, {0.73f, 2.46f, 2.09f}, {0.73f, 1.84f, 0.19f}, {1.23f, 1.39f, 1.17f}, {1.50f, 0.13f, 2.01f}, {0.33f, 1.65f, 2.25f}, {1.34f, 1.25f, 0.18f},
	{0.86f, 0.51f, 1.39f}, {2.32f, 0.25f, 0.95f}, {2.40f, 0.67f, 1.63f}, {0.40f, 1.57f, 1.01f}, {0.03f, 2.45f, 1.74f}, {0.07f, 2.22f, 1.03f}, {1.46f, 2.37f, 1.80f}, {1.40f, 1.09f, 1.89f}, {1.00f, 0.86f, 1.86f},
	{0.17f, 2.43f, 0.76f}, {0.53f, 2.03f, 1.37f}, {1.28f, 2.48f, 1.69f}, {1.88f, 1.93f, 2.28f}, {0.25f, 0.17f, 0.17f}, {0.66f, 0.78f, 1.92f}, {0.90f, 1.70f, 1.47f}, {1.84f, 0.92f, 0.36f}, {2.23f, 2.27f, 2.34f},
	{1.42f, 1.70f, 1.71f}, {0.86f, 0.62f, 0.00f}, {1.89f, 0.42f, 2.32f}, {2.12f, 0.13f, 0.24f}, {1.93f, 1.50f, 0.26f}, {0.52f, 2.28f, 0.08f}, {1.15f, 0.97f, 2.45f}, {0.77f, 1.22f, 0.09f}, {0.05f, 0.66f, 1.04f},
	{0.18f, 1.82f, 0.18f}, {1.45f, 0.13f, 1.60f}, {0.05f, 2.34f, 1.76f}, {2.43f, 1.53f, 2.00f}, {1.34f, 0.93f, 2.12f}, {2.34f, 2.29f, 0.46f}, {2.16f, 1.86f, 0.33f}, {1.86f, 1.84f, 2.08f}, {2.55f, 2.54f, 1.38f},
	{1.84f, 1.08f, 1.35f}, {1.90f, 2.45f, 1.11f}, {1.44f, 1.74f, 0.69f}, {0.93f, 1.07f, 1.17f}, {0.39f, 0.76f, 0.34f}, {0.05f, 1.55f, 2.01f}, {0.93f, 1.16f, 0.41f}, {2.06f, 1.34f, 1.51f}, {0.36f, 2.38f, 0.44f},
	{2.10f, 0.48f, 2.11f}, {1.65f, 0.46f, 0.02f}, {2.34f, 2.48f, 1.72f}, {1.03f, 0.93f, 2.50f}, {2.18f, 1.36f, 0.66f}, {1.82f, 1.63f, 1.08f}, {0.18f, 0.18f, 1.07f}, {1.90f, 0.92f, 1.76f}, {0.92f, 2.09f, 1.86f},
	{0.19f, 2.50f, 2.25f}, {0.05f, 0.03f, 2.54f}, {1.56f, 1.97f, 0.50f}, {0.97f, 2.24f, 2.35f}, {0.31f, 1.83f, 1.99f}, {1.95f, 0.59f, 1.90f}, {2.25f, 0.36f, 0.75f}, {0.55f, 1.92f, 2.34f}, {0.13f, 2.44f, 1.86f},
	{1.65f, 0.30f, 0.51f}, {2.34f, 1.47f, 0.27f}, {0.42f, 1.20f, 2.04f}, {0.06f, 2.27f, 0.26f}, {1.89f, 2.36f, 0.74f}, {2.19f, 2.15f, 1.97f}, {1.33f, 0.28f, 1.36f}, {1.56f, 1.31f, 2.16f}, {1.07f, 2.40f, 2.16f}
};


/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

cparticle_t	*active_particles, *free_particles;
cparticle_t	particles[MAX_PARTICLES];


/*
===============
CL_ClearParticles
===============
*/
void CL_ClearParticles (void)
{
	int		i;

	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0; i < MAX_PARTICLES; i++)
	{
		particles[i].next = &particles[i + 1];
		particles[i].size = 6 + (rand () & 7);
	}

	particles[MAX_PARTICLES - 1].next = NULL;
}


cparticle_t *CL_GetParticle (void)
{
	if (!free_particles)
		return NULL;
	else
	{
		// get a new particle
		cparticle_t *p = free_particles;

		// link it in
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		// save off creation time for it
		p->time = cl.time;

		// and done
		return p;
	}
}


/*
===============
CL_ParticleEffect

Wall impact puffs
===============
*/
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = color + (rand () & 7);
		d = rand () & 31;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = crand () * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
}


/*
===============
CL_ParticleEffect2
===============
*/
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = color;
		d = rand () & 7;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = crand () * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
}


// RAFAEL
/*
===============
CL_ParticleEffect3
===============
*/
void CL_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = color;
		d = rand () & 7;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = crand () * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
}

/*
===============
CL_TeleporterParticles
===============
*/
void CL_TeleporterParticles (entity_state_t *ent)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 8; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = 0xdb;

		for (j = 0; j < 2; j++)
		{
			p->org[j] = ent->origin[j] - 16 + (rand () & 31);
			p->vel[j] = crand () * 14;
		}

		p->org[2] = ent->origin[2] - 8 + (rand () & 7);
		p->vel[2] = 80 + (rand () & 7);

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -0.5;
	}
}


/*
===============
CL_LogoutEffect

===============
*/
void CL_LogoutEffect (vec3_t org, int type)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 500; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		if (type == MZ_LOGIN)
			p->color = 0xd0 + (rand () & 7);	// green
		else if (type == MZ_LOGOUT)
			p->color = 0x40 + (rand () & 7);	// red
		else
			p->color = 0xe0 + (rand () & 7);	// yellow

		p->org[0] = org[0] - 16 + frand () * 32;
		p->org[1] = org[1] - 16 + frand () * 32;
		p->org[2] = org[2] - 24 + frand () * 56;

		for (j = 0; j < 3; j++)
			p->vel[j] = crand () * 20;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1.0 + frand () * 0.3);
	}
}


/*
===============
CL_ItemRespawnParticles

===============
*/
void CL_ItemRespawnParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 64; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = 0xd4 + (rand () & 3);	// green

		p->org[0] = org[0] + crand () * 8;
		p->org[1] = org[1] + crand () * 8;
		p->org[2] = org[2] + crand () * 8;

		for (j = 0; j < 3; j++)
			p->vel[j] = crand () * 8;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY * 0.2;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1.0 + frand () * 0.3);
	}
}


/*
===============
CL_ExplosionParticles
===============
*/
void CL_ExplosionParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 256; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = 0xe0 + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () % 32) - 16);
			p->vel[j] = (rand () % 384) - 192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -0.8 / (0.5 + frand () * 0.3);
	}
}


/*
===============
CL_BigTeleportParticles
===============
*/
void CL_BigTeleportParticles (vec3_t org)
{
	int			i;
	cparticle_t	*p;
	float		angle, dist;
	static int colortable[4] = {2 * 8, 13 * 8, 21 * 8, 18 * 8};

	for (i = 0; i < 4096; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = colortable[rand () & 3];

		angle = M_PI * 2 * (rand () & 1023) / 1023.0;
		dist = rand () & 31;

		p->org[0] = org[0] + cos (angle) * dist;
		p->vel[0] = cos (angle) * (70 + (rand () & 63));
		p->accel[0] = -cos (angle) * 100;

		p->org[1] = org[1] + sin (angle) * dist;
		p->vel[1] = sin (angle) * (70 + (rand () & 63));
		p->accel[1] = -sin (angle) * 100;

		p->org[2] = org[2] + 8 + (rand () % 90);
		p->vel[2] = -100 + (rand () & 31);
		p->accel[2] = PARTICLE_GRAVITY * 4;

		p->alpha = 1.0;
		p->alphavel = -0.3 / (0.5 + frand () * 0.3);
	}
}


/*
===============
CL_BlasterParticles

Wall impact puffs
===============
*/
void CL_BlasterParticles (vec3_t org, vec3_t dir)
{
	int			i, j;
	cparticle_t	*p;
	float		d;
	int			count;

	count = 40;
	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = 0xe0 + (rand () & 7);
		d = rand () & 15;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * 30 + crand () * 40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
}


/*
===============
CL_BlasterTrail

===============
*/
void CL_BlasterTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.3 + frand () * 0.2);
		p->color = 0xe0;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand ();
			p->vel[j] = crand () * 5;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_QuadTrail

===============
*/
void CL_QuadTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.8 + frand () * 0.2);
		p->color = 115;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 16;
			p->vel[j] = crand () * 5;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_FlagTrail

===============
*/
void CL_FlagTrail (vec3_t start, vec3_t end, float color)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.8 + frand () * 0.2);
		p->color = color;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 16;
			p->vel[j] = crand () * 5;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_DiminishingTrail

===============
*/
void CL_DiminishingTrail (vec3_t start, vec3_t end, centity_t *old, int flags)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;
	float		orgscale;
	float		velscale;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 0.5;
	VectorScale (vec, dec, vec);

	if (old->trailcount > 900)
	{
		orgscale = 4;
		velscale = 15;
	}
	else if (old->trailcount > 800)
	{
		orgscale = 2;
		velscale = 10;
	}
	else
	{
		orgscale = 1;
		velscale = 5;
	}

	while (len > 0)
	{
		len -= dec;

		// drop less particles as it flies
		if ((rand () & 1023) < old->trailcount)
		{
			if ((p = CL_GetParticle ()) == NULL) return;

			VectorClear (p->accel);

			if (flags & EF_GIB)
			{
				p->alpha = 1.0;
				p->alphavel = -1.0 / (1 + frand () * 0.4);
				p->color = 0xe8 + (rand () & 7);
				for (j = 0; j < 3; j++)
				{
					p->org[j] = move[j] + crand () * orgscale;
					p->vel[j] = crand () * velscale;
					p->accel[j] = 0;
				}
				p->vel[2] -= PARTICLE_GRAVITY;
			}
			else if (flags & EF_GREENGIB)
			{
				p->alpha = 1.0;
				p->alphavel = -1.0 / (1 + frand () * 0.4);
				p->color = 0xdb + (rand () & 7);
				for (j = 0; j < 3; j++)
				{
					p->org[j] = move[j] + crand () * orgscale;
					p->vel[j] = crand () * velscale;
					p->accel[j] = 0;
				}
				p->vel[2] -= PARTICLE_GRAVITY;
			}
			else
			{
				p->alpha = 1.0;
				p->alphavel = -1.0 / (1 + frand () * 0.2);
				p->color = 4 + (rand () & 7);
				for (j = 0; j < 3; j++)
				{
					p->org[j] = move[j] + crand () * orgscale;
					p->vel[j] = crand () * velscale;
				}
				p->accel[2] = 20;
			}
		}

		old->trailcount -= 5;
		if (old->trailcount < 100)
			old->trailcount = 100;
		VectorAdd (move, vec, move);
	}
}

void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up)
{
	float		d;

	// this rotate and negat guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

/*
===============
CL_RocketTrail

===============
*/
void CL_RocketTrail (vec3_t start, vec3_t end, centity_t *old)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;

	// smoke
	CL_DiminishingTrail (start, end, old, EF_ROCKET);

	// fire
	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 1;
	VectorScale (vec, dec, vec);

	while (len > 0)
	{
		len -= dec;

		if ((rand () & 7) == 0)
		{
			if ((p = CL_GetParticle ()) == NULL) return;

			VectorClear (p->accel);

			p->alpha = 1.0;
			p->alphavel = -1.0 / (1 + frand () * 0.2);
			p->color = 0xdc + (rand () & 3);

			for (j = 0; j < 3; j++)
			{
				p->org[j] = move[j] + crand () * 5;
				p->vel[j] = crand () * 20;
			}

			p->accel[2] = -PARTICLE_GRAVITY;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_RailTrail

===============
*/
void CL_RailTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;
	vec3_t		right, up;
	int			i;
	float		d, c, s;
	vec3_t		dir;
	byte		clr = 0x74;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	MakeNormalVectors (vec, right, up);

	for (i = 0; i < len; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		d = i * 0.1;
		c = cos (d);
		s = sin (d);

		VectorScale (right, c, dir);
		VectorMA (dir, s, up, dir);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.2);
		p->color = clr + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + dir[j] * 3;
			p->vel[j] = dir[j] * 6;
		}

		VectorAdd (move, vec, move);
	}

	dec = 0.75;
	VectorScale (vec, dec, vec);
	VectorCopy (start, move);

	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.6 + frand () * 0.2);
		p->color = 0x0 + rand () & 15;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 3;
			p->vel[j] = crand () * 3;
		}

		VectorAdd (move, vec, move);
	}
}

// RAFAEL
/*
===============
CL_IonripperTrail
===============
*/
void CL_IonripperTrail (vec3_t start, vec3_t ent)
{
	vec3_t	move;
	vec3_t	vec;
	float	len;
	int		j;
	cparticle_t *p;
	int		dec;
	int     left = 0;

	VectorCopy (start, move);
	VectorSubtract (ent, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 0.5;
		p->alphavel = -1.0 / (0.3 + frand () * 0.2);
		p->color = 0xe4 + (rand () & 3);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j];
			p->accel[j] = 0;
		}

		if (left)
		{
			left = 0;
			p->vel[0] = 10;
		}
		else
		{
			left = 1;
			p->vel[0] = -10;
		}

		p->vel[1] = 0;
		p->vel[2] = 0;

		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_BubbleTrail

===============
*/
void CL_BubbleTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			i, j;
	cparticle_t	*p;
	float		dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 32;
	VectorScale (vec, dec, vec);

	for (i = 0; i < len; i += dec)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.2);
		p->color = 4 + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 2;
			p->vel[j] = crand () * 5;
		}

		p->vel[2] += 6;

		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_FlyParticles
===============
*/

#define	BEAMLENGTH			16
void CL_FlyParticles (vec3_t origin, int count)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist = 64;
	float		ltime = (float) cl.time / 1000.0;

	for (i = 0; i < count; i += 2)
	{
		// allow > NUMVERTEXNORMALS flies
		float *avel = avelocities[i % NUMVERTEXNORMALS];
		float *bdir = bytedirs[i % NUMVERTEXNORMALS];

		angle = (ltime + i) * avel[0];
		sy = sin (angle);
		cy = cos (angle);

		angle = (ltime + i) * avel[1];
		sp = sin (angle);
		cp = cos (angle);

		angle = (ltime + i) * avel[2];
		sr = sin (angle);
		cr = cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if ((p = CL_GetParticle ()) == NULL) return;

		dist = sin (ltime + i) * 64;

		p->org[0] = origin[0] + bdir[0] * dist + forward[0] * BEAMLENGTH;
		p->org[1] = origin[1] + bdir[1] * dist + forward[1] * BEAMLENGTH;
		p->org[2] = origin[2] + bdir[2] * dist + forward[2] * BEAMLENGTH;

		VectorClear (p->vel);
		VectorClear (p->accel);

		p->color = 0;
		p->alpha = 1;
		p->alphavel = -100;
	}
}

void CL_FlyEffect (centity_t *ent, vec3_t origin)
{
	int		n;
	int		count;
	int		starttime;

	if (ent->fly_stoptime < cl.time)
	{
		starttime = cl.time;
		ent->fly_stoptime = cl.time + 60000;
	}
	else
		starttime = ent->fly_stoptime - 60000;

	if ((n = cl.time - starttime) < 20000)
		count = n * NUMVERTEXNORMALS / 20000.0;
	else
	{
		if ((n = ent->fly_stoptime - cl.time) < 20000)
			count = n * NUMVERTEXNORMALS / 20000.0;
		else count = NUMVERTEXNORMALS;
	}

	CL_FlyParticles (origin, count);
}


/*
===============
CL_BfgParticles
===============
*/

#define	BEAMLENGTH			16
void CL_BfgParticles (entity_t *ent)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist = 64;
	vec3_t		v;
	float		ltime = (float) cl.time / 1000.0;

	for (i = 0; i < NUMVERTEXNORMALS; i++)
	{
		angle = ltime * avelocities[i][0];
		sy = sin (angle);
		cy = cos (angle);

		angle = ltime * avelocities[i][1];
		sp = sin (angle);
		cp = cos (angle);

		angle = ltime * avelocities[i][2];
		sr = sin (angle);
		cr = cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if ((p = CL_GetParticle ()) == NULL) return;

		dist = sin (ltime + i) * 64;

		p->org[0] = ent->currorigin[0] + bytedirs[i][0] * dist + forward[0] * BEAMLENGTH;
		p->org[1] = ent->currorigin[1] + bytedirs[i][1] * dist + forward[1] * BEAMLENGTH;
		p->org[2] = ent->currorigin[2] + bytedirs[i][2] * dist + forward[2] * BEAMLENGTH;

		VectorClear (p->vel);
		VectorClear (p->accel);

		VectorSubtract (p->org, ent->currorigin, v);
		dist = VectorLength (v) / 90.0;

		p->color = floor (0xd0 + dist * 7);
		p->alpha = 1.0 - dist;
		p->alphavel = -100;
	}
}


/*
===============
CL_TrapParticles
===============
*/
// RAFAEL
void CL_TrapParticles (entity_t *ent)
{
	vec3_t		move;
	vec3_t		vec;
	vec3_t		start, end;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	ent->currorigin[2] -= 14;
	VectorCopy (ent->currorigin, start);
	VectorCopy (ent->currorigin, end);
	end[2] += 64;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.3 + frand () * 0.2);
		p->color = 0xe0;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand ();
			p->vel[j] = crand () * 15;
		}

		p->accel[2] = PARTICLE_GRAVITY;

		VectorAdd (move, vec, move);
	}

	{
		int			i, j, k;
		cparticle_t	*p;
		float		vel;
		vec3_t		dir;
		vec3_t		org;

		ent->currorigin[2] += 14;
		VectorCopy (ent->currorigin, org);

		for (i = -2; i <= 2; i += 4)
		{
			for (j = -2; j <= 2; j += 4)
			{
				for (k = -2; k <= 4; k += 4)
				{
					if ((p = CL_GetParticle ()) == NULL) return;

					p->color = 0xe0 + (rand () & 3);
					p->alpha = 1.0;
					p->alphavel = -1.0 / (0.3 + (rand () & 7) * 0.02);

					p->org[0] = org[0] + i + ((rand () & 23) * crand ());
					p->org[1] = org[1] + j + ((rand () & 23) * crand ());
					p->org[2] = org[2] + k + ((rand () & 23) * crand ());

					dir[0] = j * 8;
					dir[1] = i * 8;
					dir[2] = k * 8;

					VectorNormalize (dir);
					vel = 50 + rand () & 63;
					VectorScale (dir, vel, p->vel);

					p->accel[0] = p->accel[1] = 0;
					p->accel[2] = -PARTICLE_GRAVITY;
				}
			}
		}
	}
}


/*
===============
CL_BFGExplosionParticles
===============
*/
//FIXME combined with CL_ExplosionParticles
void CL_BFGExplosionParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 256; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = 0xd0 + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () % 32) - 16);
			p->vel[j] = (rand () % 384) - 192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;

		p->alpha = 1.0;
		p->alphavel = -0.8 / (0.5 + frand () * 0.3);
	}
}


/*
===============
CL_TeleportParticles

===============
*/
void CL_TeleportParticles (vec3_t org)
{
	int			i, j, k;
	cparticle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i = -16; i <= 16; i += 4)
	{
		for (j = -16; j <= 16; j += 4)
		{
			for (k = -16; k <= 32; k += 4)
			{
				if ((p = CL_GetParticle ()) == NULL) return;

				p->color = 7 + (rand () & 7);

				p->alpha = 1.0;
				p->alphavel = -1.0 / (0.3 + (rand () & 7) * 0.02);

				p->org[0] = org[0] + i + (rand () & 3);
				p->org[1] = org[1] + j + (rand () & 3);
				p->org[2] = org[2] + k + (rand () & 3);

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, p->vel);

				p->accel[0] = p->accel[1] = 0;
				p->accel[2] = -PARTICLE_GRAVITY;
			}
		}
	}
}


/*
======
vectoangles2 - this is duplicated in the game DLL, but I need it here.
======
*/
void vectoangles2 (vec3_t value1, vec3_t angles)
{
	float	forward;
	float	yaw, pitch;

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		// PMM - fixed to correct for pitch of 0
		if (value1[0])
			yaw = (atan2 (value1[1], value1[0]) * 180 / M_PI);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0] * value1[0] + value1[1] * value1[1]);
		pitch = (atan2 (value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[0] = -pitch;
	angles[1] = yaw;
	angles[2] = 0;
}


/*
======
CL_DebugTrail
======
*/
void CL_DebugTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	//	int			j;
	cparticle_t	*p;
	float		dec;
	vec3_t		right, up;
	//	int			i;
	//	float		d, c, s;
	//	vec3_t		dir;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	MakeNormalVectors (vec, right, up);

	//	VectorScale(vec, RT2_SKIP, vec);

	//	dec = 1.0;
	//	dec = 0.75;
	dec = 3;
	VectorScale (vec, dec, vec);
	VectorCopy (start, move);

	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);
		VectorClear (p->vel);

		p->alpha = 1.0;
		p->alphavel = -0.1;
		p->color = 0x74 + (rand () & 7);

		VectorCopy (move, p->org);
		VectorAdd (move, vec, move);
	}

}

/*
===============
CL_SmokeTrail
===============
*/
void CL_SmokeTrail (vec3_t start, vec3_t end, int colorStart, int colorRun, int spacing)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, spacing, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= spacing;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.5);
		p->color = colorStart + (rand () % colorRun);

		for (j = 0; j < 3; j++)
			p->org[j] = move[j] + crand () * 3;

		p->vel[2] = 20 + crand () * 5;
		VectorAdd (move, vec, move);
	}
}

void CL_ForceWall (vec3_t start, vec3_t end, int color)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorScale (vec, 4, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= 4;

		if (frand () > 0.3)
		{
			if ((p = CL_GetParticle ()) == NULL) return;

			VectorClear (p->accel);

			p->alpha = 1.0;
			p->alphavel = -1.0 / (3.0 + frand () * 0.5);
			p->color = color;

			for (j = 0; j < 3; j++)
				p->org[j] = move[j] + crand () * 3;

			p->vel[0] = 0;
			p->vel[1] = 0;
			p->vel[2] = -40 - (crand () * 10);
		}

		VectorAdd (move, vec, move);
	}
}

void CL_FlameEffects (centity_t *ent, vec3_t origin)
{
	int			n, count;
	int			j;
	cparticle_t	*p;

	count = rand () & 0xF;

	for (n = 0; n < count; n++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.2);
		p->color = 226 + (rand () % 4);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = origin[j] + crand () * 5;
			p->vel[j] = crand () * 5;
		}

		p->vel[2] = crand () * -10;
		p->accel[2] = -PARTICLE_GRAVITY;
	}

	count = rand () & 0x7;

	for (n = 0; n < count; n++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.5);
		p->color = 0 + (rand () % 4);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = origin[j] + crand () * 3;
		}

		p->vel[2] = 20 + crand () * 5;
	}
}


/*
===============
CL_GenericParticleEffect
===============
*/
void CL_GenericParticleEffect (vec3_t org, vec3_t dir, int color, int count, int numcolors, int dirspread, float alphavel)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		if (numcolors > 1)
			p->color = color + (rand () & numcolors);
		else
			p->color = color;

		d = rand () & dirspread;
		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = crand () * 20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		//		VectorCopy (accel, p->accel);
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * alphavel);
		//		p->alphavel = alphavel;
	}
}

/*
===============
CL_BubbleTrail2 (lets you control the # of bubbles by setting the distance between the spawns)

===============
*/
void CL_BubbleTrail2 (vec3_t start, vec3_t end, int dist)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			i, j;
	cparticle_t	*p;
	float		dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = dist;
	VectorScale (vec, dec, vec);

	for (i = 0; i < len; i += dec)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1 + frand () * 0.1);
		p->color = 4 + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 2;
			p->vel[j] = crand () * 10;
		}

		p->org[2] -= 4;
		p->vel[2] += 20;

		VectorAdd (move, vec, move);
	}
}


void CL_Heatbeam (vec3_t start, vec3_t forward)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	vec3_t		right, up;
	int			i;
	float		c, s;
	vec3_t		dir;
	float		ltime;
	float		step = 32.0, rstep;
	float		start_pt;
	float		rot;
	float		variance;
	vec3_t		end;

	VectorMA (start, 4096, forward, end);

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	// FIXME - pmm - these might end up using old values?
	//	MakeNormalVectors (vec, right, up);
	VectorCopy (cl.v_right, right);
	VectorCopy (cl.v_up, up);

	VectorMA (move, -0.5, right, move);
	VectorMA (move, -0.5, up, move);

	ltime = (float) cl.time / 1000.0;
	start_pt = fmod (ltime * 96.0, step);
	VectorMA (move, start_pt, vec, move);

	VectorScale (vec, step, vec);

	//	Com_Printf ("%f\n", ltime);
	rstep = M_PI / 10.0;

	for (i = start_pt; i < len; i += step)
	{
		if (i > step * 5) // don't bother after the 5th ring
			break;

		for (rot = 0; rot < M_PI * 2; rot += rstep)
		{
			if ((p = CL_GetParticle ()) == NULL) return;

			VectorClear (p->accel);
			//			rot+= fmod(ltime, 12.0) * M_PI;
			//			c = cos(rot)/2.0;
			//			s = sin(rot)/2.0;
			//			variance = 0.4 + ((float)rand()/(float)RAND_MAX) *0.2;
			variance = 0.5;
			c = cos (rot) * variance;
			s = sin (rot) * variance;

			// trim it so it looks like it's starting at the origin
			if (i < 10)
			{
				VectorScale (right, c * (i / 10.0), dir);
				VectorMA (dir, s * (i / 10.0), up, dir);
			}
			else
			{
				VectorScale (right, c, dir);
				VectorMA (dir, s, up, dir);
			}

			p->alpha = 0.5;
			//		p->alphavel = -1.0 / (1 + frand () * 0.2);
			p->alphavel = -1000.0;
			//		p->color = 0x74 + (rand () & 7);
			p->color = 223 - (rand () & 7);

			for (j = 0; j < 3; j++)
			{
				p->org[j] = move[j] + dir[j] * 3;
				//			p->vel[j] = dir[j] * 6;
				p->vel[j] = 0;
			}
		}
		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_ParticleSteamEffect

Puffs with velocity along direction, with some randomness thrown in
===============
*/
void CL_ParticleSteamEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude)
{
	int			i, j;
	cparticle_t	*p;
	float		d;
	vec3_t		r, u;

	//	vectoangles2 (dir, angle_dir);
	//	AngleVectors (angle_dir, f, r, u);

	MakeNormalVectors (dir, r, u);

	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = color + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + magnitude * 0.1 * crand ();
			//			p->vel[j] = dir[j] * magnitude;
		}

		VectorScale (dir, magnitude, p->vel);
		d = crand () * magnitude / 3;
		VectorMA (p->vel, d, r, p->vel);
		d = crand () * magnitude / 3;
		VectorMA (p->vel, d, u, p->vel);

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY / 2;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
}


void CL_ParticleSteamEffect2 (cl_sustain_t *self)
//vec3_t org, vec3_t dir, int color, int count, int magnitude)
{
	int			i, j;
	cparticle_t	*p;
	float		d;
	vec3_t		r, u;
	vec3_t		dir;

	//	vectoangles2 (dir, angle_dir);
	//	AngleVectors (angle_dir, f, r, u);

	VectorCopy (self->dir, dir);
	MakeNormalVectors (dir, r, u);

	for (i = 0; i < self->count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = self->color + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = self->org[j] + self->magnitude * 0.1 * crand ();
			//			p->vel[j] = dir[j] * magnitude;
		}

		VectorScale (dir, self->magnitude, p->vel);
		d = crand () * self->magnitude / 3;
		VectorMA (p->vel, d, r, p->vel);
		d = crand () * self->magnitude / 3;
		VectorMA (p->vel, d, u, p->vel);

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY / 2;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
	self->nextthink += self->thinkinterval;
}

/*
===============
CL_TrackerTrail
===============
*/
void CL_TrackerTrail (vec3_t start, vec3_t end, int particleColor)
{
	vec3_t		move;
	vec3_t		vec;
	vec3_t		forward, right, up, angle_dir;
	float		len;
	cparticle_t	*p;
	int			dec;
	float		dist;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	VectorCopy (vec, forward);
	vectoangles2 (forward, angle_dir);
	AngleVectors (angle_dir, forward, right, up);

	dec = 3;
	VectorScale (vec, 3, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -2.0;
		p->color = particleColor;
		dist = DotProduct (move, forward);
		VectorMA (move, 8 * cos (dist), up, p->org);

		p->vel[0] = p->vel[1] = 0;
		p->vel[2] = 5;

		VectorAdd (move, vec, move);
	}
}

void CL_Tracker_Shell (vec3_t origin)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;

	for (i = 0; i < 300; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = INSTANT_PARTICLE;
		p->color = 0;

		dir[0] = crand ();
		dir[1] = crand ();
		dir[2] = crand ();
		VectorNormalize (dir);

		VectorMA (origin, 40, dir, p->org);
	}
}

void CL_MonsterPlasma_Shell (vec3_t origin)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;

	for (i = 0; i < 40; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = INSTANT_PARTICLE;
		p->color = 0xe0;

		dir[0] = crand ();
		dir[1] = crand ();
		dir[2] = crand ();
		VectorNormalize (dir);

		VectorMA (origin, 10, dir, p->org);
		//		VectorMA (origin, 10 * (((rand () & 0x7fff) / ((float) 0x7fff))), dir, p->org);
	}
}

void CL_Widowbeamout (cl_sustain_t *self)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;
	static int colortable[4] = {2 * 8, 13 * 8, 21 * 8, 18 * 8};
	float			ratio;

	ratio = 1.0 - (((float) self->endtime - (float) cl.time) / 2100.0);

	for (i = 0; i < 300; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = INSTANT_PARTICLE;
		p->color = colortable[rand () & 3];

		dir[0] = crand ();
		dir[1] = crand ();
		dir[2] = crand ();
		VectorNormalize (dir);

		VectorMA (self->org, (45.0 * ratio), dir, p->org);
		//		VectorMA(origin, 10 * (((rand () & 0x7fff) / ((float) 0x7fff))), dir, p->org);
	}
}

void CL_Nukeblast (cl_sustain_t *self)
{
	vec3_t			dir;
	int				i;
	cparticle_t		*p;
	static int colortable[4] = {110, 112, 114, 116};
	float			ratio;

	ratio = 1.0 - (((float) self->endtime - (float) cl.time) / 1000.0);

	for (i = 0; i < 700; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = INSTANT_PARTICLE;
		p->color = colortable[rand () & 3];

		dir[0] = crand ();
		dir[1] = crand ();
		dir[2] = crand ();
		VectorNormalize (dir);

		VectorMA (self->org, (200.0 * ratio), dir, p->org);
		//		VectorMA(origin, 10 * (((rand () & 0x7fff) / ((float) 0x7fff))), dir, p->org);
	}
}

void CL_WidowSplash (vec3_t org)
{
	static int colortable[4] = {2 * 8, 13 * 8, 21 * 8, 18 * 8};
	int			i;
	cparticle_t	*p;
	vec3_t		dir;

	for (i = 0; i < 256; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = colortable[rand () & 3];

		dir[0] = crand ();
		dir[1] = crand ();
		dir[2] = crand ();
		VectorNormalize (dir);
		VectorMA (org, 45.0, dir, p->org);
		VectorMA (vec3_origin, 40.0, dir, p->vel);

		p->accel[0] = p->accel[1] = 0;
		p->alpha = 1.0;

		p->alphavel = -0.8 / (0.5 + frand () * 0.3);
	}
}


void CL_Tracker_Explode (vec3_t	origin)
{
	vec3_t			dir, backdir;
	int				i;
	cparticle_t		*p;

	for (i = 0; i < 300; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0;
		p->color = 0;

		dir[0] = crand ();
		dir[1] = crand ();
		dir[2] = crand ();
		VectorNormalize (dir);
		VectorScale (dir, -1, backdir);

		VectorMA (origin, 64, dir, p->org);
		VectorScale (backdir, 64, p->vel);
	}
}


/*
===============
CL_TagTrail

===============
*/
void CL_TagTrail (vec3_t start, vec3_t end, float color)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len >= 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.8 + frand () * 0.2);
		p->color = color;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand () * 16;
			p->vel[j] = crand () * 5;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_ColorExplosionParticles
===============
*/
void CL_ColorExplosionParticles (vec3_t org, int color, int run)
{
	int			i, j;
	cparticle_t	*p;

	for (i = 0; i < 128; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = color + (rand () % run);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () % 32) - 16);
			p->vel[j] = (rand () % 256) - 128;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -0.4 / (0.6 + frand () * 0.2);
	}
}

/*
===============
CL_ParticleSmokeEffect - like the steam effect, but unaffected by gravity
===============
*/
void CL_ParticleSmokeEffect (vec3_t org, vec3_t dir, int color, int count, int magnitude)
{
	int			i, j;
	cparticle_t	*p;
	float		d;
	vec3_t		r, u;

	MakeNormalVectors (dir, r, u);

	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = color + (rand () & 7);

		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + magnitude * 0.1 * crand ();
			//			p->vel[j] = dir[j] * magnitude;
		}
		VectorScale (dir, magnitude, p->vel);
		d = crand () * magnitude / 3;
		VectorMA (p->vel, d, r, p->vel);
		d = crand () * magnitude / 3;
		VectorMA (p->vel, d, u, p->vel);

		p->accel[0] = p->accel[1] = p->accel[2] = 0;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
}

/*
===============
CL_BlasterParticles2

Wall impact puffs (Green)
===============
*/
void CL_BlasterParticles2 (vec3_t org, vec3_t dir, unsigned int color)
{
	int			i, j;
	cparticle_t	*p;
	float		d;
	int			count;

	count = 40;
	for (i = 0; i < count; i++)
	{
		if ((p = CL_GetParticle ()) == NULL) return;

		p->color = color + (rand () & 7);

		d = rand () & 15;
		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand () & 7) - 4) + d * dir[j];
			p->vel[j] = dir[j] * 30 + crand () * 40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand () * 0.3);
	}
}

/*
===============
CL_BlasterTrail2

Green!
===============
*/
void CL_BlasterTrail2 (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if ((p = CL_GetParticle ()) == NULL) return;

		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.3 + frand () * 0.2);
		p->color = 0xd0;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = move[j] + crand ();
			p->vel[j] = crand () * 5;
		}

		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_AddParticles
===============
*/
void CL_AddParticles (void)
{
	cparticle_t		*p, *next;
	float			alpha;

	cparticle_t *active = NULL;
	cparticle_t *tail = NULL;

	for (p = active_particles; p; p = next)
	{
		// moved this outside the condition so that it's valid for moving the particle below
		float time = (cl.time - p->time) * 0.001;

		next = p->next;

		// PMM - added INSTANT_PARTICLE handling for heat beam
		if (p->alphavel != INSTANT_PARTICLE)
		{
			// this needs to run on the CPU so that we can correctly remove faded-out particles
			alpha = p->alpha + time * p->alphavel;

			if (alpha <= 0)
			{
				// faded out
				p->next = free_particles;
				free_particles = p;
				continue;
			}
		}
		else alpha = p->alpha;

		p->next = NULL;

		if (!tail)
			active = tail = p;
		else
		{
			tail->next = p;
			tail = p;
		}

		// particle movement is now done on the GPU
		V_AddParticle (p->org, p->vel, p->accel, time, p->color, alpha, p->size);

		// PMM - INSTANT_PARTICLE only lasts for 1 frame and dies immediately after
		if (p->alphavel == INSTANT_PARTICLE)
		{
			p->alphavel = 0.0;
			p->alpha = 0.0;
		}
	}

	active_particles = active;
}


