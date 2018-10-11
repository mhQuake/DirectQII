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

// draw.c

#include "r_local.h"


typedef struct drawpolyvert_s {
	float position[2];
	float texcoord[2];
	union {
		DWORD color;
		byte rgba[4];
		float slice;
	};
} drawpolyvert_t;


// these should be sized such that MAX_DRAW_INDEXES = (MAX_DRAW_VERTS / 4) * 6
#define MAX_DRAW_VERTS		0x400
#define MAX_DRAW_INDEXES	0x600

#if (MAX_DRAW_VERTS / 4) * 6 != MAX_DRAW_INDEXES
#error (MAX_DRAW_VERTS / 4) * 6 != MAX_DRAW_INDEXES
#endif


static drawpolyvert_t *d_drawverts = NULL;
static int d_firstdrawvert = 0;
static int d_numdrawverts = 0;

static ID3D11Buffer *d3d_DrawVertexes = NULL;
static ID3D11Buffer *d3d_DrawIndexes = NULL;


#define STAT_MINUS		10	// num frame for '-' stats digit

static image_t	*draw_chars;
static image_t	*sb_nums[2];

static int d3d_DrawTexturedShader;
static int d3d_DrawFinalizeShader;
static int d3d_DrawCinematicShader;
static int d3d_DrawColouredShader;
static int d3d_DrawTexArrayShader;
static int d3d_DrawFadescreenShader;


static ID3D11Buffer *d3d_DrawConstants = NULL;
static ID3D11Buffer *d3d_CineConstants = NULL;


__declspec(align(16)) typedef struct drawconstants_s {
	QMATRIX OrthoMatrix;
	float gamma;
	float brightness;
	float aspectW;
	float aspectH;
} drawconstants_t;


void Draw_CreateBuffers (void)
{
	D3D11_BUFFER_DESC vbDesc = {
		sizeof (drawpolyvert_t) * MAX_DRAW_VERTS,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_VERTEX_BUFFER,
		D3D11_CPU_ACCESS_WRITE,
		0,
		0
	};

	D3D11_BUFFER_DESC ibDesc = {
		sizeof (unsigned short) * MAX_DRAW_INDEXES,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	int i;
	unsigned short *ndx = ri.Load_AllocMemory (sizeof (unsigned short) * MAX_DRAW_INDEXES);
	D3D11_SUBRESOURCE_DATA srd = {ndx, 0, 0};

	for (i = 0; i < MAX_DRAW_VERTS; i += 4, ndx += 6)
	{
		ndx[0] = i + 0;
		ndx[1] = i + 1;
		ndx[2] = i + 2;

		ndx[3] = i + 0;
		ndx[4] = i + 2;
		ndx[5] = i + 3;
	}

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, NULL, &d3d_DrawVertexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_DrawIndexes, "d3d_DrawIndexes");

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &d3d_DrawIndexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_DrawVertexes, "d3d_DrawVertexes");

	ri.Load_FreeMemory ();
}


/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	D3D11_BUFFER_DESC cbDrawDesc = {
		sizeof (drawconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	D3D11_BUFFER_DESC cbCineDesc = {
		sizeof (QMATRIX),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	D3D11_INPUT_ELEMENT_DESC layout_standard[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		VDECL ("COLOUR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0)
	};

	D3D11_INPUT_ELEMENT_DESC layout_texarray[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0)
	};

	// cbuffers
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbDrawDesc, NULL, &d3d_DrawConstants);
	D_RegisterConstantBuffer (d3d_DrawConstants, 0);

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbCineDesc, NULL, &d3d_CineConstants);
	D_RegisterConstantBuffer (d3d_CineConstants, 5);

	// shaders
	d3d_DrawTexturedShader = D_CreateShaderBundle (IDR_DRAWSHADER, "DrawTexturedVS", NULL, "DrawTexturedPS", DEFINE_LAYOUT (layout_standard));
	d3d_DrawColouredShader = D_CreateShaderBundle (IDR_DRAWSHADER, "DrawColouredVS", NULL, "DrawColouredPS", DEFINE_LAYOUT (layout_standard));
	d3d_DrawTexArrayShader = D_CreateShaderBundle (IDR_DRAWSHADER, "DrawTexArrayVS", NULL, "DrawTexArrayPS", DEFINE_LAYOUT (layout_texarray));

	// shaders for use without buffers
	d3d_DrawFinalizeShader = D_CreateShaderBundle (IDR_DRAWSHADER, "DrawFinalizeVS", NULL, "DrawFinalizePS", NULL, 0);
	d3d_DrawCinematicShader = D_CreateShaderBundle (IDR_DRAWSHADER, "DrawCinematicVS", NULL, "DrawCinematicPS", NULL, 0);
	d3d_DrawFadescreenShader = D_CreateShaderBundle (IDR_DRAWSHADER, "DrawFadescreenVS", NULL, "DrawFadescreenPS", NULL, 0);

	// vertex and index buffers
	Draw_CreateBuffers ();

	// load console characters
	draw_chars = GL_FindImage ("pics/conchars.pcx", it_charset);
	sb_nums[0] = R_LoadTexArray ("num");
	sb_nums[1] = R_LoadTexArray ("anum");
}


