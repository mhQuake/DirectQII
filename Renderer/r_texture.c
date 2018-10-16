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


image_t		gltextures[MAX_GLTEXTURES];

unsigned	d_8to24table_solid[256];
unsigned	d_8to24table_alpha[256];
unsigned	d_8to24table_trans33[256];
unsigned	d_8to24table_trans66[256];


void R_DescribeTexture (D3D11_TEXTURE2D_DESC *Desc, int width, int height, int arraysize, int flags)
{
	// basic info
	Desc->Width = width;
	Desc->Height = height;
	Desc->MipLevels = (flags & TEX_MIPMAP) ? 0 : 1;

	// select the appropriate format
	if (flags & TEX_R32F)
		Desc->Format = DXGI_FORMAT_R32_FLOAT;
	else if (flags & TEX_R16G16)
		Desc->Format = DXGI_FORMAT_R16G16_SNORM;
	else Desc->Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// no multisampling
	Desc->SampleDesc.Count = 1;
	Desc->SampleDesc.Quality = 0;

	// allow creation of staging textures for e.g. copying off screenshots to
	if (flags & TEX_STAGING)
	{
		// assume we want read/write always
		Desc->Usage = D3D11_USAGE_STAGING;
		Desc->CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
		Desc->BindFlags = 0;
	}
	else
	{
		// normal usage with no CPU access
		Desc->Usage = (flags & TEX_MUTABLE) ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE;
		Desc->CPUAccessFlags = 0;
		Desc->BindFlags = D3D11_BIND_SHADER_RESOURCE;
	}

	// select if creating a cubemap (allow creation of cubemap arrays)
	if (flags & TEX_CUBEMAP)
	{
	    Desc->ArraySize = 6 * arraysize;
	    Desc->MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	}
	else
	{
	    Desc->ArraySize = arraysize;
	    Desc->MiscFlags = 0;
	}
}


void R_CreateTexture32 (image_t *image, unsigned *data)
{
	D3D11_TEXTURE2D_DESC Desc;

	if (image->flags & TEX_CHARSET)
	{
		int i;
		D3D11_SUBRESOURCE_DATA srd[256];

		for (i = 0; i < 256; i++)
		{
			int row = (i >> 4);
			int col = (i & 15);

			srd[i].pSysMem = &data[((row * (image->width >> 4)) * image->width) + col * (image->width >> 4)];
			srd[i].SysMemPitch = image->width << 2;
			srd[i].SysMemSlicePitch = 0;
		}

		// describe the texture
		R_DescribeTexture (&Desc, image->width >> 4, image->height >> 4, 256, image->flags);

		// failure is not an option...
		if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &Desc, srd, &image->Texture))) ri.Sys_Error (ERR_FATAL, "CreateTexture2D failed");
	}
	else
	{
		// this is good for a 4-billion X 4-billion texture; we assume it will never be needed that large
		D3D11_SUBRESOURCE_DATA srd[32];

		// copy these off so that they can be changed during miplevel reduction
		int width = image->width;
		int height = image->height;

		// the first one just has the data
		srd[0].pSysMem = data;
		srd[0].SysMemPitch = width << 2;
		srd[0].SysMemSlicePitch = 0;

		// create further miplevels for the texture type
		if (image->flags & TEX_MIPMAP)
		{
			int mipnum;

			for (mipnum = 1; width > 1 || height > 1; mipnum++)
			{
				// choose the appropriate filter
				if ((width & 1) || (height & 1))
					data = Image_MipReduceLinearFilter (data, width, height);
				else data = Image_MipReduceBoxFilter (data, width, height);

				if ((width = width >> 1) < 1) width = 1;
				if ((height = height >> 1) < 1) height = 1;

				srd[mipnum].pSysMem = data;
				srd[mipnum].SysMemPitch = width << 2;
				srd[mipnum].SysMemSlicePitch = 0;
			}
		}

		R_DescribeTexture (&Desc, image->width, image->height, 1, image->flags);

		// failure is not an option...
		if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &Desc, srd, &image->Texture))) ri.Sys_Error (ERR_FATAL, "CreateTexture2D failed");
	}

	// failure is not an option...
	if (FAILED (d3d_Device->lpVtbl->CreateShaderResourceView (d3d_Device, (ID3D11Resource *) image->Texture, NULL, &image->SRV))) ri.Sys_Error (ERR_FATAL, "CreateShaderResourceView failed");
}


