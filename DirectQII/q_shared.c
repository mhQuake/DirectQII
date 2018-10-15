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

#include "q_shared.h"

#define DEG2RAD(a) (a * M_PI) / 180.0F

vec3_t vec3_origin = {0, 0, 0};

//============================================================================

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float angle;
	static float sr, sp, sy, cr, cp, cy; // static to help MS compiler fp bugs

	angle = angles[1] * (M_PI * 2 / 360);
	sy = sin (angle);
	cy = cos (angle);
	angle = angles[0] * (M_PI * 2 / 360);
	sp = sin (angle);
	cp = cos (angle);
	angle = angles[2] * (M_PI * 2 / 360);
	sr = sin (angle);
	cr = cos (angle);

	if (forward)
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if (right)
	{
		right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
		right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
		right[2] = -1 * sr * cp;
	}

	if (up)
	{
		up[0] = (cr * sp * cy + -sr * -sy);
		up[1] = (cr * sp * sy + -sr * cy);
		up[2] = cr * cp;
	}
}


//============================================================================


float Q_fabs (float f)
{
#if 0
	if (f >= 0)
		return f;
	return -f;
#else
	int tmp = *(int *) &f;
	tmp &= 0x7FFFFFFF;
	return *(float *) &tmp;
#endif
}

#if defined _M_IX86 && !defined C_ONLY
#pragma warning (disable:4035)
__declspec(naked) long Q_ftol (float f)
{
	static int tmp;
	__asm fld dword ptr[esp + 4]
		__asm fistp tmp
	__asm mov eax, tmp
	__asm ret
}
#pragma warning (default:4035)
#endif

/*
===============
LerpAngle

===============
*/
float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;
	if (a1 - a2 < -180)
		a1 += 360;
	return a2 + frac * (a1 - a2);
}


float anglemod (float a)
{
	return (360.0 / 65536) * ((int) (a * (65536 / 360.0)) & 65535);
}


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
#pragma warning( disable: 4035 )

__declspec(naked) int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx

			cmp bops_initialized, 1
			je  initialized
			mov bops_initialized, 1

			mov Ljmptab[0 * 4], offset Lcase0
			mov Ljmptab[1 * 4], offset Lcase1
			mov Ljmptab[2 * 4], offset Lcase2
			mov Ljmptab[3 * 4], offset Lcase3
			mov Ljmptab[4 * 4], offset Lcase4
			mov Ljmptab[5 * 4], offset Lcase5
			mov Ljmptab[6 * 4], offset Lcase6
			mov Ljmptab[7 * 4], offset Lcase7

initialized :

		mov edx, ds : dword ptr[4 + 12 + esp]
			mov ecx, ds : dword ptr[4 + 4 + esp]
			xor eax, eax
			mov ebx, ds : dword ptr[4 + 8 + esp]
			mov al, ds : byte ptr[17 + edx]
			cmp al, 8
			jge Lerror
			fld ds : dword ptr[0 + edx]
			fld st (0)
			jmp dword ptr[Ljmptab + eax * 4]
Lcase0 :
	   fmul ds : dword ptr[ebx]
	   fld ds : dword ptr[0 + 4 + edx]
	   fxch st (2)
	   fmul ds : dword ptr[ecx]
	   fxch st (2)
	   fld st (0)
	   fmul ds : dword ptr[4 + ebx]
	   fld ds : dword ptr[0 + 8 + edx]
	   fxch st (2)
	   fmul ds : dword ptr[4 + ecx]
	   fxch st (2)
	   fld st (0)
	   fmul ds : dword ptr[8 + ebx]
	   fxch st (5)
	   faddp st (3), st (0)
	   fmul ds : dword ptr[8 + ecx]
	   fxch st (1)
	   faddp st (3), st (0)
	   fxch st (3)
	   faddp st (2), st (0)
	   jmp LSetSides