void Draw_UpdateConstants (int scrflags)
{
	__declspec(align(16)) drawconstants_t consts;

	R_MatrixIdentity (&consts.OrthoMatrix);
	R_MatrixOrtho (&consts.OrthoMatrix, 0, vid.conwidth, vid.conheight, 0, -1, 1);

	if (scrflags & SCR_NO_GAMMA)
		consts.gamma = 1.0f;
	else consts.gamma = vid_gamma->value;

	if (scrflags & SCR_NO_BRIGHTNESS)
		consts.brightness = 1.0f;
	else consts.brightness = vid_brightness->value;

	consts.aspectW = (float) vid.conwidth / (float) vid.width;
	consts.aspectH = (float) vid.conheight / (float) vid.height;

	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_DrawConstants, 0, NULL, &consts, 0, 0);
}


void Draw_Flush (void)
{
	if (d_drawverts)
	{
		d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) d3d_DrawVertexes, 0);
		d_drawverts = NULL;
	}

	if (d_numdrawverts)
	{
		D_BindVertexBuffer (0, d3d_DrawVertexes, sizeof (drawpolyvert_t), 0);
		D_BindIndexBuffer (d3d_DrawIndexes, DXGI_FORMAT_R16_UINT);

		d3d_Context->lpVtbl->DrawIndexed (d3d_Context, (d_numdrawverts >> 2) * 6, 0, d_firstdrawvert);

		d_firstdrawvert += d_numdrawverts;
		d_numdrawverts = 0;
	}
}


qboolean Draw_EnsureBufferSpace (void)
{
	if (d_firstdrawvert + d_numdrawverts + 4 >= MAX_DRAW_VERTS)
	{
		// if we run out of buffer space for the next quad we flush the batch and begin a new one
		Draw_Flush ();
		d_firstdrawvert = 0;
	}

	if (!d_drawverts)
	{
		// first index is only reset to 0 if the buffer must wrap so this is valid to do
		D3D11_MAP mode = (d_firstdrawvert > 0) ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
		D3D11_MAPPED_SUBRESOURCE msr;

		if (FAILED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) d3d_DrawVertexes, 0, mode, 0, &msr)))
			return false;
		else d_drawverts = (drawpolyvert_t *) msr.pData + d_firstdrawvert;
	}

	// all OK!
	return true;
}


void Draw_TexturedVertex (drawpolyvert_t *vert, float x, float y, DWORD color, float s, float t)
{
	vert->position[0] = x;
	vert->position[1] = y;

	vert->texcoord[0] = s;
	vert->texcoord[1] = t;

	vert->color = color;
}


void Draw_ColouredVertex (drawpolyvert_t *vert, float x, float y, DWORD color)
{
	vert->position[0] = x;
	vert->position[1] = y;

	vert->color = color;
}


void Draw_CharacterVertex (drawpolyvert_t *vert, float x, float y, float s, float t, float slice)
{
	vert->position[0] = x;
	vert->position[1] = y;

	vert->texcoord[0] = s;
	vert->texcoord[1] = t;

	vert->slice = slice;
}


