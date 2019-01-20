#ifndef BMPFONTGENERATOR_HPP
#define BMPFONTGENERATOR_HPP

#include "FontObject.hpp"

class BmpFontGenerator : public FontObject {
public:
	BmpFontGenerator();

	void set_map_output_file(const ConstString & path){
		m_map_output_file = path;
	}


	int update_map(const ConstString & source, const ConstString & map);
	int generate_font_file(const ConstString & destination);

	var::Vector<sg_font_char_t> & character_list(){ return m_character_list; }
	const var::Vector<sg_font_char_t> & character_list() const { return m_character_list; }

	var::Vector<sg_font_kerning_pair_t> & kerning_pair_list(){ return m_kerning_pair_list; }
	const var::Vector<sg_font_kerning_pair_t> & kerning_pair_list() const { return m_kerning_pair_list; }

	var::Vector<Bitmap> & bitmap_list(){ return m_bitmap_list; }
	const var::Vector<Bitmap> & bitmap_list() const { return m_bitmap_list; }

	void set_is_ascii(bool value = true){ m_is_ascii = true; }

private:
	int generate_map_file(const sg_font_header_t & header, const var::Vector<Bitmap> & master_canvas_list);

	Region find_space_on_canvas(Bitmap & canvas, Area dimensions);
	String parse_map_line(const var::ConstString & title,const String & line);
	var::String m_map_output_file;
	bool m_is_ascii;
	var::Vector<Bitmap> build_master_canvas(const sg_font_header_t & header);
	var::Vector<sg_font_char_t> m_character_list;
	var::Vector<sg_font_kerning_pair_t> m_kerning_pair_list;
	var::Vector<Bitmap> m_bitmap_list;
};

#endif // BMPFONTGENERATOR_HPP