void R_CreateTexture8 (image_t *image, byte *data, unsigned *palette)
{
	unsigned *trans = GL_Image8To32 (data, image->width, image->height, palette);
	R_CreateTexture32 (image, trans);
}


void R_TexSubImage32 (ID3D11Texture2D *tex, int level, int x, int y, int w, int h, unsigned *data)
{
	D3D11_BOX texbox = {x, y, 0, x + w, y + h, 1};
	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) tex, level, &texbox, data, w << 2, 0);
}


void R_TexSubImage8 (ID3D11Texture2D *tex, int level, int x, int y, int w, int h, byte *data, unsigned *palette)
{
	unsigned *trans = GL_Image8To32 (data, w, h, palette);
	R_TexSubImage32 (tex, level, x, y, w, h, trans);
	ri.Load_FreeMemory ();
}


void GL_TexEnv (int mode)
{
}

void R_BindTexture (ID3D11ShaderResourceView *SRV)
{
	// only PS slot 0 is filtered; everything else is bound once-only at the start of each frame
	static ID3D11ShaderResourceView *OldSRV;

	if (OldSRV != SRV)
	{
		d3d_Context->lpVtbl->PSSetShaderResources (d3d_Context, 0, 1, &SRV);
		OldSRV = SRV;
	}
}


void R_BindTexArray (ID3D11ShaderResourceView *SRV)
{
	// PS slot 6 holds a texture array that's used for the charset and little sbar numbers
	static ID3D11ShaderResourceView *OldSRV;

	if (OldSRV != SRV)
	{
		d3d_Context->lpVtbl->PSSetShaderResources (d3d_Context, 6, 1, &SRV);
		OldSRV = SRV;
	}
}


image_t *GL_FindFreeImage (char *name, int width, int height, imagetype_t type)
{
	image_t		*image;
	int			i;

	// find a free image_t
	for (i = 0, image = gltextures; i < MAX_GLTEXTURES; i++, image++)
	{
		if (image->Texture) continue;
		if (image->SRV) continue;

		break;
	}

	if (i == MAX_GLTEXTURES)
		ri.Sys_Error (ERR_DROP, "MAX_GLTEXTURES");

	image = &gltextures[i];

	if (strlen (name) >= sizeof (image->name))
		ri.Sys_Error (ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);

	strcpy (image->name, name);
	image->registration_sequence = r_registration_sequence;

	image->width = width;
	image->height = height;

	// basic flags
	image->flags = TEX_RGBA8;

	// additional flags - these image types are mipmapped
	if (type != it_pic && type != it_charset) image->flags |= TEX_MIPMAP;

	// these image types may be thrown away
	if (type != it_pic && type != it_charset) image->flags |= TEX_DISPOSABLE;

	// drawn as a 256-slice texture array
	if (type == it_charset) image->flags |= TEX_CHARSET;

	return image;
}


/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits, unsigned *palette)
{
	image_t *image = GL_FindFreeImage (name, width, height, type);

	// floodfill 8-bit alias skins (32-bit are assumed to be already filled)
	if (type == it_skin && bits == 8)
		R_FloodFillSkin (pic, width, height);

	// problem - if we use linear filtering, we lose all of the fine pixel art detail in the original 8-bit textures.
	// if we use nearest filtering we can't do anisotropic and we get noise at minification levels.
	// so what we do is upscale the texture by a simple 2x nearest-neighbour upscale, which gives us magnification-nearest
	// quality but not with the same degree of discontinuous noise, but let's us minify and anisotropically filter them properly.
	if ((type == it_wall || type == it_skin) && bits == 8)
	{
		pic = Image_Upscale8 (pic, image->width, image->height);
		image->width <<= 1;
		image->height <<= 1;
		image->flags |= TEX_UPSCALE;
	}

	// it's 2018 and we have non-power-of-two textures nowadays so don't bother with scraps
	if (bits == 8)
		R_CreateTexture8 (image, pic, palette);
	else
		R_CreateTexture32 (image, (unsigned *) pic);

	// if the image was upscaled, bring it back down again so that texcoord calculation will work as expected
	if (image->flags & TEX_UPSCALE)
	{
		image->width >>= 1;
		image->height >>= 1;
	}

	// free memory used for loading the image
	ri.Load_FreeMemory ();

	return image;
}


