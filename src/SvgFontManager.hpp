/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#ifndef SVGFONTMANAGER_HPP_
#define SVGFONTMANAGER_HPP_

#include <sapi/sgfx.hpp>
#include <sapi/var.hpp>
#include "ApplicationPrinter.hpp"
#include "BmpFontManager.hpp"

class SvgFontManager : public ApplicationPrinter {
public:
	SvgFontManager();

	int process_svg_font_file(const ConstString & path);
	int process_svg_icon_file(const ConstString & source, const ConstString & dest);

	void set_canvas_size(u16 size){
		m_canvas_size = size;
		if( size == 0 ){ m_canvas_size = 128; }
	}

	void set_show_canvas(bool value = true){
		m_is_show_canvas = value;
	}

	void set_pour_grid_size(u16 size){
		m_pour_grid_size = size;
		if( size == 0 ){ m_pour_grid_size = 8; }
	}

	void set_character_set(const ConstString & character_set){
		m_character_set = character_set;
		m_bmp_font_manager.set_is_ascii(false);
	}

	void set_downsample_factor(const Dim & dim){
		m_downsample = dim;
	}

	void set_flip_y(bool value = true){
		if( value ){
			m_scale_sign_y = -1;
		} else {
			m_scale_sign_y = 1;
		}
	}

private:

	int process_svg_icon(const JsonObject & object);

	BmpFontManager m_bmp_font_manager; //used for exporting to bmp

	u16 m_canvas_size;
	Dim m_downsample;
	Dim m_canvas_dimensions;
	Point m_canvas_origin;
	u16 m_pour_grid_size;
	bool m_is_show_canvas;

	int parse_svg_path(const char * d);

	static const ConstString path_commands_sign(){ return "MmCcSsLlHhVvQqTtAaZz-"; }
	static const ConstString path_commands_space(){ return "MmCcSsLlHhVvQqTtAaZz \n\t"; }
	static const ConstString path_commands(){ return "MmCcSsLlHhVvQqTtAaZz"; }
	static bool is_command_char(char c);

	enum {
		NO_STATE,
		MOVETO_STATE,
		LINETO_STATE,
		TOTAL_STATE
	};

	var::Vector<sg_vector_path_description_t> convert_svg_path(Bitmap & canvas, const var::ConstString & d, const Dim & canvas_dimensions, sg_size_t grid_size, bool is_fit_icon);
	var::Vector<sg_vector_path_description_t> process_svg_path(const ConstString & path);

	Region parse_bounds(const ConstString & value);
	Dim calculate_canvas_dimension(const Region & bounds, sg_size_t canvas_size);
	Point calculate_canvas_origin(const Region & bounds, const Dim & canvas_dimensions);

	Point convert_svg_coord(float x, float y, bool is_absolute = true);

	void fit_icon_to_canvas(Bitmap & bitmap, VectorPath & vector_path, const VectorMap & map, bool recheck);

	enum {
		PATH_DESCRIPTION_MAX = 256,
	};

	int m_state;
	Point m_current_point;
	Point m_control_point;
	char m_last_command_character;
	Region m_bounds;
	float m_scale;
	int m_scale_sign_y;
	u16 m_point_size;
	var::Vector<sg_vector_path_description_t> m_vector_path_icon_list;

	String m_character_set;

	var::Vector<sg_font_char_t> m_font_character_list;

	static var::Vector<sg_point_t> calculate_pour_points(Bitmap & bitmap, const Bitmap & fill_points);
	static void find_all_fill_points(const Bitmap & bitmap, Bitmap & fill_points, const Region & region, sg_size_t grid);
	static bool is_fill_point(const Bitmap & bitmap, sg_point_t point, const Region & region);

	int process_glyph(const JsonObject & glyph);
	int process_hkern(const JsonObject & kerning);

	sg_size_t map_svg_value_to_bitmap(u32 value);


};

#endif /* SVGFONTMANAGER_HPP_ */
