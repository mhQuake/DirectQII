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


// gamma-correct to 16-bit precision, average, then mix back down to 8-bit precision so that we don't lose ultra-darks in the correction process
unsigned short image_mipgammatable[256];
byte image_mipinversegamma[65536];


byte Image_GammaVal8to8 (byte val, float gamma)
{
	float f = powf ((val + 1) / 256.0, gamma);
	float inf = f * 255 + 0.5;

	if (inf < 0) inf = 0;
	if (inf > 255) inf = 255;

	return inf;
}


byte Image_ContrastVal8to8 (byte val, float contrast)
{
	float f = (float) val * contrast;

	if (f < 0) f = 0;
	if (f > 255) f = 255;

	return f;
}


unsigned short Image_GammaVal8to16 (byte val, float gamma)
{
	float f = powf ((val + 1) / 256.0, gamma);
	float inf = f * 65535 + 0.5;

	if (inf < 0) inf = 0;
	if (inf > 65535) inf = 65535;

	return inf;
}


byte Image_GammaVal16to8 (unsigned short val, float gamma)
{
	float f = powf ((val + 1) / 65536.0, gamma);
	float inf = (f * 255) + 0.5;

	if (inf < 0) inf = 0;
	if (inf > 255) inf = 255;

	return inf;
}


unsigned short Image_GammaVal16to16 (unsigned short val, float gamma)
{
	float f = powf ((val + 1) / 65536.0, gamma);
	float inf = (f * 65535) + 0.5;

	if (inf < 0) inf = 0;
	if (inf > 65535) inf = 65535;

	return inf;
}


int AverageMip (int _1, int _2, int _3, int _4)
{
	return (_1 + _2 + _3 + _4) >> 2;
}


int AverageMipGC (int _1, int _2, int _3, int _4)
{
	// http://filmicgames.com/archives/327
	// gamma-correct to 16-bit precision, average, then mix back down to 8-bit precision so that we don't lose ultra-darks in the correction process
	return image_mipinversegamma[(image_mipgammatable[_1] + image_mipgammatable[_2] + image_mipgammatable[_3] + image_mipgammatable[_4]) >> 2];
}


byte *Image_Upscale8 (byte *in, int inwidth, int inheight)
{
	byte *out = (byte *) ri.Hunk_Alloc (inwidth * inheight * 4);
	int outwidth = inwidth << 1;
	int outheight = inheight << 1;
	int outx, outy, inx, iny;

	for (outy = 0; outy < outheight; outy++)
	{
		for (outx = 0; outx < outwidth; outx++)
		{
			iny = outy >> 1;
			inx = outx >> 1;
			out[outy * outwidth + outx] = in[iny * inwidth + inx];
		}
	}

	return out;
}


unsigned *Image_Upscale32 (unsigned *in, int inwidth, int inheight)
{
	unsigned *out = (unsigned *) ri.Hunk_Alloc (inwidth * inheight * 4 * sizeof (unsigned));
	int outwidth = inwidth << 1;
	int outheight = inheight << 1;
	int outx, outy, inx, iny;

	for (outy = 0; outy < outheight; outy++)
	{
		for (outx = 0; outx < outwidth; outx++)
		{
			iny = outy >> 1;
			inx = outx >> 1;
			out[outy * outwidth + outx] = in[iny * inwidth + inx];
		}
	}

	return out;
}


/*
=================================================================

PCX LOADING

=================================================================
*/