image_t *GL_HaveImage (char *name, int flags)
{
	int		i;
	image_t	*image;

	// look for it
	for (i = 0, image = gltextures; i < MAX_GLTEXTURES; i++, image++)
	{
		// not a valid image
		if (!image->Texture) continue;
		if (!image->SRV) continue;

		// only brush models send texinfo flags and they must match because we're encoding alpha into the textures
		if (image->texinfoflags != flags) continue;

		if (!strcmp (name, image->name))
		{
			image->registration_sequence = r_registration_sequence;
			return image;
		}
	}

	// don't have it
	return NULL;
}


/*
================
GL_LoadWal
================
*/
image_t *GL_LoadWal (char *name, int flags)
{
	miptex_t	*mt;
	int			width, height, ofs;
	image_t		*image;

	// look for it
	if ((image = GL_HaveImage (name, flags)) != NULL)
		return image;

	// load the pic from disk
	ri.FS_LoadFile (name, (void **) &mt);

	if (!mt)
	{
		ri.Con_Printf (PRINT_ALL, "GL_FindImage: can't load %s\n", name);
		return r_notexture;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);

	// choose the correct palette to use (note: using texinfo flags here)
	if (flags & SURF_TRANS33)
		image = GL_LoadPic (name, (byte *) mt + ofs, width, height, it_wall, 8, d_8to24table_trans33);
	else if (flags & SURF_TRANS66)
		image = GL_LoadPic (name, (byte *) mt + ofs, width, height, it_wall, 8, d_8to24table_trans66);
	else image = GL_LoadPic (name, (byte *) mt + ofs, width, height, it_wall, 8, d_8to24table_solid);

	ri.FS_FreeFile ((void *) mt);

	// free any memory used for loading
	ri.Load_FreeMemory ();

	// store out the flags used for matching
	image->texinfoflags = flags;

	return image;
}


/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
image_t *GL_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		len;
	byte	*pic, *palette;
	int		width, height;

	// validate the name
	if (!name) return NULL;
	if ((len = strlen (name)) < 5) return NULL;

	// look for it
	if ((image = GL_HaveImage (name, 0)) != NULL)
		return image;

	// load the pic from disk
	pic = NULL;
	palette = NULL;

	// PCX/TGA types only; WAL is sent directly through GL_LoadWal
	if (!strcmp (name + len - 4, ".pcx"))
	{
		unsigned table[256];

		LoadPCX (name, &pic, &palette, &width, &height);

		if (!pic)
			return NULL;

		// skins use the solid palette; everything else has alpha
		if (type == it_skin)
			Image_QuakePalFromPCXPal (table, palette, TEX_RGBA8);
		else Image_QuakePalFromPCXPal (table, palette, TEX_ALPHA);

		image = GL_LoadPic (name, pic, width, height, type, 8, table);
	}
	else if (!strcmp (name + len - 4, ".tga"))
	{
		if ((pic = Image_LoadTGA (name, &width, &height)) == NULL)
			return NULL;
		else image = GL_LoadPic (name, pic, width, height, type, 32, NULL);
	}
	else
	{
		ri.Sys_Error (ERR_DROP, "GL_FindImage : %s is unsupported file type\n", name);
		return NULL;
	}

	// free any memory used for loading
	ri.Load_FreeMemory ();

	// store out the flags used for matching
	image->texinfoflags = 0;

	return image;
}


/*
===============
R_RegisterSkin
===============
*/
struct image_s *R_RegisterSkin (char *name)
{
	return GL_FindImage (name, it_skin);
}


/*
================
R_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void R_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	// never free special textures
	r_notexture->registration_sequence = r_registration_sequence;
	r_blacktexture->registration_sequence = r_registration_sequence;
	r_greytexture->registration_sequence = r_registration_sequence;
	r_whitetexture->registration_sequence = r_registration_sequence;

	for (i = 0, image = gltextures; i < MAX_GLTEXTURES; i++, image++)
	{
		// not a valid image
		if (!image->Texture) continue;
		if (!image->SRV) continue;

		// used this sequence
		if (image->registration_sequence == r_registration_sequence) continue;

		// disposable type
		if (image->flags & TEX_DISPOSABLE)
		{
			SAFE_RELEASE (image->Texture);
			SAFE_RELEASE (image->SRV);

			memset (image, 0, sizeof (*image));
		}
	}
}


/*
===============
R_InitImages
===============
*/
void R_InitImages (void)
{
	r_registration_sequence = 1;
	Draw_GetPalette ();
}


