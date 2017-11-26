/*
 * Svg.hpp
 *
 *  Created on: Nov 25, 2017
 *      Author: tgil
 */

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

	enum {
		OBJECT_MAX = 192
	};

	int m_state;
	Point m_start_point;
	Point m_current_point;
	Point m_control_point;
	sg_bounds_t m_bounds;
	float m_scale;
	int m_object;
	sg_vector_primitive_t m_objs[OBJECT_MAX];


};

#endif /* SVGFONT_HPP_ */
