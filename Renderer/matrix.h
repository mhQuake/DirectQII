
#ifndef MATRIX_H
#define MATRIX_H

// define early so that it's available to everything
__declspec (align (16)) typedef union _QMATRIX
{
	float m4x4[4][4];
	float m16[16];
} QMATRIX;

// multiply in-place rather than construct a new matrix and leave it at that
QMATRIX *R_MatrixIdentity (QMATRIX *m);
QMATRIX *R_MatrixMultiply (QMATRIX *out, QMATRIX *m1, QMATRIX *m2);
QMATRIX *R_MatrixTranslate (QMATRIX *m, float x, float y, float z);
QMATRIX *R_MatrixScale (QMATRIX *m, float x, float y, float z);
QMATRIX *R_MatrixLoad (QMATRIX *dst, QMATRIX *src);
QMATRIX *R_MatrixOrtho (QMATRIX *m, float left, float right, float bottom, float top, float zNear, float zFar);
QMATRIX *R_MatrixFrustum (QMATRIX *m, float fovx, float fovy, float zn, float zf);
QMATRIX *R_MatrixLoadf (QMATRIX *m, float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44);
QMATRIX *R_MatrixRotate (QMATRIX *m, float p, float y, float r);
QMATRIX *R_MatrixRotateAxis (QMATRIX *m, float angle, float x, float y, float z);
QMATRIX *R_MatrixCamera (QMATRIX *m, const float *origin, const float *angles);

float *R_VectorTransform (QMATRIX *m, float *out, float *in);
float *R_VectorInverseTransform (QMATRIX *m, float *out, float *in);


__inline float SafeSqrt (float in)
{
	if (in > 0)
		return sqrtf (in);
	else return 0;
}

// vector functions from directq
__inline void Vector2Madf (float *out, const float *vec, const float scale, const float *add)
{
	out[0] = (float) ((double) vec[0] * (double) scale + (double) add[0]);
	out[1] = (float) ((double) vec[1] * (double) scale + (double) add[1]);
}


__inline void Vector2Mad (float *out, const float *vec, const float *scale, const float *add)
{
	out[0] = (float) ((double) vec[0] * (double) scale[0] + (double) add[0]);
	out[1] = (float) ((double) vec[1] * (double) scale[1] + (double) add[1]);
}


__inline void Vector3Madf (float *out, const float *vec, const float scale, const float *add)
{
	out[0] = (float) ((double) vec[0] * (double) scale + (double) add[0]);
	out[1] = (float) ((double) vec[1] * (double) scale + (double) add[1]);
	out[2] = (float) ((double) vec[2] * (double) scale + (double) add[2]);
}


__inline void Vector3Mad (float *out, const float *vec, const float *scale, const float *add)
{
	out[0] = (float) ((double) vec[0] * (double) scale[0] + (double) add[0]);
	out[1] = (float) ((double) vec[1] * (double) scale[1] + (double) add[1]);
	out[2] = (float) ((double) vec[2] * (double) scale[2] + (double) add[2]);
}


__inline void Vector4Madf (float *out, const float *vec, const float scale, const float *add)
{
	out[0] = (float) ((double) vec[0] * (double) scale + (double) add[0]);
	out[1] = (float) ((double) vec[1] * (double) scale + (double) add[1]);
	out[2] = (float) ((double) vec[2] * (double) scale + (double) add[2]);
	out[3] = (float) ((double) vec[3] * (double) scale + (double) add[3]);
}


__inline void Vector4Mad (float *out, const float *vec, const float *scale, const float *add)
{
	out[0] = (float) ((double) vec[0] * (double) scale[0] + (double) add[0]);
	out[1] = (float) ((double) vec[1] * (double) scale[1] + (double) add[1]);
	out[2] = (float) ((double) vec[2] * (double) scale[2] + (double) add[2]);
	out[3] = (float) ((double) vec[3] * (double) scale[3] + (double) add[3]);
}


__inline void Vector2Scalef (float *dst, const float *vec, const float scale)
{
	dst[0] = (float) ((double) vec[0] * (double) scale);
	dst[1] = (float) ((double) vec[1] * (double) scale);
}