void Draw_TexturedQuad (image_t *image, int x, int y, int w, int h, unsigned color)
{
	R_BindTexture (image->SRV);

	D_BindShaderBundle (d3d_DrawTexturedShader);
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSNoDepth, d3d_RSNoCull);

	if (Draw_EnsureBufferSpace ())
	{
		Draw_TexturedVertex (&d_drawverts[d_numdrawverts++], x, y, color, 0, 0);
		Draw_TexturedVertex (&d_drawverts[d_numdrawverts++], x + w, y, color, 1, 0);
		Draw_TexturedVertex (&d_drawverts[d_numdrawverts++], x + w, y + h, color, 1, 1);
		Draw_TexturedVertex (&d_drawverts[d_numdrawverts++], x, y + h, color, 0, 1);

		Draw_Flush ();
	}
}


void Draw_CharacterQuad (int x, int y, int w, int h, int slice)
{
	// check for overflow
	if (Draw_EnsureBufferSpace ())
	{
		// and draw it
		Draw_CharacterVertex (&d_drawverts[d_numdrawverts++], x, y, 0, 0, slice);
		Draw_CharacterVertex (&d_drawverts[d_numdrawverts++], x + w, y, 1, 0, slice);
		Draw_CharacterVertex (&d_drawverts[d_numdrawverts++], x + w, y + h, 1, 1, slice);
		Draw_CharacterVertex (&d_drawverts[d_numdrawverts++], x, y + h, 0, 1, slice);
	}
}


void Draw_Field (int x, int y, int color, int width, int value)
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	Com_sprintf (num, sizeof (num), "%i", value);
	l = strlen (num);

	if (l > width)
		l = width;

	x += 2 + sb_nums[color]->width * (width - l);
	ptr = num;

	R_BindTexArray (sb_nums[color]->SRV);

	D_BindShaderBundle (d3d_DrawTexArrayShader);
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSNoDepth, d3d_RSNoCull);

	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else frame = *ptr - '0';

		Draw_CharacterQuad (x, y, sb_nums[color]->width, sb_nums[color]->height, frame);

		x += sb_nums[color]->width;
		ptr++;
		l--;
	}

	Draw_Flush ();
}


/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char (int x, int y, int num)
{
	// totally off screen
	if (y <= -8) return;

	// space
	if ((num & 127) == 32) return;

	// these are done for each char but they only trigger state changes for the first
	R_BindTexArray (draw_chars->SRV);

	D_BindShaderBundle (d3d_DrawTexArrayShader);
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSNoDepth, d3d_RSNoCull);

	Draw_CharacterQuad (x, y, 8, 8, num & 255);
}


/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	if (name[0] != '/' && name[0] != '\\')
		return GL_FindImage (va ("pics/%s.pcx", name), it_pic);
	else return GL_FindImage (name + 1, it_pic);
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl = Draw_FindPic (pic);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *pic)
{
	image_t *gl = Draw_FindPic (pic);

	if (!gl)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	Draw_TexturedQuad (gl, x, y, gl->width, gl->height, 0xffffffff);
}


void Draw_ConsoleBackground (int x, int y, int w, int h, char *pic, int alpha)
{
	image_t *gl = Draw_FindPic (pic);

	if (!gl)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (alpha >= 255)
		Draw_TexturedQuad (gl, x, y, w, h, 0xffffffff);
	else if (alpha > 0)
		Draw_TexturedQuad (gl, x, y, w, h, (alpha << 24) | 0xffffff);
}


void Draw_Fill (int x, int y, int w, int h, int c)
{
	// this is a quad filled with a single solid colour so it doesn't need to blend
	D_BindShaderBundle (d3d_DrawColouredShader);
	D_SetRenderStates (d3d_BSNone, d3d_DSNoDepth, d3d_RSNoCull);

	if (Draw_EnsureBufferSpace ())
	{
		Draw_ColouredVertex (&d_drawverts[d_numdrawverts++], x, y, d_8to24table_solid[c]);
		Draw_ColouredVertex (&d_drawverts[d_numdrawverts++], x + w, y, d_8to24table_solid[c]);
		Draw_ColouredVertex (&d_drawverts[d_numdrawverts++], x + w, y + h, d_8to24table_solid[c]);
		Draw_ColouredVertex (&d_drawverts[d_numdrawverts++], x, y + h, d_8to24table_solid[c]);

		Draw_Flush ();
	}
}