/*
==============
Image_LoadPCX
==============
*/
void Image_LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;

	*pic = NULL;
	*palette = NULL;

	// load the file
	len = ri.FS_LoadFile (filename, (void **) &raw);

	if (!raw)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
		return;
	}

	// parse the PCX file
	pcx = (pcx_t *) raw;

	pcx->xmin = LittleShort (pcx->xmin);
	pcx->ymin = LittleShort (pcx->ymin);
	pcx->xmax = LittleShort (pcx->xmax);
	pcx->ymax = LittleShort (pcx->ymax);
	pcx->hres = LittleShort (pcx->hres);
	pcx->vres = LittleShort (pcx->vres);
	pcx->bytes_per_line = LittleShort (pcx->bytes_per_line);
	pcx->palette_type = LittleShort (pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a || pcx->version != 5 || pcx->encoding != 1 || pcx->bits_per_pixel != 8)
	{
		ri.Con_Printf (PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	out = ri.Hunk_Alloc ((pcx->ymax + 1) * (pcx->xmax + 1));

	*pic = out;
	pix = out;

	if (palette)
	{
		*palette = ri.Hunk_Alloc (768);
		memcpy (*palette, (byte *) pcx + len - 768, 768);
	}

	if (width) *width = pcx->xmax + 1;
	if (height) *height = pcx->ymax + 1;

	for (y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1)
	{
		for (x = 0; x <= pcx->xmax;)
		{
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while (runLength-- > 0)
				pix[x++] = dataByte;
		}
	}

	if (raw - (byte *) pcx > len)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		*pic = NULL;
	}

	ri.FS_FreeFile (pcx);
}


byte *Image_LoadPCX32 (char *name, int *width, int *height)
{
	byte *pic8 = NULL;
	byte *pal8 = NULL;
	byte *pic32 = NULL;

	Image_LoadPCX (name, &pic8, &pal8, width, height);

	if (pic8 && pal8)
		pic32 = (byte *) GL_Image8To32 (pic8, *width, *height, d_8to24table_solid);

	return pic32;
}


/*
=========================================================

TARGA LOADING

=========================================================
*/