__inline void Vector2Scale (float *dst, const float *vec, const float *scale)
{
	dst[0] = (float) ((double) vec[0] * (double) scale[0]);
	dst[1] = (float) ((double) vec[1] * (double) scale[1]);
}


__inline void Vector3Scalef (float *dst, const float *vec, const float scale)
{
	dst[0] = (float) ((double) vec[0] * (double) scale);
	dst[1] = (float) ((double) vec[1] * (double) scale);
	dst[2] = (float) ((double) vec[2] * (double) scale);
}


__inline void Vector3Scale (float *dst, const float *vec, const float *scale)
{
	dst[0] = (float) ((double) vec[0] * (double) scale[0]);
	dst[1] = (float) ((double) vec[1] * (double) scale[1]);
	dst[2] = (float) ((double) vec[2] * (double) scale[2]);
}


__inline void Vector4Scalef (float *dst, const float *vec, const float scale)
{
	dst[0] = (float) ((double) vec[0] * (double) scale);
	dst[1] = (float) ((double) vec[1] * (double) scale);
	dst[2] = (float) ((double) vec[2] * (double) scale);
	dst[3] = (float) ((double) vec[3] * (double) scale);
}


__inline void Vector4Scale (float *dst, const float *vec, const float *scale)
{
	dst[0] = (float) ((double) vec[0] * (double) scale[0]);
	dst[1] = (float) ((double) vec[1] * (double) scale[1]);
	dst[2] = (float) ((double) vec[2] * (double) scale[2]);
	dst[3] = (float) ((double) vec[3] * (double) scale[3]);
}


__inline void Vector2Recipf (float *dst, const float *vec, const float scale)
{
	dst[0] = (float) ((double) vec[0] / (double) scale);
	dst[1] = (float) ((double) vec[1] / (double) scale);
}


__inline void Vector2Recip (float *dst, const float *vec, const float *scale)
{
	dst[0] = (float) ((double) vec[0] / (double) scale[0]);
	dst[1] = (float) ((double) vec[1] / (double) scale[1]);
}


__inline void Vector3Recipf (float *dst, const float *vec, const float scale)
{
	dst[0] = (float) ((double) vec[0] / (double) scale);
	dst[1] = (float) ((double) vec[1] / (double) scale);
	dst[2] = (float) ((double) vec[2] / (double) scale);
}


__inline void Vector3Recip (float *dst, const float *vec, const float *scale)
{
	dst[0] = (float) ((double) vec[0] / (double) scale[0]);
	dst[1] = (float) ((double) vec[1] / (double) scale[1]);
	dst[2] = (float) ((double) vec[2] / (double) scale[2]);
}


__inline void Vector4Recipf (float *dst, const float *vec, const float scale)
{
	dst[0] = (float) ((double) vec[0] / (double) scale);
	dst[1] = (float) ((double) vec[1] / (double) scale);
	dst[2] = (float) ((double) vec[2] / (double) scale);
	dst[3] = (float) ((double) vec[3] / (double) scale);
}


__inline void Vector4Recip (float *dst, const float *vec, const float *scale)
{
	dst[0] = (float) ((double) vec[0] / (double) scale[0]);
	dst[1] = (float) ((double) vec[1] / (double) scale[1]);
	dst[2] = (float) ((double) vec[2] / (double) scale[2]);
	dst[3] = (float) ((double) vec[3] / (double) scale[3]);
}


__inline void Vector2Copy (float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
}


__inline void Vector3Copy (float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}


__inline void Vector4Copy (float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}


__inline void Vector2Addf (float *dst, const float *vec1, const float add)
{
	dst[0] = (float) ((double) vec1[0] + (double) add);
	dst[1] = (float) ((double) vec1[1] + (double) add);
}


__inline void Vector2Add (float *dst, const float *vec1, const float *vec2)
{
	dst[0] = (float) ((double) vec1[0] + (double) vec2[0]);
	dst[1] = (float) ((double) vec1[1] + (double) vec2[1]);
}


__inline void Vector2Subtractf (float *dst, const float *vec1, const float sub)
{
	dst[0] = (float) ((double) vec1[0] - (double) sub);
	dst[1] = (float) ((double) vec1[1] - (double) sub);
}


