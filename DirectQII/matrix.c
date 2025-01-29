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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include "r_local.h"


#define DEG2RAD(a) ((a * M_PI) / 180.0f)


/*
============================================================================================================

MATRIX OPS

These happen in-place on the matrix and update it's current values

============================================================================================================
*/

QMATRIX *R_MatrixIdentity (QMATRIX *m)
{
	return R_MatrixLoadf (m, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
}


QMATRIX *R_MatrixMultiply (QMATRIX *out, QMATRIX *m1, QMATRIX *m2)
{
	return R_MatrixLoadf (
		out,
		(m1->m4x4[0][0] * m2->m4x4[0][0]) + (m1->m4x4[1][0] * m2->m4x4[0][1]) + (m1->m4x4[2][0] * m2->m4x4[0][2]) + (m1->m4x4[3][0] * m2->m4x4[0][3]),
		(m1->m4x4[0][1] * m2->m4x4[0][0]) + (m1->m4x4[1][1] * m2->m4x4[0][1]) + (m1->m4x4[2][1] * m2->m4x4[0][2]) + (m1->m4x4[3][1] * m2->m4x4[0][3]),
		(m1->m4x4[0][2] * m2->m4x4[0][0]) + (m1->m4x4[1][2] * m2->m4x4[0][1]) + (m1->m4x4[2][2] * m2->m4x4[0][2]) + (m1->m4x4[3][2] * m2->m4x4[0][3]),
		(m1->m4x4[0][3] * m2->m4x4[0][0]) + (m1->m4x4[1][3] * m2->m4x4[0][1]) + (m1->m4x4[2][3] * m2->m4x4[0][2]) + (m1->m4x4[3][3] * m2->m4x4[0][3]),
		(m1->m4x4[0][0] * m2->m4x4[1][0]) + (m1->m4x4[1][0] * m2->m4x4[1][1]) + (m1->m4x4[2][0] * m2->m4x4[1][2]) + (m1->m4x4[3][0] * m2->m4x4[1][3]),
		(m1->m4x4[0][1] * m2->m4x4[1][0]) + (m1->m4x4[1][1] * m2->m4x4[1][1]) + (m1->m4x4[2][1] * m2->m4x4[1][2]) + (m1->m4x4[3][1] * m2->m4x4[1][3]),
		(m1->m4x4[0][2] * m2->m4x4[1][0]) + (m1->m4x4[1][2] * m2->m4x4[1][1]) + (m1->m4x4[2][2] * m2->m4x4[1][2]) + (m1->m4x4[3][2] * m2->m4x4[1][3]),
		(m1->m4x4[0][3] * m2->m4x4[1][0]) + (m1->m4x4[1][3] * m2->m4x4[1][1]) + (m1->m4x4[2][3] * m2->m4x4[1][2]) + (m1->m4x4[3][3] * m2->m4x4[1][3]),
		(m1->m4x4[0][0] * m2->m4x4[2][0]) + (m1->m4x4[1][0] * m2->m4x4[2][1]) + (m1->m4x4[2][0] * m2->m4x4[2][2]) + (m1->m4x4[3][0] * m2->m4x4[2][3]),
		(m1->m4x4[0][1] * m2->m4x4[2][0]) + (m1->m4x4[1][1] * m2->m4x4[2][1]) + (m1->m4x4[2][1] * m2->m4x4[2][2]) + (m1->m4x4[3][1] * m2->m4x4[2][3]),
		(m1->m4x4[0][2] * m2->m4x4[2][0]) + (m1->m4x4[1][2] * m2->m4x4[2][1]) + (m1->m4x4[2][2] * m2->m4x4[2][2]) + (m1->m4x4[3][2] * m2->m4x4[2][3]),
		(m1->m4x4[0][3] * m2->m4x4[2][0]) + (m1->m4x4[1][3] * m2->m4x4[2][1]) + (m1->m4x4[2][3] * m2->m4x4[2][2]) + (m1->m4x4[3][3] * m2->m4x4[2][3]),
		(m1->m4x4[0][0] * m2->m4x4[3][0]) + (m1->m4x4[1][0] * m2->m4x4[3][1]) + (m1->m4x4[2][0] * m2->m4x4[3][2]) + (m1->m4x4[3][0] * m2->m4x4[3][3]),
		(m1->m4x4[0][1] * m2->m4x4[3][0]) + (m1->m4x4[1][1] * m2->m4x4[3][1]) + (m1->m4x4[2][1] * m2->m4x4[3][2]) + (m1->m4x4[3][1] * m2->m4x4[3][3]),
		(m1->m4x4[0][2] * m2->m4x4[3][0]) + (m1->m4x4[1][2] * m2->m4x4[3][1]) + (m1->m4x4[2][2] * m2->m4x4[3][2]) + (m1->m4x4[3][2] * m2->m4x4[3][3]),
		(m1->m4x4[0][3] * m2->m4x4[3][0]) + (m1->m4x4[1][3] * m2->m4x4[3][1]) + (m1->m4x4[2][3] * m2->m4x4[3][2]) + (m1->m4x4[3][3] * m2->m4x4[3][3])
	);
}


QMATRIX *R_MatrixOrtho (QMATRIX *m, float l, float r, float b, float t, float zn, float zf)
{
	// Direct3D ortho matrix
	// adapted from https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixorthooffcenterrh
	QMATRIX m2 = {
		2 / (r - l),
		0,
		0,
		0,
		0,
		2 / (t - b),
		0,
		0,
		0,
		0,
		1 / (zn - zf),
		0,
		(l + r) / (l - r),
		(t + b) / (b - t),
		zn / (zn - zf),
		1
	};

	return R_MatrixMultiply (m, m, &m2);
}


QMATRIX *R_MatrixFrustum (QMATRIX *m, float fovx, float fovy, float zn, float zf)
{
	// Direct3D frustum matrix
	// adapted from https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovrh
	QMATRIX m2 = {
		1.0f / tan (DEG2RAD (fovx) * 0.5f),
		0,
		0,
		0,
		0,
		1.0f / tan (DEG2RAD (fovy) * 0.5f),
		0,
		0,
		0,
		0,
		zf / (zn - zf),
		-1,
		0,
		0,
		zn * zf / (zn - zf),
		0
	};

	return R_MatrixMultiply (m, m, &m2);
}


QMATRIX *R_MatrixLoadf (QMATRIX *m, float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44)
{
	m->m4x4[0][0] = _11; m->m4x4[0][1] = _12; m->m4x4[0][2] = _13; m->m4x4[0][3] = _14;
	m->m4x4[1][0] = _21; m->m4x4[1][1] = _22; m->m4x4[1][2] = _23; m->m4x4[1][3] = _24;
	m->m4x4[2][0] = _31; m->m4x4[2][1] = _32; m->m4x4[2][2] = _33; m->m4x4[2][3] = _34;
	m->m4x4[3][0] = _41; m->m4x4[3][1] = _42; m->m4x4[3][2] = _43; m->m4x4[3][3] = _44;

	return m;
}


QMATRIX *R_MatrixLoad (QMATRIX *dst, QMATRIX *src)
{
	memcpy (dst, src, sizeof (QMATRIX));
	return dst;
}


QMATRIX *R_MatrixTranslate (QMATRIX *m, float x, float y, float z)
{
	m->m4x4[3][0] += x * m->m4x4[0][0] + y * m->m4x4[1][0] + z * m->m4x4[2][0];
	m->m4x4[3][1] += x * m->m4x4[0][1] + y * m->m4x4[1][1] + z * m->m4x4[2][1];
	m->m4x4[3][2] += x * m->m4x4[0][2] + y * m->m4x4[1][2] + z * m->m4x4[2][2];
	m->m4x4[3][3] += x * m->m4x4[0][3] + y * m->m4x4[1][3] + z * m->m4x4[2][3];

	return m;
}


QMATRIX *R_MatrixScale (QMATRIX *m, float x, float y, float z)
{
	Vector4Scalef (m->m4x4[0], m->m4x4[0], x);
	Vector4Scalef (m->m4x4[1], m->m4x4[1], y);
	Vector4Scalef (m->m4x4[2], m->m4x4[2], z);

	return m;
}


QMATRIX *R_MatrixRotate (QMATRIX *m, float p, float y, float r)
{
	float sr = sin (DEG2RAD (r));
	float sp = sin (DEG2RAD (p));
	float sy = sin (DEG2RAD (y));
	float cr = cos (DEG2RAD (r));
	float cp = cos (DEG2RAD (p));
	float cy = cos (DEG2RAD (y));

	QMATRIX m1 = {
		(cp * cy),
		(cp * sy),
		-sp,
		0.0f,
		(cr * -sy) + (sr * sp * cy),
		(cr * cy) + (sr * sp * sy),
		(sr * cp),
		0.0f,
		(sr * sy) + (cr * sp * cy),
		(-sr * cy) + (cr * sp * sy),
		(cr * cp),
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	return R_MatrixMultiply (m, m, &m1);
}


QMATRIX *R_MatrixRotateAxis (QMATRIX *m, float angle, float x, float y, float z)
{
	float sa = sin (DEG2RAD (angle));
	float ca = cos (DEG2RAD (angle));

	QMATRIX m1 = {
		(1.0f - ca) * x * x + ca,
		(1.0f - ca) * y * x + sa * z,
		(1.0f - ca) * z * x - sa * y,
		0.0f,
		(1.0f - ca) * x * y - sa * z,
		(1.0f - ca) * y * y + ca,
		(1.0f - ca) * z * y + sa * x,
		0.0f,
		(1.0f - ca) * x * z + sa * y,
		(1.0f - ca) * y * z - sa * x,
		(1.0f - ca) * z * z + ca,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	return R_MatrixMultiply (m, m, &m1);
}


QMATRIX *R_MatrixCamera (QMATRIX *m, const float *origin, const float *angles)
{
	float sr = sin (DEG2RAD (angles[2]));
	float sp = sin (DEG2RAD (angles[0]));
	float sy = sin (DEG2RAD (angles[1]));
	float cr = cos (DEG2RAD (angles[2]));
	float cp = cos (DEG2RAD (angles[0]));
	float cy = cos (DEG2RAD (angles[1]));

	float _11 = -((cr * -sy) + (sr * sp * cy));
	float _21 = -((cr * cy) + (sr * sp * sy));
	float _31 = -(sr * cp);

	float _12 = (sr * sy) + (cr * sp * cy);
	float _22 = (-sr * cy) + (cr * sp * sy);
	float _32 = (cr * cp);

	float _13 = -(cp * cy);
	float _23 = -(cp * sy);
	float _33 = sp;

	QMATRIX m1 = {
		_11, _12, _13, 0.0f,
		_21, _22, _23, 0.0f,
		_31, _32, _33, 0.0f,
		-origin[0] * _11 - origin[1] * _21 - origin[2] * _31,
		-origin[0] * _12 - origin[1] * _22 - origin[2] * _32,
		-origin[0] * _13 - origin[1] * _23 - origin[2] * _33,
		1.0f
	};

	return R_MatrixMultiply (m, m, &m1);
}


float *R_VectorTransform (QMATRIX *m, float *out, float *in)
{
	out[0] = in[0] * m->m4x4[0][0] + in[1] * m->m4x4[1][0] + in[2] * m->m4x4[2][0] + m->m4x4[3][0];
	out[1] = in[1] * m->m4x4[0][1] + in[1] * m->m4x4[1][1] + in[2] * m->m4x4[2][1] + m->m4x4[3][1];
	out[2] = in[2] * m->m4x4[0][2] + in[1] * m->m4x4[1][2] + in[2] * m->m4x4[2][2] + m->m4x4[3][2];

	return out;
}


float *R_VectorInverseTransform (QMATRIX *m, float *out, float *in)
{
	// http://content.gpwiki.org/index.php/MathGem:Fast_Matrix_Inversion (note: dead link)
	out[0] = Vector3Dot (m->m4x4[0], in) - Vector3Dot (m->m4x4[0], m->m4x4[3]);
	out[1] = Vector3Dot (m->m4x4[1], in) - Vector3Dot (m->m4x4[1], m->m4x4[3]);
	out[2] = Vector3Dot (m->m4x4[2], in) - Vector3Dot (m->m4x4[2], m->m4x4[3]);

	return out;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------
// quaternion crap
// weenies love quaternions because they "waste less memory"

enum {
	X, Y, Z, W
};

void Quat_computeW (quat4_t q)
{
	float t = 1.0f - (q[X] * q[X]) - (q[Y] * q[Y]) - (q[Z] * q[Z]);

	if (t < 0.0f)
		q[W] = 0.0f;
	else q[W] = -sqrt (t);
}


void Quat_normalize (quat4_t q)
{
	// compute magnitude of the quaternion
	float mag = sqrt ((q[X] * q[X]) + (q[Y] * q[Y]) + (q[Z] * q[Z]) + (q[W] * q[W]));

	// check for bogus length, to protect against divide by zero
	if (mag > 0.0f)
	{
		// normalize it
		float oneOverMag = 1.0f / mag;

		q[X] *= oneOverMag;
		q[Y] *= oneOverMag;
		q[Z] *= oneOverMag;
		q[W] *= oneOverMag;
	}
}

void Quat_multQuat (const quat4_t qa, const quat4_t qb, quat4_t out)
{
	out[W] = (qa[W] * qb[W]) - (qa[X] * qb[X]) - (qa[Y] * qb[Y]) - (qa[Z] * qb[Z]);
	out[X] = (qa[X] * qb[W]) + (qa[W] * qb[X]) + (qa[Y] * qb[Z]) - (qa[Z] * qb[Y]);
	out[Y] = (qa[Y] * qb[W]) + (qa[W] * qb[Y]) + (qa[Z] * qb[X]) - (qa[X] * qb[Z]);
	out[Z] = (qa[Z] * qb[W]) + (qa[W] * qb[Z]) + (qa[X] * qb[Y]) - (qa[Y] * qb[X]);
}

void Quat_multVec (const quat4_t q, const vec3_t v, quat4_t out)
{
	out[W] = -(q[X] * v[X]) - (q[Y] * v[Y]) - (q[Z] * v[Z]);
	out[X] = (q[W] * v[X]) + (q[Y] * v[Z]) - (q[Z] * v[Y]);
	out[Y] = (q[W] * v[Y]) + (q[Z] * v[X]) - (q[X] * v[Z]);
	out[Z] = (q[W] * v[Z]) + (q[X] * v[Y]) - (q[Y] * v[X]);
}

void Quat_inverse (const quat4_t q, quat4_t inv)
{
	inv[X] = -q[X]; inv[Y] = -q[Y];
	inv[Z] = -q[Z]; inv[W] = q[W];

	Quat_normalize (inv);
}

void Quat_rotatePoint (const quat4_t q, const vec3_t in, vec3_t out)
{
	quat4_t tmp, inv, final;

	Quat_inverse (q, inv);
	Quat_multVec (q, in, tmp);
	Quat_multQuat (tmp, inv, final);

	out[X] = final[X];
	out[Y] = final[Y];
	out[Z] = final[Z];
}


void Quat_inverseRotatePoint (const quat4_t q, const vec3_t in, vec3_t out)
{
	quat4_t tmp, inv, final;

	Quat_inverse (q, inv);
	Quat_multVec (inv, in, tmp);
	Quat_multQuat (tmp, q, final);

	out[X] = final[X];
	out[Y] = final[Y];
	out[Z] = final[Z];
}


/*
==================
Quat_dotProduct

==================
*/
float Quat_dotProduct (const quat4_t qa, const quat4_t qb)
{
	return ((qa[X] * qb[X]) + (qa[Y] * qb[Y]) + (qa[Z] * qb[Z]) + (qa[W] * qb[W]));
}


/*
==================
Quat_slerp

==================
*/
void Quat_slerp (const quat4_t qa, const quat4_t qb, float t, quat4_t out)
{
	// Check for out-of range parameter and return edge points if so
	if (t <= 0.0)
	{
		memcpy (out, qa, sizeof (quat4_t));
		return;
	}

	if (t >= 1.0)
	{
		memcpy (out, qb, sizeof (quat4_t));
		return;
	}

	{
		// Compute "cosine of angle between quaternions" using dot product
		float cosOmega = Quat_dotProduct (qa, qb);

		// If negative dot, use -q1.  Two quaternions q and -q represent the same rotation, but may produce different slerp.  We chose q or -q to rotate using the acute angle.
		float q1w = qb[W];
		float q1x = qb[X];
		float q1y = qb[Y];
		float q1z = qb[Z];

		if (cosOmega < 0.0f)
		{
			q1w = -q1w;
			q1x = -q1x;
			q1y = -q1y;
			q1z = -q1z;
			cosOmega = -cosOmega;
		}

		{
			// Compute interpolation fraction, checking for quaternions almost exactly the same
			float k0, k1;

			if (cosOmega > 0.9999f)
			{
				// Very close - just use linear interpolation, which will protect againt a divide by zero
				k0 = 1.0f - t;
				k1 = t;
			}
			else
			{
				// Compute the sin of the angle using the trig identity sin^2(omega) + cos^2(omega) = 1
				float sinOmega = sqrt (1.0f - (cosOmega * cosOmega));

				// Compute the angle from its sin and cosine
				float omega = atan2 (sinOmega, cosOmega);

				// Compute inverse of denominator, so we only have to divide once
				float oneOverSinOmega = 1.0f / sinOmega;

				// Compute interpolation parameters
				k0 = sin ((1.0f - t) * omega) * oneOverSinOmega;
				k1 = sin (t * omega) * oneOverSinOmega;
			}

			// Interpolate and return new quaternion
			out[W] = (k0 * qa[3]) + (k1 * q1w);
			out[X] = (k0 * qa[0]) + (k1 * q1x);
			out[Y] = (k0 * qa[1]) + (k1 * q1y);
			out[Z] = (k0 * qa[2]) + (k1 * q1z);
		}
	}
}

