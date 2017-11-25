/*
 * BmpFont.hpp
 *
 *  Created on: Oct 19, 2017
 *      Author: tgil
 */

#ifndef BMPFONT_HPP_
#define BMPFONT_HPP_

#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/fmt.hpp>
#include <sapi/sgfx.hpp>


class BmpFont {
public:
	BmpFont();
	virtual ~BmpFont();

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

	static int convert_directory(const char * dir_path);

	static int gen_fonts(const char * def_file,
			const char * bitmap_file,
			const char * font_file,
			const char * charset = 0);

	static int load_char(bmpfont_char_t & c, const Token & t);
	static int load_kerning(bmpfont_kerning_t & c, const Token & t);
	static int load_info(bmpfont_hdr_t & hdr, const Token & t);
	static void show_char(Bmp & bmp, bmpfont_char_t c);

	static int get_max(File & def, int & w, int &h);
	static int get_kerning_count(File & def);
	static int get_char(File & def, bmpfont_char_t & d, uint8_t ascii);
	static int get_kerning(File & def, bmpfont_kerning_t & k);
	static int scan_char(File & def, bmpfont_char_t & d);
	static int get_bitmap(Bmp & bmp, bmpfont_char_t c, Bitmap * master);



};

#endif /* BMPFONT_HPP_ */
