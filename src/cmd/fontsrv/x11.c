#include <u.h>

#include <fontconfig/fontconfig.h>

#if defined(__CYGWIN__) || defined(__MSYS__)
#include "ft-shim.h"
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "a.h"

static FcConfig    *fc;
static int         dpi = 96;

#if defined(__CYGWIN__) || defined(__MSYS__)
static FTShimLibrary lib;
#else
static FT_Library  lib;
#endif

void
loadfonts(void)
{
	int i;
	int e;
	FcFontSet *sysfonts;

#if defined(_WIN32)
	/*
	 * Load embedded fontconfig config programmatically so we don't
	 * need a fonts.conf file on disk next to the binary.
	 */
	{
		static const FcChar8 embedded_conf[] =
			"<?xml version=\"1.0\"?>\n"
			"<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
			"<fontconfig>\n"
			"  <dir>C:\\Windows\\Fonts</dir>\n"
			"  <dir>WINDOWSFONTDIR</dir>\n"
			"  <cachedir>LOCAL_APPDATA_FONTCONFIG_CACHE</cachedir>\n"
			"  <match target=\"pattern\">\n"
			"    <test qual=\"any\" name=\"family\"><string>mono</string></test>\n"
			"    <edit name=\"family\" mode=\"assign\" binding=\"same\"><string>Consolas</string></edit>\n"
			"  </match>\n"
			"</fontconfig>\n";
		fc = FcConfigCreate();
		if(fc == NULL || !FcConfigParseAndLoadFromMemory(fc, embedded_conf, FcTrue)
		  || !FcConfigBuildFonts(fc)){
			fprint(2, "fontconfig initialization failed\n");
			exits("fontconfig failed");
		}
		FcConfigSetCurrent(fc);
	}
#else
	if(!FcInit() || (fc=FcInitLoadConfigAndFonts()) == NULL) {
		fprint(2, "fontconfig initialization failed\n");
		exits("fontconfig failed");
	}
#endif

#if defined(__CYGWIN__) || defined(__MSYS__)
	e = ftshim_init(&lib);
#else
	e = FT_Init_FreeType(&lib);
#endif
	if(e) {
		fprint(2, "freetype initialization failed: %d\n", e);
		exits("freetype failed");
	}

	sysfonts = FcConfigGetFonts(fc, FcSetSystem);

	xfont = emalloc9p(sysfonts->nfont*sizeof xfont[0]);
	memset(xfont, 0, sysfonts->nfont*sizeof xfont[0]);
	for(i=0; i<sysfonts->nfont; i++) {
		FcChar8 *fullname, *fontfile;
		int index;
		FcPattern *pat = sysfonts->fonts[i];

		if(FcPatternGetString(pat, FC_POSTSCRIPT_NAME, 0, &fullname) != FcResultMatch ||
		   FcPatternGetString(pat, FC_FILE, 0, &fontfile) != FcResultMatch     ||
		   FcPatternGetInteger(pat, FC_INDEX, 0, &index) != FcResultMatch)
			continue;

		xfont[nxfont].name     = strdup((char*)fullname);
		xfont[nxfont].fontfile = strdup((char*)fontfile);
		xfont[nxfont].index    = index;
		nxfont++;
	}

	FcFontSetDestroy(sysfonts);
}

void
load(XFont *f)
{
	int e;
	int i;

#if defined(__CYGWIN__) || defined(__MSYS__)
	FTShimFace face;
	unsigned int charcode, glyph_index;

	if(f->loaded)
		return;

	e = ftshim_new_face(lib, f->fontfile, f->index, &face);
	if(e){
		fprint(2, "load failed for %s (%s) index:%d\n", f->name, f->fontfile, f->index);
		return;
	}
	if(!ftshim_is_scalable(face)) {
		fprint(2, "%s is a non scalable font, skipping\n", f->name);
		ftshim_done_face(face);
		f->loaded = 1;
		return;
	}
	f->unit = ftshim_units_per_em(face);
	f->height = (int)((ftshim_ascender(face) - ftshim_descender(face)) * 1.35);
	f->originy = ftshim_descender(face) * 1.35;
	for(charcode=ftshim_get_first_char(face, &glyph_index); glyph_index != 0;
		charcode=ftshim_get_next_char(face, charcode, &glyph_index)) {

		int idx = charcode/SubfontSize;

		if(charcode > Runemax)
			break;

		if(!f->range[idx])
			f->range[idx] = 1;
	}
	ftshim_done_face(face);
#else
	FT_Face face;
	FT_ULong ft_charcode;
	FT_UInt ft_glyph_index;

	if(f->loaded)
		return;

	e = FT_New_Face(lib, f->fontfile, f->index, &face);
	if(e){
		fprint(2, "load failed for %s (%s) index:%d\n", f->name, f->fontfile, f->index);
		return;
	}
	if(!FT_IS_SCALABLE(face)) {
		fprint(2, "%s is a non scalable font, skipping\n", f->name);
		FT_Done_Face(face);
		f->loaded = 1;
		return;
	}
	f->unit = face->units_per_EM;
	f->height = (int)((face->ascender - face->descender) * 1.35);
	f->originy = face->descender * 1.35;

	for(ft_charcode=FT_Get_First_Char(face, &ft_glyph_index); ft_glyph_index != 0;
		ft_charcode=FT_Get_Next_Char(face, ft_charcode, &ft_glyph_index)) {

		int idx = ft_charcode/SubfontSize;

		if(ft_charcode > Runemax)
			break;

		if(!f->range[idx])
			f->range[idx] = 1;
	}
	FT_Done_Face(face);
#endif

	if(!f->range[0])
		f->range[0] = 1;

	for(i=0; i<nelem(f->range); i++)
		if(f->range[i])
			f->file[f->nfile++] = i;

	f->loaded = 1;
}