Lcase1 :
		fmul ds : dword ptr[ecx]
			fld ds : dword ptr[0 + 4 + edx]
			fxch st (2)
			fmul ds : dword ptr[ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[4 + ebx]
			fld ds : dword ptr[0 + 8 + edx]
			fxch st (2)
			fmul ds : dword ptr[4 + ecx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[8 + ebx]
			fxch st (5)
			faddp st (3), st (0)
			fmul ds : dword ptr[8 + ecx]
			fxch st (1)
			faddp st (3), st (0)
			fxch st (3)
			faddp st (2), st (0)
			jmp LSetSides
Lcase2 :
		fmul ds : dword ptr[ebx]
			fld ds : dword ptr[0 + 4 + edx]
			fxch st (2)
			fmul ds : dword ptr[ecx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[4 + ecx]
			fld ds : dword ptr[0 + 8 + edx]
			fxch st (2)
			fmul ds : dword ptr[4 + ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[8 + ebx]
			fxch st (5)
			faddp st (3), st (0)
			fmul ds : dword ptr[8 + ecx]
			fxch st (1)
			faddp st (3), st (0)
			fxch st (3)
			faddp st (2), st (0)
			jmp LSetSides
Lcase3 :
		fmul ds : dword ptr[ecx]
			fld ds : dword ptr[0 + 4 + edx]
			fxch st (2)
			fmul ds : dword ptr[ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[4 + ecx]
			fld ds : dword ptr[0 + 8 + edx]
			fxch st (2)
			fmul ds : dword ptr[4 + ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[8 + ebx]
			fxch st (5)
			faddp st (3), st (0)
			fmul ds : dword ptr[8 + ecx]
			fxch st (1)
			faddp st (3), st (0)
			fxch st (3)
			faddp st (2), st (0)
			jmp LSetSides
Lcase4 :
		fmul ds : dword ptr[ebx]
			fld ds : dword ptr[0 + 4 + edx]
			fxch st (2)
			fmul ds : dword ptr[ecx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[4 + ebx]
			fld ds : dword ptr[0 + 8 + edx]
			fxch st (2)
			fmul ds : dword ptr[4 + ecx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[8 + ecx]
			fxch st (5)
			faddp st (3), st (0)
			fmul ds : dword ptr[8 + ebx]
			fxch st (1)
			faddp st (3), st (0)
			fxch st (3)
			faddp st (2), st (0)
			jmp LSetSides
Lcase5 :
		fmul ds : dword ptr[ecx]
			fld ds : dword ptr[0 + 4 + edx]
			fxch st (2)
			fmul ds : dword ptr[ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[4 + ebx]
			fld ds : dword ptr[0 + 8 + edx]
			fxch st (2)
			fmul ds : dword ptr[4 + ecx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[8 + ecx]
			fxch st (5)
			faddp st (3), st (0)
			fmul ds : dword ptr[8 + ebx]
			fxch st (1)
			faddp st (3), st (0)
			fxch st (3)
			faddp st (2), st (0)
			jmp LSetSides
Lcase6 :
		fmul ds : dword ptr[ebx]
			fld ds : dword ptr[0 + 4 + edx]
			fxch st (2)
			fmul ds : dword ptr[ecx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[4 + ecx]
			fld ds : dword ptr[0 + 8 + edx]
			fxch st (2)
			fmul ds : dword ptr[4 + ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[8 + ecx]
			fxch st (5)
			faddp st (3), st (0)
			fmul ds : dword ptr[8 + ebx]
			fxch st (1)
			faddp st (3), st (0)
			fxch st (3)
			faddp st (2), st (0)
			jmp LSetSides
Lcase7 :
		fmul ds : dword ptr[ecx]
			fld ds : dword ptr[0 + 4 + edx]
			fxch st (2)
			fmul ds : dword ptr[ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[4 + ecx]
			fld ds : dword ptr[0 + 8 + edx]
			fxch st (2)
			fmul ds : dword ptr[4 + ebx]
			fxch st (2)
			fld st (0)
			fmul ds : dword ptr[8 + ecx]
			fxch st (5)
			faddp st (3), st (0)
			fmul ds : dword ptr[8 + ebx]
			fxch st (1)
			faddp st (3), st (0)
			fxch st (3)
			faddp st (2), st (0)
LSetSides :
		  faddp st (2), st (0)
		  fcomp ds : dword ptr[12 + edx]
		  xor ecx, ecx
		  fnstsw ax
		  fcomp ds : dword ptr[12 + edx]
		  and ah, 1
		  xor ah, 1
		  add cl, ah
		  fnstsw ax
		  and ah, 1
		  add ah, ah
		  add cl, ah
		  pop ebx
		  mov eax, ecx
		  ret
Lerror :
		int 3
	}
}
#pragma warning( default: 4035 )


int VectorCompare (vec3_t v1, vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
		return 0;

	return 1;
}


vec_t VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;

}

vec_t VectorNormalize2 (vec3_t v, vec3_t out)
{
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}

	return length;

}

void VectorMA (vec3_t add, float scale, vec3_t mult, vec3_t out)
{
	out[0] = add[0] + scale * mult[0];
	out[1] = add[1] + scale * mult[1];
	out[2] = add[2] + scale * mult[2];
}


vec_t DotProduct (vec3_t v1, vec3_t v2)
{
	return (float) ((double) v1[0] * (double) v2[0] + (double) v1[1] * (double) v2[1] + (double) v1[2] * (double) v2[2]);
}

void VectorNegate (vec3_t in, vec3_t out)
{
	out[0] = -in[0];
	out[1] = -in[1];
	out[2] = -in[2];
}

void VectorSet (vec3_t v, float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

void VectorClear (vec3_t v)
{
	v[0] = v[1] = v[2] = 0;
}

void VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

void VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

void VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

double sqrt (double x);

vec_t VectorLength (vec3_t v)
{
	int		i;
	float	length;

	length = 0;
	for (i = 0; i < 3; i++)
		length += v[i] * v[i];
	length = sqrt (length);		// FIXME

	return length;
}

void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}


int Q_log2 (int val)
{
	int answer = 0;
	while (val >>= 1)
		answer++;
	return answer;
}



//====================================================================================

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int		i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i = 0; i < 7 && *in; i++, in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *s, *s2;

	s = in + strlen (in) - 1;

	while (s != in && *s != '.')
		s--;

	for (s2 = s; s2 != in && *s2 != '/'; s2--)
		;

	if (s - s2 < 2)
		out[0] = 0;
	else
	{
		s--;
		strncpy (out, s2 + 1, s - s2);
		out[s - s2] = 0;
	}
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath (char *in, char *out)
{
	char *s;

	s = in + strlen (in) - 1;

	while (s != in && *s != '/')
		s--;

	strncpy (out, in, s - in);
	out[s - in] = 0;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char    *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + strlen (path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}

/*
============================================================================

BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean	bigendien;

// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places
short (*_BigShort) (short l);
short (*_LittleShort) (short l);
int (*_BigLong) (int l);
int (*_LittleLong) (int l);
float (*_BigFloat) (float l);
float (*_LittleFloat) (float l);

short	BigShort (short l) { return _BigShort (l); }
short	LittleShort (short l) { return _LittleShort (l); }
int		BigLong (int l) { return _BigLong (l); }
int		LittleLong (int l) { return _LittleLong (l); }
float	BigFloat (float l) { return _BigFloat (l); }
float	LittleFloat (float l) { return _LittleFloat (l); }

short   ShortSwap (short l)
{
	byte    b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

short	ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	byte    b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

int	LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
	byte	swaptest[2] = {1, 0};

	// set the byte swapping variables in a portable manner	
	if (*(short *) swaptest == 1)
	{
		bigendien = false;
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}
}



/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va (char *format, ...)
{
#define VA_NUM_BUFFS 64 // this is 64k of memory but we want to replace all varargs funcs with routing through va so that's OK
	static char va_buffers[VA_NUM_BUFFS][4096]; // because Con_Printf is now routed through here this needs to be increased to the size of MAXPRINTMSG
	static int buffer_idx = 0;

	char *string = va_buffers[buffer_idx];
	va_list argptr;

	// go to the next buffer
	buffer_idx = (buffer_idx + 1) & (VA_NUM_BUFFS - 1);

	// and do the varargs stuff
	va_start (argptr, format);
	vsprintf (string, format, argptr);
	va_end (argptr);

	return string;
}


char com_token[MAX_TOKEN_CHARS];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char **data_p)
{
	int		c;
	int		len;
	char	*data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

	// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
		//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
============================================================================

LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

// FIXME: replace all Q_stricmp with Q_strcasecmp
int Q_stricmp (char *s1, char *s2)
{
#if defined(WIN32)
	return _stricmp (s1, s2);
#else
	return strcasecmp (s1, s2);
#endif
}


int Q_strncasecmp (char *s1, char *s2, int n)
{
	int		c1, c2;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
	} while (c1);

	return 0;		// strings are equal
}

int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}



void Com_sprintf (char *dest, int size, char *fmt, ...)
{
	int		len;
	va_list		argptr;
	char	bigbuffer[0x10000];

	va_start (argptr, fmt);
	len = vsprintf (bigbuffer, fmt, argptr);
	va_end (argptr);
	if (len >= size)
		Com_Printf ("Com_sprintf: overflow of %i in %i\n", len, size);
	strncpy (dest, bigbuffer, size - 1);
}

/*
=====================================================================

INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares
	// work without stomping on each other
	static	int	valueindex;
	char	*o;

	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey))
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
		//		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey))
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate (char *s)
{
	if (strstr (s, "\""))
		return false;
	if (strstr (s, ";"))
		return false;
	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;
	int		maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\"))
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";"))
	{
		Com_Printf ("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\""))
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen (key) > MAX_INFO_KEY - 1 || strlen (value) > MAX_INFO_KEY - 1)
	{
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen (value))
		return;

	Com_sprintf (newi, sizeof (newi), "\\%s\\%s", key, value);

	if (strlen (newi) + strlen (s) > maxsize)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen (s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
}

//====================================================================


