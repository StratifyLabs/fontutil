/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#ifndef SVGFONTMANAGER_HPP_
#define SVGFONTMANAGER_HPP_


#include "FontObject.hpp"
#include "BmpFontGenerator.hpp"

class FillPoint {
public:

	FillPoint(const Point & point, sg_size_t spacing){
		m_point = point;
		m_spacing = spacing;
	}

	Point point() const { return m_point; }
	sg_size_t spacing() const { return m_spacing; }

	bool operator < (const FillPoint & a) const {
		return spacing() < a.spacing();
	}

	bool operator > (const FillPoint & a) const {
		return spacing() > a.spacing();
	}

private:
	sg_size_t m_spacing;
	sg_point_t m_point;
};

class SvgFontManager : public FontObject {
public:
	SvgFontManager();

	int process_svg_font_file(const ConstString & path);
	int process_svg_icon_file(const ConstString & source, const ConstString & dest);

	void set_map_output_file(const ConstString & path){
		m_bmp_font_generator.set_map_output_file(path);
	}

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

	void set_downsample_factor(const Area & dim){
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

	BmpFontGenerator m_bmp_font_generator; //used for exporting to bmp

	u16 m_canvas_size;
	Area m_downsample;
	Area m_canvas_dimensions;
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

	var::Vector<sg_vector_path_description_t> convert_svg_path(Bitmap & canvas, const var::ConstString & d, const Area & canvas_dimensions, sg_size_t grid_size, bool is_fit_icon);
	var::Vector<sg_vector_path_description_t> process_svg_path(const ConstString & path);

	Region parse_bounds(const ConstString & value);
	Area calculate_canvas_dimension(const Region & bounds, sg_size_t canvas_size);
	Point calculate_canvas_origin(const Region & bounds, const Area & canvas_dimensions);

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


	var::Vector<sg_font_char_t> m_font_character_list;

	static var::Vector<sg_point_t> calculate_pour_points(Bitmap & bitmap, const var::Vector<FillPoint> & fill_points);
	static var::Vector<FillPoint> find_all_fill_points(const Bitmap & bitmap, const Region & region, sg_size_t grid);
	static sg_size_t is_fill_point(const Bitmap & bitmap, sg_point_t point, const Region & region);

	int process_glyph(const JsonObject & glyph);
	int process_hkern(const JsonObject & kerning);

	sg_size_t map_svg_value_to_bitmap(u32 value);


};

#endif /* SVGFONTMANAGER_HPP_ */
