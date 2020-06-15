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


void D_Mapshot (char *checkname)
{
}


void D_CaptureScreenshot (char *checkname)
{
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

	// now convert it down
	Image_CollapseRowPitch ((unsigned *) msr.pData, descRT.Width, descRT.Height, msr.RowPitch >> 2);
	Image_Compress32To24RGBtoBGR ((byte *) msr.pData, descRT.Width, descRT.Height);

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


void R_ClearToBlack (void)
{
	float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	d3d_Context->lpVtbl->ClearRenderTargetView (d3d_Context, d3d_RenderTarget, clear);
}