/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i = 0, image = gltextures; i < MAX_GLTEXTURES; i++, image++)
	{
		SAFE_RELEASE (image->Texture);
		SAFE_RELEASE (image->SRV);

		memset (image, 0, sizeof (*image));
	}

	Draw_ShutdownRawImage ();
}


image_t *R_LoadTexArray (char *base)
{
	int i;
	image_t *image = NULL;
	char *sb_nums[11] = {"_0", "_1", "_2", "_3", "_4", "_5", "_6", "_7", "_8", "_9", "_minus"};

	byte	*sb_pic[11];
	byte	*sb_palette[11];
	int		sb_width[11];
	int		sb_height[11];

	D3D11_SUBRESOURCE_DATA srd[11];
	D3D11_TEXTURE2D_DESC Desc;

	for (i = 0; i < 11; i++)
	{
		LoadPCX (va ("pics/%s%s.pcx", base, sb_nums[i]), &sb_pic[i], &sb_palette[i], &sb_width[i], &sb_height[i]);

		if (!sb_pic[i]) ri.Sys_Error (ERR_FATAL, "malformed sb number set");
		if (sb_width[i] != sb_width[0]) ri.Sys_Error (ERR_FATAL, "malformed sb number set");
		if (sb_height[i] != sb_height[0]) ri.Sys_Error (ERR_FATAL, "malformed sb number set");

		srd[i].pSysMem = GL_Image8To32 (sb_pic[i], sb_width[i], sb_height[i], d_8to24table_alpha);
		srd[i].SysMemPitch = sb_width[i] << 2;
		srd[i].SysMemSlicePitch = 0;
	}

	// find an image_t for it
	image = GL_FindFreeImage (va ("sb_%ss_texarray", base), sb_width[0], sb_height[0], it_pic);

	// describe the texture
	R_DescribeTexture (&Desc, sb_width[0], sb_height[0], 11, image->flags);

	// failure is not an option...
	if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &Desc, srd, &image->Texture))) ri.Sys_Error (ERR_FATAL, "CreateTexture2D failed");
	if (FAILED (d3d_Device->lpVtbl->CreateShaderResourceView (d3d_Device, (ID3D11Resource *) image->Texture, NULL, &image->SRV))) ri.Sys_Error (ERR_FATAL, "CreateShaderResourceView failed");

	// free memory used for loading the image
	ri.Load_FreeMemory ();

	return image;
}


void R_CreateRenderTarget (rendertarget_t *rt)
{
	ID3D11Texture2D *pBackBuffer = NULL;

	// Get a pointer to the back buffer
	if (FAILED (d3d_SwapChain->lpVtbl->GetBuffer (d3d_SwapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer)))
	{
		ri.Sys_Error (ERR_FATAL, "D_CreateRenderTargetAtBackbufferSize : d3d_SwapChain->GetBuffer failed");
		return;
	}

	// get the description of the backbuffer for creating the new rendertarget from it
	pBackBuffer->lpVtbl->GetDesc (pBackBuffer, &rt->Desc);
	pBackBuffer->lpVtbl->Release (pBackBuffer);

	// adjust the desc for RTT usage
	rt->Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

	// and create it - failure is not an option...
	if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &rt->Desc, NULL, &rt->Texture))) ri.Sys_Error (ERR_FATAL, "CreateTexture2D failed");
	if (FAILED (d3d_Device->lpVtbl->CreateShaderResourceView (d3d_Device, (ID3D11Resource *) rt->Texture, NULL, &rt->SRV))) ri.Sys_Error (ERR_FATAL, "CreateShaderResourceView failed");
	if (FAILED (d3d_Device->lpVtbl->CreateRenderTargetView (d3d_Device, (ID3D11Resource *) rt->Texture, NULL, &rt->RTV))) ri.Sys_Error (ERR_FATAL, "CreateRenderTargetView failed");
}