//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSDepthNoWrite, d3d_RSNoCull);
	D_BindShaderBundle (d3d_DrawFadescreenShader);

	// full-screen triangle
	d3d_Context->lpVtbl->Draw (d3d_Context, 3, 0);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
texture_t r_CinematicPic;


void Draw_ShutdownRawImage (void)
{
	R_ReleaseTexture (&r_CinematicPic);
}


void R_TexSubImage32 (ID3D11Texture2D *tex, int level, int x, int y, int w, int h, unsigned *data);
void R_TexSubImage8 (ID3D11Texture2D *tex, int level, int x, int y, int w, int h, byte *data, unsigned *palette);

void Draw_StretchRaw (int cols, int rows, byte *data, int frame, const unsigned char *palette)
{
	// we only need to refresh the texture if the frame changes
	static int r_rawframe = -1;

	// matrix transform for positioning the cinematic correctly
	// sampler state should be set to clamp-to-border with a border color of black
	__declspec(align(16)) QMATRIX cineMatrix;
	float strans, ttrans;

	// if the dimensions change the texture needs to be recreated
	if (r_CinematicPic.Desc.Width != cols || r_CinematicPic.Desc.Height != rows)
		Draw_ShutdownRawImage ();

	if (!r_CinematicPic.SRV)
	{
		// ensure in case we got a partial creation
		Draw_ShutdownRawImage ();

		// and create it
		R_CreateTexture (&r_CinematicPic, NULL, cols, rows, 1, TEX_RGBA8 | TEX_MUTABLE);

		// load the image
		r_rawframe = -1;
	}

	// only reload the texture if the frame changes
	// in *theory* the original code allowed the palette to be changed independently of the texture, in practice the .cin format doesn't support this
	if (r_rawframe != frame)
	{
		if (palette)
		{
			unsigned r_rawpalette[256];
			Image_QuakePalFromPCXPal (r_rawpalette, palette, TEX_RGBA8);
			R_TexSubImage8 (r_CinematicPic.Texture, 0, 0, 0, cols, rows, data, r_rawpalette);
		}
		else R_TexSubImage8 (r_CinematicPic.Texture, 0, 0, 0, cols, rows, data, d_8to24table_solid);

		r_rawframe = frame;
	}

	// free any memory we may have used for loading it
	ri.Load_FreeMemory ();

	// derive the texture matrix for the cinematic pic
	if (vid.conwidth > vid.conheight)
	{
		strans = 0.5f * ((float) rows / (float) cols) * ((float) vid.conwidth / (float) vid.conheight);
		ttrans = -0.5f;
	}
	else
	{
		strans = 0.5f;
		ttrans = -0.5f * ((float) cols / (float) rows) * ((float) vid.conheight / (float) vid.conwidth);
	}

	// load it on
	R_MatrixLoadf (&cineMatrix, strans, 0, 0, 0, 0, ttrans, 0, 0, 0, 0, 1, 0, 0.5f, 0.5f, 0, 1);

	// and upload it to the GPU
	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_CineConstants, 0, NULL, &cineMatrix, 0, 0);

	R_BindTexture (r_CinematicPic.SRV);

	D_BindShaderBundle (d3d_DrawCinematicShader);
	D_SetRenderStates (d3d_BSNone, d3d_DSNoDepth, d3d_RSNoCull);

	d3d_Context->lpVtbl->Draw (d3d_Context, 3, 0);
}


void R_Set2D (void)
{
	// switch to our 2d viewport
	D3D11_VIEWPORT vp = {0, 0, vid.width, vid.height, 0, 0};
	d3d_Context->lpVtbl->RSSetViewports (d3d_Context, 1, &vp);
}


