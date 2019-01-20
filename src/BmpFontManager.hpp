/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#ifndef BMPFONTMANAGER_HPP_
#define BMPFONTMANAGER_HPP_

#include "FontObject.hpp"
#include "BmpFontGenerator.hpp"

class BmpFontManager : public FontObject {
public:
	BmpFontManager();

	int convert_font(const ConstString & path);
	int convert_directory(const ConstString & dir_path, bool overwrite = false);


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


	int load_bmp_characters(const File & def);
	int populate_lists_from_bitmap_definition(const File & def, const Bmp & bitmap_file);
	int populate_kerning_pair_list_from_bitmap_definition(const File & def);

	int create_color_index(const Bmp & bitmap_file);

	int load_char(bmpfont_char_t & c, const Tokenizer & t);
	int get_char(const File & def, bmpfont_char_t & d, uint8_t ascii);
	Bitmap get_bitmap(const Bmp & bmp, bmpfont_char_t c);

	int add_character_to_lists(const bmpfont_char_t & d, const Bmp & bitmap_file);

	var::Vector<u32> m_bmp_color_index;


	var::Vector<bmpfont_char_t> m_bmp_characters;

	BmpFontGenerator m_generator;

};

#endif /* BMPFONTMANAGER_HPP_ */