Memsubfont*
mksubfont(XFont *xf, char *name, int lo, int hi, int size, int antialias)
{
	int e;
	Memimage *m, *mc, *m1;
	double pixel_size;
	int w, x, y, y0;
	int i;
	Fontchar *fc0, *fc1;
	Memsubfont *sf;

#if defined(__CYGWIN__) || defined(__MSYS__)
	FTShimFace face;
	int FT_LOAD_RENDER_val = ftshim_load_render();
	int FT_LOAD_NO_AUTOHINT_val = ftshim_load_no_autohint();
	int FT_LOAD_TARGET_MONO_val = ftshim_load_target_mono();

	e = ftshim_new_face(lib, xf->fontfile, xf->index, &face);
	if(e){
		fprint(2, "load failed for %s (%s) index:%d\n", xf->name, xf->fontfile, xf->index);
		return nil;
	}

	e = ftshim_set_char_size(face, 0, size<<6, dpi, dpi);
	if(e){
		fprint(2, "FT_Set_Char_Size failed\n");
		ftshim_done_face(face);
		return nil;
	}

	pixel_size = (dpi*size)/72.0;
	w = x = (int)((ftshim_max_advance_width(face)) * pixel_size/xf->unit + 0.99999999);
	y = (int)((ftshim_ascender(face) - ftshim_descender(face)) * pixel_size/xf->unit + 0.99999999);
	y0 = (int)((-ftshim_descender(face)) * pixel_size/xf->unit + 0.99999999);

	m = allocmemimage(Rect(0, 0, x*(hi+1-lo)+1, y+1), antialias ? GREY8 : GREY1);
	if(m == nil) {
		ftshim_done_face(face);
		return nil;
	}
	mc = allocmemimage(Rect(0, 0, x+1, y+1), antialias ? GREY8 : GREY1);
	if(mc == nil) {
		freememimage(m);
		ftshim_done_face(face);
		return nil;
	}
	memfillcolor(m, DBlack);
	memfillcolor(mc, DBlack);
	fc1 = malloc((hi+2 - lo) * sizeof fc1[0]);
	sf = malloc(sizeof *sf);
	if(fc1 == nil || sf == nil) {
		freememimage(m);
		freememimage(mc);
		free(fc1);
		free(sf);
		ftshim_done_face(face);
		return nil;
	}
	fc0 = fc1;

	x = 0;
	for(i=lo; i<=hi; i++, fc1++) {
		unsigned int k;
		int r;
		int advance;

		memfillcolor(mc, DBlack);

		fc1->x = x;
		fc1->top = 0;
		fc1->bottom = Dy(m->r);
		e = 1;
		k = ftshim_get_char_index(face, i);
		if(k != 0) {
			e = ftshim_load_glyph(face, k,
				FT_LOAD_RENDER_val|FT_LOAD_NO_AUTOHINT_val|(antialias ? 0:FT_LOAD_TARGET_MONO_val));
		}
		if(e || ftshim_glyph_advance_x(face) <= 0) {
			fc1->width = 0;
			fc1->left = 0;
			if(i == 0) {
				drawpjw(m, fc1, x, w, y, y - y0);
				x += fc1->width;
			}
			continue;
		}

		uchar *base = byteaddr(mc, mc->r.min);
		advance = (ftshim_glyph_advance_x(face)+32) >> 6;

		unsigned int brows = ftshim_glyph_bitmap_rows(face);
		int bpitch = ftshim_glyph_bitmap_pitch(face);
		uchar *bbuf = ftshim_glyph_bitmap_buffer(face);

		for(r=0; r < (int)brows; r++)
			memmove(base + r*mc->width*sizeof(u32int), bbuf + r*bpitch, bpitch);

		memimagedraw(m, Rect(x, 0, x + advance, y), mc,
			Pt(-ftshim_glyph_bitmap_left(face), -(y - y0 - ftshim_glyph_bitmap_top(face))),
			memopaque, ZP, S);

		fc1->width = advance;
		fc1->left = 0;
		x += advance;
	}
	fc1->x = x;
	ftshim_done_face(face);
#else
	FT_Face face;

	e = FT_New_Face(lib, xf->fontfile, xf->index, &face);
	if(e){
		fprint(2, "load failed for %s (%s) index:%d\n", xf->name, xf->fontfile, xf->index);
		return nil;
	}

	e = FT_Set_Char_Size(face, 0, size<<6, dpi, dpi);
	if(e){
		fprint(2, "FT_Set_Char_Size failed\n");
		FT_Done_Face(face);
		return nil;
	}

	pixel_size = (dpi*size)/72.0;
	w = x = (int)((face->max_advance_width) * pixel_size/xf->unit + 0.99999999);
	y = (int)((face->ascender - face->descender) * pixel_size/xf->unit + 0.99999999);
	y0 = (int)(-face->descender * pixel_size/xf->unit + 0.99999999);

	m = allocmemimage(Rect(0, 0, x*(hi+1-lo)+1, y+1), antialias ? GREY8 : GREY1);
	if(m == nil) {
		FT_Done_Face(face);
		return nil;
	}
	mc = allocmemimage(Rect(0, 0, x+1, y+1), antialias ? GREY8 : GREY1);
	if(mc == nil) {
		freememimage(m);
		FT_Done_Face(face);
		return nil;
	}
	memfillcolor(m, DBlack);
	memfillcolor(mc, DBlack);
	fc1 = malloc((hi+2 - lo) * sizeof fc1[0]);
	sf = malloc(sizeof *sf);
	if(fc1 == nil || sf == nil) {
		freememimage(m);
		freememimage(mc);
		free(fc1);
		free(sf);
		FT_Done_Face(face);
		return nil;
	}
	fc0 = fc1;

	x = 0;
	for(i=lo; i<=hi; i++, fc1++) {
		int k, r;
		int advance;

		memfillcolor(mc, DBlack);

		fc1->x = x;
		fc1->top = 0;
		fc1->bottom = Dy(m->r);
		e = 1;
		k = FT_Get_Char_Index(face, i);
		if(k != 0) {
			e = FT_Load_Glyph(face, k, FT_LOAD_RENDER|FT_LOAD_NO_AUTOHINT|(antialias ? 0:FT_LOAD_TARGET_MONO));
		}
		if(e || face->glyph->advance.x <= 0) {
			fc1->width = 0;
			fc1->left = 0;
			if(i == 0) {
				drawpjw(m, fc1, x, w, y, y - y0);
				x += fc1->width;
			}
			continue;
		}

		FT_Bitmap *bitmap = &face->glyph->bitmap;
		uchar *base = byteaddr(mc, mc->r.min);
		advance = (face->glyph->advance.x+32) >> 6;

		for(r=0; r < (int)bitmap->rows; r++)
			memmove(base + r*mc->width*sizeof(u32int), bitmap->buffer + r*bitmap->pitch, bitmap->pitch);

		memimagedraw(m, Rect(x, 0, x + advance, y), mc,
			Pt(-face->glyph->bitmap_left, -(y - y0 - face->glyph->bitmap_top)),
			memopaque, ZP, S);

		fc1->width = advance;
		fc1->left = 0;
		x += advance;
	}
	fc1->x = x;
	FT_Done_Face(face);
#endif

	if(x == 0)
		x = 1;
	if(y == 0)
		y = 1;
	if(antialias)
		x += -x & 3;
	else
		x += -x & 31;
	m1 = allocmemimage(Rect(0, 0, x, y), antialias ? GREY8 : GREY1);
	memimagedraw(m1, m1->r, m, m->r.min, memopaque, ZP, S);
	freememimage(m);
	freememimage(mc);

	sf->name = nil;
	sf->n = hi+1 - lo;
	sf->height = Dy(m1->r);
	sf->ascent = Dy(m1->r) - y0;
	sf->info = fc0;
	sf->bits = m1;

	return sf;
}