__inline void Vector2Subtract (float *dst, const float *vec1, const float *vec2)
{
	dst[0] = (float) ((double) vec1[0] - (double) vec2[0]);
	dst[1] = (float) ((double) vec1[1] - (double) vec2[1]);
}


__inline void Vector3Addf (float *dst, const float *vec1, const float add)
{
	dst[0] = (float) ((double) vec1[0] + (double) add);
	dst[1] = (float) ((double) vec1[1] + (double) add);
	dst[2] = (float) ((double) vec1[2] + (double) add);
}


__inline void Vector3Add (float *dst, const float *vec1, const float *vec2)
{
	dst[0] = (float) ((double) vec1[0] + (double) vec2[0]);
	dst[1] = (float) ((double) vec1[1] + (double) vec2[1]);
	dst[2] = (float) ((double) vec1[2] + (double) vec2[2]);
}


__inline void Vector3Subtractf (float *dst, const float *vec1, const float sub)
{
	dst[0] = (float) ((double) vec1[0] - (double) sub);
	dst[1] = (float) ((double) vec1[1] - (double) sub);
	dst[2] = (float) ((double) vec1[2] - (double) sub);
}


__inline void Vector3Subtract (float *dst, const float *vec1, const float *vec2)
{
	dst[0] = (float) ((double) vec1[0] - (double) vec2[0]);
	dst[1] = (float) ((double) vec1[1] - (double) vec2[1]);
	dst[2] = (float) ((double) vec1[2] - (double) vec2[2]);
}


__inline void Vector4Addf (float *dst, const float *vec1, const float add)
{
	dst[0] = (float) ((double) vec1[0] + (double) add);
	dst[1] = (float) ((double) vec1[1] + (double) add);
	dst[2] = (float) ((double) vec1[2] + (double) add);
	dst[3] = (float) ((double) vec1[3] + (double) add);
}


__inline void Vector4Add (float *dst, const float *vec1, const float *vec2)
{
	dst[0] = (float) ((double) vec1[0] + (double) vec2[0]);
	dst[1] = (float) ((double) vec1[1] + (double) vec2[1]);
	dst[2] = (float) ((double) vec1[2] + (double) vec2[2]);
	dst[3] = (float) ((double) vec1[3] + (double) vec2[3]);
}


__inline void Vector4Subtractf (float *dst, const float *vec1, const float sub)
{
	dst[0] = (float) ((double) vec1[0] - (double) sub);
	dst[1] = (float) ((double) vec1[1] - (double) sub);
	dst[2] = (float) ((double) vec1[2] - (double) sub);
	dst[3] = (float) ((double) vec1[3] - (double) sub);
}


__inline void Vector4Subtract (float *dst, const float *vec1, const float *vec2)
{
	dst[0] = (float) ((double) vec1[0] - (double) vec2[0]);
	dst[1] = (float) ((double) vec1[1] - (double) vec2[1]);
	dst[2] = (float) ((double) vec1[2] - (double) vec2[2]);
	dst[3] = (float) ((double) vec1[3] - (double) vec2[3]);
}


__inline float Vector2Dot (const float *x, const float *y)
{
	return (float) (((double) x[0] * (double) y[0]) + ((double) x[1] * (double) y[1]));
}


__inline float Vector3Dot (const float *x, const float *y)
{
	// fix up math optimizations that screw things up in Quake
	return (float) (((double) x[0] * (double) y[0]) + ((double) x[1] * (double) y[1]) + ((double) x[2] * (double) y[2]));
}


__inline float Vector4Dot (const float *x, const float *y)
{
	return (float) (((double) x[0] * (double) y[0]) + ((double) x[1] * (double) y[1]) + ((double) x[2] * (double) y[2]) + ((double) x[3] * (double) y[3]));
}


__inline void Vector2Lerpf (float *dst, const float *l1, const float *l2, const float b)
{
	dst[0] = (float) ((double) l1[0] + ((double) l2[0] - (double) l1[0]) * (double) b);
	dst[1] = (float) ((double) l1[1] + ((double) l2[1] - (double) l1[1]) * (double) b);
}


