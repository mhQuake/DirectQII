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
// r_misc.c

#include "r_local.h"

/*
==================
R_CreateSpecialTextures
==================
*/
void R_CreateSpecialTextures (void)
{
	int x, y;
	byte data[8][8][4];

	byte dottexture[8][8] = {
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 0, 0, 0},
		{0, 1, 1, 1, 1, 0, 0, 0},
		{0, 0, 1, 1, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
	};

	// also use this for bad textures, but without alpha
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 0;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}

	r_blacktexture = GL_LoadPic ("***r_blacktexture***", (byte *) data, 8, 8, it_wall, 32, NULL);

	// also use this for bad textures, but without alpha
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 127;
			data[y][x][1] = 127;
			data[y][x][2] = 127;
			data[y][x][3] = 255;
		}
	}

	r_greytexture = GL_LoadPic ("***r_greytexture***", (byte *) data, 8, 8, it_wall, 32, NULL);

	// also use this for bad textures, but without alpha
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = 255;
		}
	}

	r_blacktexture = GL_LoadPic ("***r_blacktexture***", (byte *) data, 8, 8, it_wall, 32, NULL);

	// also use this for bad textures, but without alpha
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = dottexture[x & 3][y & 3] * 255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}

	r_notexture = GL_LoadPic ("***r_notexture***", (byte *) data, 8, 8, it_wall, 32, NULL);
}


/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/

void D_CaptureScreenshot (char *checkname, float gammaval)
{
	int i;
	byte inversegamma[256];
	ID3D11Texture2D *pBackBuffer = NULL;
	ID3D11Texture2D *pScreenShot = NULL;
	D3D11_TEXTURE2D_DESC descRT;
	D3D11_MAPPED_SUBRESOURCE msr;

	// Get a pointer to the back buffer
	if (FAILED (d3d_SwapChain->lpVtbl->GetBuffer (d3d_SwapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer)))
	{
		ri.Con_Printf (PRINT_ALL, "d3d_SwapChain->GetBuffer failed");
		return;
	}

	// get the description of the backbuffer for creating the depth buffer from it
	pBackBuffer->lpVtbl->GetDesc (pBackBuffer, &descRT);

	// set the stuff we want to differ
	descRT.Usage = D3D11_USAGE_STAGING;
	descRT.BindFlags = 0;
	descRT.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

	// create a copy of the back buffer
	if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &descRT, NULL, &pScreenShot)))
	{
		ri.Con_Printf (PRINT_ALL, "SCR_GetScreenData : failed to create scratch texture\n");
		goto failed;
	}

	// copy over the back buffer to the screenshot texture
	d3d_Context->lpVtbl->CopyResource (d3d_Context, (ID3D11Resource *) pScreenShot, (ID3D11Resource *) pBackBuffer);
	R_SyncPipeline ();

	if (FAILED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) pScreenShot, 0, D3D11_MAP_READ_WRITE, 0, &msr)))
	{
		ri.Con_Printf (PRINT_ALL, "SCR_GetScreenData : failed to map scratch texture\n");
		goto failed;
	}

	// build an inverse gamma table
	for (i = 0; i < 256; i++)
	{
		if (gammaval > 0)
		{
			if (gammaval < 1 || gammaval > 1)
				inversegamma[i] = Image_GammaVal8to8 (i, 1.0f / gammaval);
			else inversegamma[i] = i;
		}
		else inversegamma[i] = 255 - i;
	}

	// now convert it down
	Image_CollapseRowPitch ((unsigned *) msr.pData, descRT.Width, descRT.Height, msr.RowPitch >> 2);
	Image_Compress32To24RGBtoBGR ((byte *) msr.pData, descRT.Width, descRT.Height);

	// apply inverse gamma
	Image_ApplyTranslationRGB ((byte *) msr.pData, descRT.Width * descRT.Height, inversegamma);

	// and write it out
	Image_WriteDataToTGA (checkname, msr.pData, descRT.Width, descRT.Height, 24);

	// unmap, and...
	d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) pScreenShot, 0);

	// ...done
	ri.Con_Printf (PRINT_ALL, "Wrote %s\n", checkname);

failed:;
	// clean up
	SAFE_RELEASE (pScreenShot);
	SAFE_RELEASE (pBackBuffer);
}


/*
==================
GL_ScreenShot_f
==================
*/
void GL_ScreenShot_f (void)
{
	char		checkname[MAX_OSPATH];
	int			i;
	FILE		*f;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof (checkname), "%s/scrnshot", ri.FS_Gamedir ());
	ri.Mkdir (checkname);

	// find a file name to save it to
	for (i = 0; i <= 99; i++)
	{
		Com_sprintf (checkname, sizeof (checkname), "%s/scrnshot/quake%02i.tga", ri.FS_Gamedir (), i);
		f = fopen (checkname, "rb");

		if (!f)
			break;	// file doesn't exist

		fclose (f);
	}

	if (i == 100)
	{
		ri.Con_Printf (PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n");
		return;
	}

	D_CaptureScreenshot (checkname, vid_gamma->value);
}


