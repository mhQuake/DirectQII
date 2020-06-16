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

#include "r_local.h"


qboolean D_CaptureScreenSubrect (char *checkname, int x, int y, int width, int height, float gammaval, float contrastval)
{
	byte inversegamma[256];
	ID3D11Texture2D *pBackBuffer = NULL;
	ID3D11Texture2D *pScreenShot = NULL;
	D3D11_TEXTURE2D_DESC descRT;
	D3D11_MAPPED_SUBRESOURCE msr;
	qboolean succeeded = false; // it's failed until we know it succeeds

	// Get a pointer to the back buffer
	if (FAILED (d3d_SwapChain->lpVtbl->GetBuffer (d3d_SwapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer)))
	{
		ri.Con_Printf (PRINT_ALL, "D_CaptureScreenSubrect : d3d_SwapChain->GetBuffer failed");
		goto failed;
	}

	// get the description of the backbuffer for creating the depth buffer from it
	pBackBuffer->lpVtbl->GetDesc (pBackBuffer, &descRT);

	// set the stuff we want to differ
	descRT.Width = width;
	descRT.Height = height;
	descRT.Usage = D3D11_USAGE_STAGING;
	descRT.BindFlags = 0;
	descRT.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

	// create a copy of the back buffer
	if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &descRT, NULL, &pScreenShot)))
	{
		ri.Con_Printf (PRINT_ALL, "D_CaptureScreenSubrect : failed to create scratch texture\n");
		goto failed;
	}

	// copy over the back buffer to the screenshot texture
	D3D11_BOX srcBox = { x, y, 0, x + width, y + height, 1 };
	d3d_Context->lpVtbl->CopySubresourceRegion (d3d_Context, (ID3D11Resource *) pScreenShot, 0, 0, 0, 0, (ID3D11Resource *) pBackBuffer, 0, &srcBox);
	R_SyncPipeline ();

	if (FAILED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) pScreenShot, 0, D3D11_MAP_READ_WRITE, 0, &msr)))
	{
		ri.Con_Printf (PRINT_ALL, "D_CaptureScreenSubrect : failed to map scratch texture\n");
		goto failed;
	}

	// now convert it down
	Image_CollapseRowPitch ((unsigned *) msr.pData, descRT.Width, descRT.Height, msr.RowPitch >> 2);
	Image_Compress32To24RGBtoBGR ((byte *) msr.pData, descRT.Width, descRT.Height);

	// build an inverse gamma table
	for (int i = 0; i < 256; i++)
	{
		if (gammaval > 0)
		{
			if (gammaval < 1 || gammaval > 1)
				inversegamma[i] = Image_GammaVal8to8 (i, 1.0f / gammaval);
			else inversegamma[i] = i;
		}
		else inversegamma[i] = 0;

		if (contrastval > 0)
		{
			if (contrastval < 1 || contrastval > 1)
				inversegamma[i] = Image_ContrastVal8to8 (inversegamma[i], 1.0f / contrastval);
			else inversegamma[i] = inversegamma[i];
		}
		else inversegamma[i] = 0;
	}

	// apply inverse gamma
	Image_ApplyTranslationRGB ((byte *) msr.pData, descRT.Width * descRT.Height, inversegamma);

	// and write it out
	Image_WriteDataToTGA (checkname, msr.pData, descRT.Width, descRT.Height, 24);

	// unmap, and...
	d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) pScreenShot, 0);

	// ...done
	succeeded = true;

	// fall through to cleanup and return
failed:;
	// clean up
	SAFE_RELEASE (pScreenShot);
	SAFE_RELEASE (pBackBuffer);

	return succeeded;
}


void D_Mapshot (char *checkname)
{
	refdef_t refdef;

	// copy over the last r_newrefdef
	memcpy (&refdef, &r_newrefdef, sizeof (refdef));

	// set it up for mapshotting
	refdef.x = 0;
	refdef.y = 0;
	refdef.width = MAPSHOT_SIZE;
	refdef.height = MAPSHOT_SIZE;

	// this is a special 1:1 aspect for mapshots so set the horizontal and vertical FOV to the same as the vertical FOV for a regular mode at horizontal 90
	refdef.main_fov.x = refdef.main_fov.y = 68.0386963f;
	refdef.gun_fov.x = refdef.gun_fov.y = 68.0386963f;

	// exclude the weapon model from mapshot renders
	refdef.rdflags |= RDF_NOWEAPONMODEL;

	// and draw it
	R_RenderFrame (&refdef);

	// now we can copy it off
	if (D_CaptureScreenSubrect (checkname, 0, 0, MAPSHOT_SIZE, MAPSHOT_SIZE, vid_gamma->value, vid_brightness->value))
	{
		// ...done
		ri.Con_Printf (PRINT_ALL, "Wrote %s\n", checkname);
	}
}


void D_CaptureScreenshot (char *checkname)
{
	// screenshot of full screen
	// screenshots are already pre-adjusted for gamma and contrast so don't need to inverse them
	if (D_CaptureScreenSubrect (checkname, 0, 0, vid.width, vid.height, 1.0f, 1.0f))
	{
		// ...done
		ri.Con_Printf (PRINT_ALL, "Wrote %s\n", checkname);
	}
}


void R_ClearToBlack (void)
{
	float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	d3d_Context->lpVtbl->ClearRenderTargetView (d3d_Context, d3d_RenderTarget, clear);
}


