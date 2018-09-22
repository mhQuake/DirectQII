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

image_t		*draw_chars;

static int d3d_DrawTexturedShader;
static int d3d_DrawColouredShader;
static int d3d_DrawFinalizeShader;


static ID3D11Buffer *d3d_DrawConstants = NULL;

typedef struct drawconstants_s {
	QMATRIX OrthoMatrix;
	float gamma;
	float contrast;
	float junk[2];
} drawconstants_t;


/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	D3D11_BUFFER_DESC cbDesc = {
		sizeof (drawconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	// buffers
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbDesc, NULL, &d3d_DrawConstants);
	D_RegisterConstantBuffer (d3d_DrawConstants, 0);

	// shaders
	d3d_DrawTexturedShader = D_CreateShaderBundleForQuadBatch (IDR_DRAWSHADER, "DrawTexturedVS", "DrawTexturedPS");
	d3d_DrawColouredShader = D_CreateShaderBundleForQuadBatch (IDR_DRAWSHADER, "DrawColouredVS", "DrawColouredPS");
	d3d_DrawFinalizeShader = D_CreateShaderBundleForQuadBatch (IDR_DRAWSHADER, "DrawFinalizeVS", "DrawFinalizePS");

	// load console characters
	draw_chars = GL_FindImage ("pics/conchars.pcx", it_pic);
}


void D_UpdateDrawConstants (int width, int height, float gammaval, float contrastval)
{
	drawconstants_t consts;

	R_MatrixIdentity (&consts.OrthoMatrix);
	R_MatrixOrtho (&consts.OrthoMatrix, 0, width, height, 0, -1, 1);

	consts.gamma = gammaval;
	consts.contrast = contrastval;

	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_DrawConstants, 0, NULL, &consts, 0, 0);
}


void Draw_TexturedColouredQuad (image_t *image, int x, int y, int w, int h, unsigned color, int sl, int sh, int tl, int th)
{
	GL_BindTexture (image->SRV);

	D_BindShaderBundle (d3d_DrawTexturedShader);
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSNoDepth, d3d_RSNoCull);

	D_CheckQuadBatch ();

	D_QuadVertexPosition2fColorTexCoord2f (x, y, color, sl, tl);
	D_QuadVertexPosition2fColorTexCoord2f (x + w, y, color, sh, tl);
	D_QuadVertexPosition2fColorTexCoord2f (x + w, y + h, color, sh, th);
	D_QuadVertexPosition2fColorTexCoord2f (x, y + h, color, sl, th);

	D_EndQuadBatch ();
}


void Draw_TexturedQuad (image_t *image, int x, int y, int w, int h, int sl, int sh, int tl, int th)
{
	GL_BindTexture (image->SRV);

	D_BindShaderBundle (d3d_DrawTexturedShader);
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSNoDepth, d3d_RSNoCull);

	D_CheckQuadBatch ();

	D_QuadVertexPosition2fTexCoord2f (x, y, sl, tl);
	D_QuadVertexPosition2fTexCoord2f (x + w, y, sh, tl);
	D_QuadVertexPosition2fTexCoord2f (x + w, y + h, sh, th);
	D_QuadVertexPosition2fTexCoord2f (x, y + h, sl, th);

	D_EndQuadBatch ();
}


void Draw_ColouredQuad (int x, int y, int w, int h, unsigned colour)
{
	D_BindShaderBundle (d3d_DrawColouredShader);
	D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSNoDepth, d3d_RSNoCull);

	D_CheckQuadBatch ();

	D_QuadVertexPosition2fColor (x, y, colour);
	D_QuadVertexPosition2fColor (x + w, y, colour);
	D_QuadVertexPosition2fColor (x + w, y + h, colour);
	D_QuadVertexPosition2fColor (x, y + h, colour);

	D_EndQuadBatch ();
}


/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
static void Draw_CharInner (int x, int y, int num)
{
	int row = num >> 4;
	int col = num & 15;

	float frow = row * 0.0625;
	float fcol = col * 0.0625;
	float size = 0.0625;

	D_QuadVertexPosition2fTexCoord2f (x, y, fcol, frow);
	D_QuadVertexPosition2fTexCoord2f (x + 8, y, fcol + size, frow);
	D_QuadVertexPosition2fTexCoord2f (x + 8, y + 8, fcol + size, frow + size);
	D_QuadVertexPosition2fTexCoord2f (x, y + 8, fcol, frow + size);
}


