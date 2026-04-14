/*
 * FreeType ABI shim — compiled with mingw64 gcc.
 * See ft-shim.h for rationale.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ft-shim.h"

int
ftshim_init(FTShimLibrary *lib)
{
	return FT_Init_FreeType((FT_Library*)lib);
}

int
ftshim_new_face(FTShimLibrary lib, const char *file, int index, FTShimFace *face)
{
	return FT_New_Face((FT_Library)lib, file, index, (FT_Face*)face);
}

void
ftshim_done_face(FTShimFace face)
{
	FT_Done_Face((FT_Face)face);
}

int
ftshim_is_scalable(FTShimFace face)
{
	return FT_IS_SCALABLE((FT_Face)face);
}

int
ftshim_units_per_em(FTShimFace face)
{
	return ((FT_Face)face)->units_per_EM;
}

int
ftshim_ascender(FTShimFace face)
{
	return ((FT_Face)face)->ascender;
}

int
ftshim_descender(FTShimFace face)
{
	return ((FT_Face)face)->descender;
}

int
ftshim_max_advance_width(FTShimFace face)
{
	return ((FT_Face)face)->max_advance_width;
}

unsigned int
ftshim_get_first_char(FTShimFace face, unsigned int *gi)
{
	FT_UInt g;
	FT_ULong c = FT_Get_First_Char((FT_Face)face, &g);
	*gi = g;
	return (unsigned int)c;
}

unsigned int
ftshim_get_next_char(FTShimFace face, unsigned int charcode, unsigned int *gi)
{
	FT_UInt g;
	FT_ULong c = FT_Get_Next_Char((FT_Face)face, charcode, &g);
	*gi = g;
	return (unsigned int)c;
}

int
ftshim_set_char_size(FTShimFace face, int cw, int ch, int hdpi, int vdpi)
{
	return FT_Set_Char_Size((FT_Face)face, cw, ch, hdpi, vdpi);
}

unsigned int
ftshim_get_char_index(FTShimFace face, unsigned int charcode)
{
	return FT_Get_Char_Index((FT_Face)face, charcode);
}

int
ftshim_load_glyph(FTShimFace face, unsigned int gi, int flags)
{
	return FT_Load_Glyph((FT_Face)face, gi, flags);
}

int
ftshim_glyph_advance_x(FTShimFace face)
{
	return (int)(((FT_Face)face)->glyph->advance.x);
}

int
ftshim_glyph_bitmap_left(FTShimFace face)
{
	return ((FT_Face)face)->glyph->bitmap_left;
}

int
ftshim_glyph_bitmap_top(FTShimFace face)
{
	return ((FT_Face)face)->glyph->bitmap_top;
}

unsigned int
ftshim_glyph_bitmap_rows(FTShimFace face)
{
	return ((FT_Face)face)->glyph->bitmap.rows;
}

unsigned int
ftshim_glyph_bitmap_width(FTShimFace face)
{
	return ((FT_Face)face)->glyph->bitmap.width;
}

int
ftshim_glyph_bitmap_pitch(FTShimFace face)
{
	return ((FT_Face)face)->glyph->bitmap.pitch;
}

unsigned char *
ftshim_glyph_bitmap_buffer(FTShimFace face)
{
	return ((FT_Face)face)->glyph->bitmap.buffer;
}

int ftshim_load_render(void) { return FT_LOAD_RENDER; }
int ftshim_load_no_autohint(void) { return FT_LOAD_NO_AUTOHINT; }
int ftshim_load_target_mono(void) { return FT_LOAD_TARGET_MONO; }
