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

	int convert_directory(const ConstString & dir_path, bool overwrite = false, int verbose = 1);

	int generate_font_file(const ConstString & destination);

	Vector<sg_font_char_t> & character_list(){ return m_character_list; }
	const Vector<sg_font_char_t> & character_list() const { return m_character_list; }

	Vector<sg_font_kerning_pair_t> & kerning_pair_list(){ return m_kerning_pair_list; }
	const Vector<sg_font_kerning_pair_t> & kerning_pair_list() const { return m_kerning_pair_list; }

	Vector<Bitmap> & bitmap_list(){ return m_bitmap_list; }
	const Vector<Bitmap> & bitmap_list() const { return m_bitmap_list; }

private:

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

	int load_font_from_bmp_files(const ConstString & def_file,
			const ConstString & bitmap_file,
			const ConstString & font_file,
			const ConstString & charset = "",
			int verbose = 0);

	int load_char(bmpfont_char_t & c, const Token & t);
	int load_kerning(bmpfont_kerning_t & c, const Token & t);
	int load_info(bmpfont_hdr_t & hdr, const Token & t);
	void show_char(Bmp & bmp, bmpfont_char_t c);

	int get_max(File & def, int & w, int &h);
	int get_kerning_count(File & def);
	int get_char(File & def, bmpfont_char_t & d, uint8_t ascii);
	int get_kerning(File & def, bmpfont_kerning_t & k);
	int scan_char(File & def, bmpfont_char_t & d);
	int get_bitmap(Bmp & bmp, bmpfont_char_t c, Bitmap & canvas, sg_point_t loc);

	Region save_region_on_canvas(Bitmap & canvas, Dim dimensions, int grid);

	Region find_space_on_canvas(Bitmap & canvas, Dim dimensions);


	Vector<sg_font_char_t> m_character_list;
	Vector<sg_font_kerning_pair_t> m_kerning_pair_list;
	Vector<Bitmap> m_bitmap_list;


};

#endif /* BMPFONTMANAGER_HPP_ */