__inline void Vector3Lerpf (float *dst, const float *l1, const float *l2, const float b)
{
	dst[0] = (float) ((double) l1[0] + ((double) l2[0] - (double) l1[0]) * (double) b);
	dst[1] = (float) ((double) l1[1] + ((double) l2[1] - (double) l1[1]) * (double) b);
	dst[2] = (float) ((double) l1[2] + ((double) l2[2] - (double) l1[2]) * (double) b);
}


__inline void Vector4Lerpf (float *dst, const float *l1, const float *l2, const float b)
{
	dst[0] = (float) ((double) l1[0] + ((double) l2[0] - (double) l1[0]) * (double) b);
	dst[1] = (float) ((double) l1[1] + ((double) l2[1] - (double) l1[1]) * (double) b);
	dst[2] = (float) ((double) l1[2] + ((double) l2[2] - (double) l1[2]) * (double) b);
	dst[3] = (float) ((double) l1[3] + ((double) l2[3] - (double) l1[3]) * (double) b);
}


__inline void Vector2Lerp (float *dst, const float *l1, const float *l2, const float *b)
{
	dst[0] = (float) ((double) l1[0] + ((double) l2[0] - (double) l1[0]) * (double) b[0]);
	dst[1] = (float) ((double) l1[1] + ((double) l2[1] - (double) l1[1]) * (double) b[1]);
}


__inline void Vector3Lerp (float *dst, const float *l1, const float *l2, const float *b)
{
	dst[0] = (float) ((double) l1[0] + ((double) l2[0] - (double) l1[0]) * (double) b[0]);
	dst[1] = (float) ((double) l1[1] + ((double) l2[1] - (double) l1[1]) * (double) b[1]);
	dst[2] = (float) ((double) l1[2] + ((double) l2[2] - (double) l1[2]) * (double) b[2]);
}


__inline void Vector4Lerp (float *dst, const float *l1, const float *l2, const float *b)
{
	dst[0] = (float) ((double) l1[0] + ((double) l2[0] - (double) l1[0]) * (double) b[0]);
	dst[1] = (float) ((double) l1[1] + ((double) l2[1] - (double) l1[1]) * (double) b[1]);
	dst[2] = (float) ((double) l1[2] + ((double) l2[2] - (double) l1[2]) * (double) b[2]);
	dst[3] = (float) ((double) l1[3] + ((double) l2[3] - (double) l1[3]) * (double) b[3]);
}


__inline void Vector2Set (float *vec, const float x, const float y)
{
	vec[0] = x;
	vec[1] = y;
}


__inline void Vector3Set (float *vec, const float x, const float y, const float z)
{
	vec[0] = x;
	vec[1] = y;
	vec[2] = z;
}


__inline void Vector4Set (float *vec, const float x, const float y, const float z, const float w)
{
	vec[0] = x;
	vec[1] = y;
	vec[2] = z;
	vec[3] = w;
}


__inline void Vector2Clear (float *vec)
{
	vec[0] = vec[1] = 0.0f;
}


__inline void Vector3Clear (float *vec)
{
	vec[0] = vec[1] = vec[2] = 0.0f;
}


__inline void Vector4Clear (float *vec)
{
	vec[0] = vec[1] = vec[2] = vec[3] = 0.0f;
}


__inline void Vector2Clamp (float *vec, const float clmp)
{
	if (vec[0] > clmp) vec[0] = clmp;
	if (vec[1] > clmp) vec[1] = clmp;
}


__inline void Vector3Clamp (float *vec, const float clmp)
{
	if (vec[0] > clmp) vec[0] = clmp;
	if (vec[1] > clmp) vec[1] = clmp;
	if (vec[2] > clmp) vec[2] = clmp;
}


__inline void Vector4Clamp (float *vec, const float clmp)
{
	if (vec[0] > clmp) vec[0] = clmp;
	if (vec[1] > clmp) vec[1] = clmp;
	if (vec[2] > clmp) vec[2] = clmp;
	if (vec[3] > clmp) vec[3] = clmp;
}


__inline void Vector2Cross (float *cross, const float *v1, const float *v2)
{
	// Sys_Error ("Just what do you think you're doing, Dave?");
}