void Draw_Char (int x, int y, int num)
{
	// totally off screen
	if (y <= -8) return;

	// space
	if ((num & 127) == 32) return;

	// at this stage we are definitely drawing at least one char so we need to set up state now; 
	// doing it now rather than at Draw_String time in case a char batch needs to be flushed because state would not be set if we deferred it
	GL_BindTexture (draw_chars->SRV);

	D_BindShaderBundle (d3d_DrawTexturedShader);
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSNoDepth, d3d_RSNoCull);

	// check for overflow
	D_CheckQuadBatch ();

	// and draw it
	Draw_CharInner (x, y, num & 255);
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

	Draw_TexturedQuad (gl, x, y, gl->width, gl->height, 0, 1, 0, 1);
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
		Draw_TexturedQuad (gl, x, y, w, h, 0, 1, 0, 1);
	else if (alpha > 0)
		Draw_TexturedColouredQuad (gl, x, y, w, h, (alpha << 24) | 0xffffff, 0, 1, 0, 1);
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image = Draw_FindPic (pic);

	if (!image)
	{
		ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	Draw_TexturedQuad (image, x, y, w, h, x / 64.0, (x + w) / 64.0, y / 64.0, (y + h) / 64.0);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	Draw_ColouredQuad (x, y, w, h, d_8to24table[c]);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	Draw_ColouredQuad (0, 0, vid.width, vid.height, 0xcc000000);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
ID3D11Texture2D *r_RawTexture = NULL;
ID3D11ShaderResourceView *r_RawSRV = NULL;
D3D11_TEXTURE2D_DESC r_RawDesc;

void Draw_ShutdownRawImage (void)
{
	SAFE_RELEASE (r_RawTexture);
	SAFE_RELEASE (r_RawSRV);
	memset (&r_RawDesc, 0, sizeof (r_RawDesc));
}


unsigned r_rawpalette[256];

unsigned *GL_Image8To32 (byte *data, int width, int height, unsigned *palette);
void R_TexSubImage32 (ID3D11Texture2D *tex, int level, int x, int y, int w, int h, unsigned *data);
void R_TexSubImage8 (ID3D11Texture2D *tex, int level, int x, int y, int w, int h, byte *data, unsigned *palette);

void R_SetCinematicPalette (const unsigned char *palette)
{
	if (palette)
		Image_QuakePalFromPCXPal (r_rawpalette, palette);
	else memcpy (r_rawpalette, d_8to24table, sizeof (d_8to24table));

	// refresh the texture if the palette changes
	Draw_ShutdownRawImage ();
}


void Draw_StretchRaw (int cols, int rows, byte *data, int frame)
{
	// we only need to refresh the texture if the frame changes
	static int r_rawframe = -1;

	// drawing coords
	int x, y, w, h;

	// using a dummy image_t so that we can push this through the standard drawing routines
	image_t gl;

	// if the dimensions change the texture needs to be recreated
	if (r_RawDesc.Width != cols || r_RawDesc.Height != rows)
		Draw_ShutdownRawImage ();

	if (!r_RawTexture || !r_RawSRV)
	{
		// ensure in case we got a partial creation
		Draw_ShutdownRawImage ();

		// describe the texture
		R_DescribeTexture (&r_RawDesc, cols, rows, 1, TEX_RGBA8);

		// failure is not an option...
		if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &r_RawDesc, NULL, &r_RawTexture))) ri.Sys_Error (ERR_FATAL, "CreateTexture2D failed");
		if (FAILED (d3d_Device->lpVtbl->CreateShaderResourceView (d3d_Device, (ID3D11Resource *) r_RawTexture, NULL, &r_RawSRV))) ri.Sys_Error (ERR_FATAL, "CreateShaderResourceView failed");

		// load the image
		r_rawframe = -1;
	}

	if (r_rawframe != frame)
	{
		// reload data if required
		R_TexSubImage8 (r_RawTexture, 0, 0, 0, cols, rows, data, r_rawpalette);
		r_rawframe = frame;
	}

	// free any memory we may have used for loading it
	ri.Load_FreeMemory ();

	// clear out the background
	Draw_ColouredQuad (0, 0, vid.width, vid.height, 0x00000000);

	// stretch up the pic to fill the viewport while still maintaining aspect
	w = vid.width;
	h = (w * rows) / cols;

	if (h > vid.height)
	{
		h = vid.height;
		w = (h * cols) / rows;
	}

	// take it down if required
	while (w > vid.width) w >>= 1;
	while (h > vid.height) h >>= 1;

	// and center it properly
	x = (vid.width - w) >> 1;
	y = (vid.height - h) >> 1;

	// set up our dummy image_t
	gl.Texture = r_RawTexture;
	gl.SRV = r_RawSRV;

	// and draw it
	Draw_TexturedQuad (&gl, x, y, w, h, 0, 1, 0, 1);
}

