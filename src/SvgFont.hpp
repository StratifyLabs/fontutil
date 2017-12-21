/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#ifndef SVGFONT_HPP_
#define SVGFONT_HPP_

#include <sapi/sgfx.hpp>

class SvgFont {
public:
	SvgFont();

	int convert_file(const char * path);

private:
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

	int parse_bounds(const char * value);

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
		MAX_FILL_POINTS = 32
	};

	int m_state;
	Point m_current_point;
	Point m_control_point;
	sg_region_t m_bounds;
	float m_scale;
	int m_object;
	sg_vector_path_description_t m_path_description_list[PATH_DESCRIPTION_MAX];

	static int calculate_pour_points(Bitmap & bitmap, const Bitmap & fill_points, sg_point_t * points, sg_size_t max_points);
	static void find_all_fill_points(const Bitmap & bitmap, Bitmap & fill_points, const Region & region, sg_size_t grid);
	static bool is_fill_point(const Bitmap & bitmap, sg_point_t point, const Region & region);


};

#endif /* SVGFONT_HPP_ */