__inline void Vector3Cross (float *cross, const float *v1, const float *v2)
{
	cross[0] = (float) ((double) v1[1] * (double) v2[2] - (double) v1[2] * (double) v2[1]);
	cross[1] = (float) ((double) v1[2] * (double) v2[0] - (double) v1[0] * (double) v2[2]);
	cross[2] = (float) ((double) v1[0] * (double) v2[1] - (double) v1[1] * (double) v2[0]);
}


__inline void Vector4Cross (float *cross, const float *v1, const float *v2)
{
	// Sys_Error ("Just what do you think you're doing, Dave?");
}


__inline float Vector2Length (const float *v)
{
	return SafeSqrt (Vector2Dot (v, v));
}


__inline float Vector3Length (const float *v)
{
	return SafeSqrt (Vector3Dot (v, v));
}


__inline float Vector4Length (const float *v)
{
	return SafeSqrt (Vector4Dot (v, v));
}


__inline float Vector2Normalize (float *v)
{
	double length = (double) Vector2Dot (v, v);

	if ((length = (double) SafeSqrt ((float) length)) > 0)
	{
		double ilength = 1.0 / length;

		v[0] = (float) ((double) v[0] * ilength);
		v[1] = (float) ((double) v[1] * ilength);
	}

	return (float) length;
}


__inline float Vector3Normalize (float *v)
{
	double length = (double) Vector3Dot (v, v);

	if ((length = (double) SafeSqrt ((float) length)) > 0)
	{
		double ilength = 1.0 / length;

		v[0] = (float) ((double) v[0] * ilength);
		v[1] = (float) ((double) v[1] * ilength);
		v[2] = (float) ((double) v[2] * ilength);
	}

	return (float) length;
}


__inline float Vector4Normalize (float *v)
{
	double length = (double) Vector4Dot (v, v);

	if ((length = (double) SafeSqrt ((float) length)) > 0)
	{
		double ilength = 1.0 / length;

		v[0] = (float) ((double) v[0] * ilength);
		v[1] = (float) ((double) v[1] * ilength);
		v[2] = (float) ((double) v[2] * ilength);
		v[3] = (float) ((double) v[3] * ilength);
	}

	return (float) length;
}


__inline qboolean Vector2Compare (const float *v1, const float *v2)
{
	if (v1[0] != v2[0]) return false;
	if (v1[1] != v2[1]) return false;

	return true;
}


__inline qboolean Vector3Compare (const float *v1, const float *v2)
{
	if (v1[0] != v2[0]) return false;
	if (v1[1] != v2[1]) return false;
	if (v1[2] != v2[2]) return false;

	return true;
}


__inline qboolean Vector4Compare (const float *v1, const float *v2)
{
	if (v1[0] != v2[0]) return false;
	if (v1[1] != v2[1]) return false;
	if (v1[2] != v2[2]) return false;
	if (v1[3] != v2[3]) return false;

	return true;
}


__inline float Vector2Dist (const float *v1, const float *v2)
{
	float dist[2];

	Vector2Subtract (dist, v1, v2);

	return Vector2Length (dist);
}


__inline float Vector3Dist (const float *v1, const float *v2)
{
	float dist[3];

	Vector3Subtract (dist, v1, v2);

	return Vector3Length (dist);
}


__inline float Vector4Dist (const float *v1, const float *v2)
{
	float dist[4];

	Vector4Subtract (dist, v1, v2);

	return Vector4Length (dist);
}


__inline float Vector2SquaredDist (const float *v1, const float *v2)
{
	float dist[2];

	Vector2Subtract (dist, v1, v2);

	return Vector2Dot (dist, dist);
}


__inline float Vector3SquaredDist (const float *v1, const float *v2)
{
	float dist[3];

	Vector3Subtract (dist, v1, v2);

	return Vector3Dot (dist, dist);
}


__inline float Vector4SquaredDist (const float *v1, const float *v2)
{
	float dist[4];

	Vector4Subtract (dist, v1, v2);

	return Vector4Dot (dist, dist);
}


__inline void Vector2Inverse (float *v)
{
	v[0] = -v[0];
	v[1] = -v[1];
}


__inline void Vector3Inverse (float *v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}


__inline void Vector4Inverse (float *v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
	v[3] = -v[3];
}

#endif
