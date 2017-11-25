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
	virtual ~SvgFont();

	int convert_file(const char * path);

private:
	void parse_svg_path(const char * d);

	static const char * path_commands_space(){ return "MmCcSsLlHhVvQqTtAaZz "; }
	static const char * path_commands(){ return "MmCcSsLlHhVvQqTtAaZz"; }
	static bool is_command_char(char c);

	enum {
		NO_STATE,
		MOVETO_STATE,
		LINETO_STATE,
		TOTAL_STATE
	};

	int m_state;
	sg_point_t m_current_point;


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

};

#endif /* SVGFONT_HPP_ */
