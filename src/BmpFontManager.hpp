/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#ifndef BMPFONTMANAGER_HPP_
#define BMPFONTMANAGER_HPP_

#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/fmt.hpp>
#include <sapi/sgfx.hpp>
#include "ApplicationPrinter.hpp"

class BmpFontManager : public ApplicationPrinter {
public:
	BmpFontManager();

	typedef struct {
		u16 num_chars;
		u16 num_kernings;
		u16 size;
	} bmpfont_hdr_t;

	typedef struct {
		u16 id;
		u16 x;
		u16 y;
		u16 width;
		u16 height;
		s16 xoffset;
		s16 yoffset;
		u16 xadvance;
		u16 page;
		u16 channel;
	} bmpfont_char_t;

	typedef struct {
		u16 first;
		u16 second;
		s16 amount;
	} bmpfont_kerning_t;

	static int convert_directory(const char * dir_path, bool overwrite = false, int verbose = 1);

	static int gen_fonts(const ConstString & def_file,
			const ConstString & bitmap_file,
			const ConstString & font_file,
			const ConstString & charset = "",
			int verbose = 0);

	static int load_char(bmpfont_char_t & c, const Token & t);
	static int load_kerning(bmpfont_kerning_t & c, const Token & t);
	static int load_info(bmpfont_hdr_t & hdr, const Token & t);
	static void show_char(Bmp & bmp, bmpfont_char_t c);

	static int get_max(File & def, int & w, int &h);
	static int get_kerning_count(File & def);
	static int get_char(File & def, bmpfont_char_t & d, uint8_t ascii);
	static int get_kerning(File & def, bmpfont_kerning_t & k);
	static int scan_char(File & def, bmpfont_char_t & d);
	static int get_bitmap(Bmp & bmp, bmpfont_char_t c, Bitmap & canvas, sg_point_t loc);

	static Region save_region_on_canvas(Bitmap & canvas, Dim dimensions, int grid);



};

#endif /* BMPFONTMANAGER_HPP_ */