/*
=============
Image_LoadTGA
=============
*/
byte *Image_LoadTGA (char *name, int *width, int *height)
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	int		length;
	TargaHeader		*targa_header;
	byte			*targa_rgba;
	byte *pic = NULL;

	// load the file
	length = ri.FS_LoadFile (name, (void **) &buffer);

	if (!buffer)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Bad tga file %s\n", name);
		return NULL;
	}

	buf_p = buffer;

	targa_header = (TargaHeader *) buf_p;
	buf_p += sizeof (TargaHeader);

	targa_header->colormap_index = LittleShort (targa_header->colormap_index);
	targa_header->colormap_length = LittleShort (targa_header->colormap_length);

	targa_header->x_origin = LittleShort (targa_header->x_origin);
	targa_header->y_origin = LittleShort (targa_header->y_origin);
	targa_header->width = LittleShort (targa_header->width);
	targa_header->height = LittleShort (targa_header->height);

	if (targa_header->image_type != 2 && targa_header->image_type != 10)
		ri.Sys_Error (ERR_DROP, "Image_LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header->colormap_type != 0 || (targa_header->pixel_size != 32 && targa_header->pixel_size != 24))
		ri.Sys_Error (ERR_DROP, "Image_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header->width;
	rows = targa_header->height;
	numPixels = columns * rows;

	if (width) *width = columns;
	if (height) *height = rows;

	targa_rgba = ri.Hunk_Alloc (numPixels * 4);
	pic = targa_rgba;

	if (targa_header->id_length != 0)
		buf_p += targa_header->id_length;  // skip TARGA image comment

	if (targa_header->image_type == 2)
	{
		// Uncompressed, RGB images
		for (row = rows - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + row * columns * 4;

			for (column = 0; column < columns; column++)
			{
				unsigned char red, green, blue, alphabyte;

				switch (targa_header->pixel_size)
				{
				case 24:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if (targa_header->image_type == 10)
	{
		// Runlength encoded RGB images
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;

		for (row = rows - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + row * columns * 4;

			for (column = 0; column < columns;)
			{
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80)
				{
					// run-length packet
					switch (targa_header->pixel_size)
					{
					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = 255;
						break;

					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						break;
					}

					for (j = 0; j < packetSize; j++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;

						if (column == columns)
						{
							// run spans across rows
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else
				{
					// non run-length packet
					for (j = 0; j < packetSize; j++)
					{
						switch (targa_header->pixel_size)
						{
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;

						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						}

						column++;

						if (column == columns)
						{
							// pixel packet run spans across rows
							column = 0;

							if (row > 0)
								row--;
							else goto breakOut;

							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
breakOut:;
		}
	}

	ri.FS_FreeFile (buffer);
	return pic;
}


/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct floodfill_s {
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
			{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
			} \
				else if (pos[off] != 255) fdc = pos[off]; \
}


void R_FloodFillSkin (byte *skin, int skinwidth, int skinheight)
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			*fifo = (floodfill_t *) ri.Hunk_Alloc (FLOODFILL_FIFO_SIZE * sizeof (floodfill_t));
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;

		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
		{
			if (d_8to24table_solid[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP (-1, -1, 0);
		if (x < skinwidth - 1)	FLOODFILL_STEP (1, 1, 0);
		if (y > 0)				FLOODFILL_STEP (-skinwidth, 0, -1);
		if (y < skinheight - 1)	FLOODFILL_STEP (skinwidth, 0, 1);
		skin[x + skinwidth * y] = fdc;
	}
}

//=======================================================


unsigned *Image_ResampleToSize (unsigned *in, int inwidth, int inheight, int outwidth, int outheight)
{
	// can this ever happen???
	if (outwidth == inwidth && outheight == inheight)
		return in;
	else
	{
		int i, j;

		unsigned *out = (unsigned *) ri.Hunk_Alloc (outwidth * outheight * 4);
		unsigned *p1 = (unsigned *) ri.Hunk_Alloc (outwidth * 4);
		unsigned *p2 = (unsigned *) ri.Hunk_Alloc (outwidth * 4);

		unsigned fracstep = inwidth * 0x10000 / outwidth;
		unsigned frac = fracstep >> 2;

		for (i = 0; i < outwidth; i++)
		{
			p1[i] = 4 * (frac >> 16);
			frac += fracstep;
		}

		frac = 3 * (fracstep >> 2);

		for (i = 0; i < outwidth; i++)
		{
			p2[i] = 4 * (frac >> 16);
			frac += fracstep;
		}

		for (i = 0; i < outheight; i++)
		{
			unsigned *outrow = out + (i * outwidth);
			unsigned *inrow0 = in + inwidth * (int) (((i + 0.25f) * inheight) / outheight);
			unsigned *inrow1 = in + inwidth * (int) (((i + 0.75f) * inheight) / outheight);

			for (j = 0; j < outwidth; j++)
			{
				byte *pix1 = (byte *) inrow0 + p1[j];
				byte *pix2 = (byte *) inrow0 + p2[j];
				byte *pix3 = (byte *) inrow1 + p1[j];
				byte *pix4 = (byte *) inrow1 + p2[j];

				// don't gamma correct the alpha channel
				((byte *) &outrow[j])[0] = AverageMipGC (pix1[0], pix2[0], pix3[0], pix4[0]);
				((byte *) &outrow[j])[1] = AverageMipGC (pix1[1], pix2[1], pix3[1], pix4[1]);
				((byte *) &outrow[j])[2] = AverageMipGC (pix1[2], pix2[2], pix3[2], pix4[2]);
				((byte *) &outrow[j])[3] = AverageMip (pix1[3], pix2[3], pix3[3], pix4[3]);
			}
		}

		return out;
	}
}


unsigned *Image_MipReduceLinearFilter (unsigned *in, int inwidth, int inheight)
{
	// round down to meet np2 specification
	int outwidth = (inwidth > 1) ? (inwidth >> 1) : 1;
	int outheight = (inheight > 1) ? (inheight >> 1) : 1;

	// and run it through the regular resampling func
	return Image_ResampleToSize (in, inwidth, inheight, outwidth, outheight);
}


unsigned *Image_MipReduceBoxFilter (unsigned *data, int width, int height)
{
	// because each SRD must have it's own data we can't mipmap in-place otherwise we'll corrupt the previous miplevel
	unsigned *trans = (unsigned *) ri.Hunk_Alloc ((width >> 1) * (height >> 1) * 4);
	byte *in = (byte *) data;
	byte *out = (byte *) trans;
	int i, j;

	// do this after otherwise it will interfere with the allocation size above
	width <<= 2;
	height >>= 1;

	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
			// don't gamma correct the alpha channel
			out[0] = AverageMipGC (in[0], in[4], in[width + 0], in[width + 4]);
			out[1] = AverageMipGC (in[1], in[5], in[width + 1], in[width + 5]);
			out[2] = AverageMipGC (in[2], in[6], in[width + 2], in[width + 6]);
			out[3] = AverageMip (in[3], in[7], in[width + 3], in[width + 7]);
		}
	}

	return trans;
}


void Image_QuakePalFromPCXPal (unsigned *qpal, const byte *pcxpal, int flags)
{
	int i;

	for (i = 0; i < 256; i++, pcxpal += 3)
	{
		int r = pcxpal[0];
		int g = pcxpal[1];
		int b = pcxpal[2];

		if (flags & TEX_TRANS33)
			qpal[i] = (85 << 24) | (r << 0) | (g << 8) | (b << 16);
		else if (flags & TEX_TRANS66)
			qpal[i] = (170 << 24) | (r << 0) | (g << 8) | (b << 16);
		else qpal[i] = (255 << 24) | (r << 0) | (g << 8) | (b << 16);
	}

	if (flags & TEX_ALPHA)
		qpal[255] = 0;	// 255 is transparent
}


/*
===============
Draw_GetPalette
===============
*/
int Draw_GetPalette (void)
{
	int		i;
	byte	*pic, *pal;
	int		width, height;

	// get the palette
	Image_LoadPCX ("pics/colormap.pcx", &pic, &pal, &width, &height);

	if (!pal)
		ri.Sys_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");

	Image_QuakePalFromPCXPal (d_8to24table_solid, pal, TEX_RGBA8);
	Image_QuakePalFromPCXPal (d_8to24table_alpha, pal, TEX_ALPHA);
	Image_QuakePalFromPCXPal (d_8to24table_trans33, pal, TEX_TRANS33);
	Image_QuakePalFromPCXPal (d_8to24table_trans66, pal, TEX_TRANS66);
	ri.Hunk_FreeAll ();

	// gamma-correct to 16-bit precision, average, then mix back down to 8-bit precision so that we don't lose ultra-darks in the correction process
	for (i = 0; i < 256; i++) image_mipgammatable[i] = Image_GammaVal8to16 (i, 2.2f);
	for (i = 0; i < 65536; i++) image_mipinversegamma[i] = Image_GammaVal16to8 (i, 1.0f / 2.2f);

	return 0;
}


unsigned *GL_Image8To32 (byte *data, int width, int height, unsigned *palette)
{
	unsigned	*trans = (unsigned *) ri.Hunk_Alloc (width * height * sizeof (unsigned));
	int			i, s = width * height;

	for (i = 0; i < s; i++)
	{
		int p = data[i];

		trans[i] = palette[p];

		if (p == 255)
		{
			// transparent, so scan around for another color to avoid alpha fringes
			// FIXME: do a full flood fill so mips work...
			if (i > width && data[i - width] != 255)
				p = data[i - width];
			else if (i < s - width && data[i + width] != 255)
				p = data[i + width];
			else if (i > 0 && data[i - 1] != 255)
				p = data[i - 1];
			else if (i < s - 1 && data[i + 1] != 255)
				p = data[i + 1];
			else p = 0;

			// copy rgb components
			((byte *) &trans[i])[0] = ((byte *) &palette[p])[0];
			((byte *) &trans[i])[1] = ((byte *) &palette[p])[1];
			((byte *) &trans[i])[2] = ((byte *) &palette[p])[2];
		}
	}

	return trans;
}


void Image_CollapseRowPitch (unsigned *data, int width, int height, int pitch)
{
	if (width != pitch)
	{
		int h, w;
		unsigned *out = data;

		// as a minor optimization we can skip the first row
		// since out and data point to the same this is OK
		out += width;
		data += pitch;

		for (h = 1; h < height; h++)
		{
			for (w = 0; w < width; w++)
				out[w] = data[w];

			out += width;
			data += pitch;
		}
	}
}


void Image_Compress32To24 (byte *data, int width, int height)
{
	int h, w;
	byte *out = data;

	for (h = 0; h < height; h++)
	{
		for (w = 0; w < width; w++, data += 4, out += 3)
		{
			out[0] = data[0];
			out[1] = data[1];
			out[2] = data[2];
		}
	}
}


void Image_Compress32To24RGBtoBGR (byte *data, int width, int height)
{
	int h, w;
	byte *out = data;

	for (h = 0; h < height; h++)
	{
		for (w = 0; w < width; w++, data += 4, out += 3)
		{
			out[0] = data[2];
			out[1] = data[1];
			out[2] = data[0];
		}
	}
}


void Image_WriteDataToTGA (char *name, void *data, int width, int height, int bpp)
{
	if ((bpp == 24 || bpp == 8) && name && data && width > 0 && height > 0)
	{
		FILE *f = fopen (name, "wb");

		if (f)
		{
			byte header[18];

			memset (header, 0, 18);

			header[2] = 2;
			header[12] = width & 255;
			header[13] = width >> 8;
			header[14] = height & 255;
			header[15] = height >> 8;
			header[16] = bpp;
			header[17] = 0x20;

			fwrite (header, 18, 1, f);
			fwrite (data, (width * height * bpp) >> 3, 1, f);

			fclose (f);
		}
	}
}


void Image_ApplyTranslationRGB (byte *rgb, int size, byte *table)
{
	int i;

	for (i = 0; i < size; i++, rgb += 3)
	{
		rgb[0] = table[rgb[0]];
		rgb[1] = table[rgb[1]];
		rgb[2] = table[rgb[2]];
	}
}


// ==============================================================================
//
//  STB - handles TGA/PNG/BMP/JPG loading
//
// ==============================================================================

void *IMG_Alloc (int size)
{
	return ri.Hunk_Alloc (size);
}

// our free implementation is just a stub and our realloc doesn't free the old allocation
void IMG_Free (void *ptr) {}

// we can't support a custom realloc with our current hunk implementation as we don't track allocation sizes, so we implement STBI_REALLOC_SIZED instead
void *IMG_Realloc (void *p, int oldsz, int newsz)
{
	void *q = (void *) ri.Hunk_Alloc (newsz);
	memcpy (q, p, oldsz);
	return q;
}

// hostile takeover!
#define STBI_MALLOC IMG_Alloc
#define STBI_REALLOC_SIZED IMG_Realloc
#define STBI_FREE IMG_Free


// needs to be defined before including stb_image.h
#define STB_IMAGE_IMPLEMENTATION

// these are the only types we want to have code for
#define STBI_ONLY_PNG

// !!!!! a 2048x2048 skybox is 100mb !!!!!
#define STBI_MAX_DIMENSIONS	2048

// and now we can include it
#include "stb_image.h"


byte *Image_LoadPNG (char *name, int *width, int *height)
{
	// take it all in one go
	byte *buf = NULL;
	int len = 0;

	if ((len = ri.FS_LoadFile (name, (void **) &buf)) != -1)
	{
		// attempt to load as 4-component RGBA
		int channels = 0;
		byte *data = stbi_load_from_memory (buf, len, width, height, &channels, 4);

		// free it
		ri.FS_FreeFile (buf);

		// and now we can return whatever we got
		return data;
	}

	// always fails
	return NULL;
}

