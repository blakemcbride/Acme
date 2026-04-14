/*
 * FreeType ABI shim.
 *
 * MSYS/Cygwin gcc uses LP64 (long=8 bytes) but the mingw64 FreeType
 * DLL uses LLP64 (long=4 bytes).  FT_Long, FT_Pos, FT_Fixed are all
 * "signed long", so every FT_FaceRec field offset differs between the
 * two ABIs.  This shim is compiled with mingw64 gcc so it sees the
 * correct struct layout, and exposes only int/pointer types across
 * the boundary.
 */
#ifndef FT_SHIM_H
#define FT_SHIM_H

typedef void *FTShimLibrary;
typedef void *FTShimFace;

int ftshim_init(FTShimLibrary *lib);
int ftshim_new_face(FTShimLibrary lib, const char *file, int index, FTShimFace *face);
void ftshim_done_face(FTShimFace face);

int ftshim_is_scalable(FTShimFace face);
int ftshim_units_per_em(FTShimFace face);
int ftshim_ascender(FTShimFace face);
int ftshim_descender(FTShimFace face);
int ftshim_max_advance_width(FTShimFace face);

unsigned int ftshim_get_first_char(FTShimFace face, unsigned int *glyph_index);
unsigned int ftshim_get_next_char(FTShimFace face, unsigned int charcode, unsigned int *glyph_index);

int ftshim_set_char_size(FTShimFace face, int char_width, int char_height, int hdpi, int vdpi);
unsigned int ftshim_get_char_index(FTShimFace face, unsigned int charcode);
int ftshim_load_glyph(FTShimFace face, unsigned int glyph_index, int load_flags);

int ftshim_glyph_advance_x(FTShimFace face);
int ftshim_glyph_bitmap_left(FTShimFace face);
int ftshim_glyph_bitmap_top(FTShimFace face);
unsigned int ftshim_glyph_bitmap_rows(FTShimFace face);
unsigned int ftshim_glyph_bitmap_width(FTShimFace face);
int ftshim_glyph_bitmap_pitch(FTShimFace face);
unsigned char *ftshim_glyph_bitmap_buffer(FTShimFace face);

int ftshim_load_render(void);
int ftshim_load_no_autohint(void);
int ftshim_load_target_mono(void);

#endif