void R_ReleaseRenderTarget (rendertarget_t *rt)
{
	SAFE_RELEASE (rt->Texture);
	SAFE_RELEASE (rt->SRV);
	SAFE_RELEASE (rt->RTV);
	memset (rt, 0, sizeof (rendertarget_t));
}


void R_CreateTexture (texture_t *t, D3D11_SUBRESOURCE_DATA *srd, int width, int height, int arraysize, int flags)
{
	// if an srd is *not* specified we must make the texture mutable because we must be able to update it later
	// if an srd *is* specified we cannot make the texture immutable because we may also need to update it later
	if (!srd) flags |= TEX_MUTABLE;

	// describe the texture
	R_DescribeTexture (&t->Desc, width, height, arraysize, flags);

	// failure is not an option...
	if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &t->Desc, srd, &t->Texture))) ri.Sys_Error (ERR_FATAL, "CreateTexture2D failed");
	if (FAILED (d3d_Device->lpVtbl->CreateShaderResourceView (d3d_Device, (ID3D11Resource *) t->Texture, NULL, &t->SRV))) ri.Sys_Error (ERR_FATAL, "CreateShaderResourceView failed");
}


void R_ReleaseTexture (texture_t *t)
{
	SAFE_RELEASE (t->Texture);
	SAFE_RELEASE (t->SRV);
	memset (t, 0, sizeof (texture_t));
}


void R_CreateTBuffer (tbuffer_t *tb, void *data, int NumElements, int ElementSize, DXGI_FORMAT Format, D3D11_USAGE Usage)
{
	D3D11_BUFFER_DESC tbDesc = {
		ElementSize * NumElements,
		Usage,
		D3D11_BIND_SHADER_RESOURCE,
		0,
		0,
		0
	};

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

	srvDesc.Format = Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = NumElements;

	if (data)
	{
		D3D11_SUBRESOURCE_DATA srd = {data, 0, 0};
		d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &tbDesc, &srd, &tb->Buffer);
	}
	else
	{
		// if no data is specified at creation time we must switch to default usage so that it can be specified later
		tbDesc.Usage = D3D11_USAGE_DEFAULT;
		d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &tbDesc, NULL, &tb->Buffer);
	}

	d3d_Device->lpVtbl->CreateShaderResourceView (d3d_Device, (ID3D11Resource *) tb->Buffer, &srvDesc, &tb->SRV);
}


void R_ReleaseTBuffer (tbuffer_t *tb)
{
	SAFE_RELEASE (tb->Buffer);
	SAFE_RELEASE (tb->SRV);
	memset (tb, 0, sizeof (tbuffer_t));
}


void R_CopyScreen (rendertarget_t *dst)
{
	ID3D11Texture2D *pBackBuffer = NULL;

	// Get a pointer to the back buffer
	if (SUCCEEDED (d3d_SwapChain->lpVtbl->GetBuffer (d3d_SwapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer)))
	{
		// we need to use CopySubresourceRegion because the target and/or source may be mipped
		d3d_Context->lpVtbl->CopySubresourceRegion (d3d_Context, (ID3D11Resource *) dst->Texture, 0, 0, 0, 0, (ID3D11Resource *) pBackBuffer, 0, NULL);

		// and done
		pBackBuffer->lpVtbl->Release (pBackBuffer);
	}
}


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// special texture loading
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
void R_CreateSpecialTextures (void)
{
	unsigned blacktexturedata = 0xff000000;
	unsigned greytexturedata = 0xff7f7f7f;
	unsigned whitetexturedata = 0xffffffff;
	byte notexturedata[16] = {0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00};

	r_blacktexture = GL_LoadPic ("***r_blacktexture***", (byte *) &blacktexturedata, 1, 1, it_wall, 32, NULL);
	r_greytexture = GL_LoadPic ("***r_greytexture***", (byte *) &greytexturedata, 1, 1, it_wall, 32, NULL);
	r_whitetexture = GL_LoadPic ("***r_whitetexture***", (byte *) &whitetexturedata, 1, 1, it_wall, 32, NULL);
	r_notexture = GL_LoadPic ("***r_notexture***", notexturedata, 4, 4, it_wall, 8, d_8to24table_solid);
}


