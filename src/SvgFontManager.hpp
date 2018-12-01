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
	int process_svg_icon_file(const ConstString & path);

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

private:

	BmpFontManager m_bmp_font_manager; //used for exporting to bmp

	u16 m_canvas_size;
	Dim m_downsample;
	Dim m_canvas_dimensions;
	Point m_canvas_origin;
	u16 m_pour_grid_size;
	bool m_is_show_canvas;

	int parse_svg_path(const char * d);

	static const char * path_commands_space(){ return "MmCcSsLlHhVvQqTtAaZz \n\t"; }
	static const char * path_commands(){ return "MmCcSsLlHhVvQqTtAaZz"; }
	static bool is_command_char(char c);

	enum {
		NO_STATE,
		MOVETO_STATE,
		LINETO_STATE,
		TOTAL_STATE
	};

	int parse_bounds(const char * value, u32 units_per_em);

	int parse_path_moveto_absolute(const char * path);
	int parse_path_moveto_relative(const char * path);
	int parse_path_lineto_absolute(const char * path);
	int parse_path_lineto_relative(const char * path);
	int parse_path_horizontal_lineto_absolute(const char * path);
	int parse_path_horizontal_lineto_relative(const char * path);
	int parse_path_vertical_lineto_absolute(const char * path);
	int parse_path_vertical_lineto_relative(const char * path);
	int parse_path_quadratic_bezier_absolute(const char * path);
	int parse_path_quadratic_bezier_relative(const char * path);
	int parse_path_quadratic_bezier_short_absolute(const char * path);
	int parse_path_quadratic_bezier_short_relative(const char * path);
	int parse_path_cubic_bezier_absolute(const char * path);
	int parse_path_cubic_bezier_relative(const char * path);
	int parse_path_cubic_bezier_short_absolute(const char * path);
	int parse_path_cubic_bezier_short_relative(const char * path);
	int parse_close_path(const char * path);

	int parse_number_arguments(const char * path, float * dest, u32 n);

	int seek_path_command(const char * path);

	Point convert_svg_coord(float x, float y, bool is_absolute = true);

	void analyze_icon(Bitmap & bitmap, sg_vector_path_t & vector_path, const VectorMap & map, bool recheck);
	void shift_icon(sg_vector_path_icon_t & icon, Point shift);
	void scale_icon(sg_vector_path_icon_t & icon, float scale);

	enum {
		PATH_DESCRIPTION_MAX = 256,
	};

	int m_state;
	Point m_current_point;
	Point m_control_point;
	Region m_bounds;
	float m_scale;
	u16 m_point_size;
	int m_object;
	sg_vector_path_description_t m_path_description_list[PATH_DESCRIPTION_MAX];

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
