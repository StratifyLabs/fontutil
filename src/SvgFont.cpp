/*
 * Svg.cpp
 *
 *  Created on: Nov 25, 2017
 *      Author: tgil
 */

#include <sapi/fmt.hpp>
#include <sapi/var.hpp>
#include "SvgFont.hpp"

SvgFont::SvgFont() {
	// TODO Auto-generated constructor stub

}

SvgFont::~SvgFont() {
	// TODO Auto-generated destructor stub
}

int SvgFont::convert_file(const char * path){
	Son<4> font;
	String value(1024);

	if( font.open_read(path) < 0 ){ return -1; }


	//d is the path
	if( font.read_str("glyphs[1].attrs.d", value) >= 0 ){
		parse_svg_path(value.c_str());
	}

	return 0;

}


void SvgFont::parse_svg_path(const char * d){
	int len = strlen(d);
	int i;
	int ret;
	char command_char;
	String str(128);

	//M, m is moveto
	//L, l, H, h, V, v is lineto uppercase is absolute, lower case is relative H is horiz, V is vertical
	//C, c, S, s is cubic bezier
	//Q, q, T, t is quadratic bezier
	//A, a elliptical arc
	//Z, z is closepath

	printf("Parse (%d) %s\n", len, d);
	i = 0;
	ret = 0;
	do {

		//what is the next command
		if( is_command_char(d[i]) ){
			ret = -1;
			command_char = d[i];
			i++;
			switch(command_char){
			case 'M':
				ret = parse_path_moveto_absolute(d+i);
				break;
			case 'm':
				ret = parse_path_moveto_relative(d+i);
				break;
			case 'L':
				ret = parse_path_lineto_absolute(d+i);
				break;
			case 'l':
				ret = parse_path_lineto_relative(d+i);
				break;
			case 'H':
				ret = parse_path_horizontal_lineto_absolute(d+i);
				break;
			case 'h':
				ret = parse_path_horizontal_lineto_relative(d+i);
				break;
			case 'V':
				ret = parse_path_vertical_lineto_absolute(d+i);
				break;
			case 'v':
				ret = parse_path_vertical_lineto_relative(d+i);
				break;
			case 'C':
				ret = parse_path_cubic_bezier_absolute(d+i);
				break;
			case 'c':
				ret = parse_path_cubic_bezier_relative(d+i);
				break;
			case 'S':
				ret = parse_path_cubic_bezier_short_absolute(d+i);
				break;
			case 's':
				ret = parse_path_cubic_bezier_short_relative(d+i);
				break;
			case 'Q':
				ret = parse_path_quadratic_bezier_absolute(d+i);
				break;
			case 'q':
				ret = parse_path_quadratic_bezier_relative(d+i);
				break;
			case 'T':
				ret = parse_path_quadratic_bezier_short_absolute(d+i);
				break;
			case 't':
				ret = parse_path_quadratic_bezier_short_relative(d+i);
				break;
			case 'A': break;
			case 'a': break;
			case 'Z':
			case 'z':
				ret = parse_close_path(d+i);
				break;
			default:
				printf("Unhandled command char %c\n", command_char);
				break;
			}
			if( ret >= 0 ){
				i += ret;
			} else {
				printf("Parsing failed at %s\n", d + i);
				return;
			}
		} else {
			printf("Invalid\n");
		}

	} while(i < len);
}

bool SvgFont::is_command_char(char c){
	int i;
	int len = strlen(path_commands());
	for(i=0; i < len; i++){
		if( c == path_commands()[i] ){
			return true;
		}
	}
	return false;
}

int SvgFont::seek_path_command(const char * path){
	int i = 0;
	int len = strlen(path);
	while( (is_command_char(path[i]) == false) && (i < len) ){
		i++;
	}
	return i;
}

int SvgFont::parse_number_arguments(const char * path, float * dest, u32 n){
	u32 i;
	Token arguments;
	arguments.assign(path);
	arguments.parse(path_commands_space());

	for(i=0; i < n; i++){
		if( i < arguments.size() ){
			dest[i] = atoff(arguments.at(i));
		} else {
			return i;
		}
	}
	return n;
}


int SvgFont::parse_path_moveto_absolute(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	printf("Moveto Absolute (%0.1f, %0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}


int SvgFont::parse_path_moveto_relative(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	printf("Moveto Relative (%0.1f, %0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_lineto_absolute(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	printf("Lineto Absolute (%0.1f, %0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_lineto_relative(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	printf("Lineto Relative (%0.1f, %0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_horizontal_lineto_absolute(const char * path){
	float values[1];
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}
	printf("Horizontal Lineto Absolute (%0.1f)\n", values[0]);
	return seek_path_command(path);
}


int SvgFont::parse_path_horizontal_lineto_relative(const char * path){
	float values[1];
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}
	printf("Horizontal Lineto Relative (%0.1f)\n", values[0]);
	return seek_path_command(path);
}


int SvgFont::parse_path_vertical_lineto_absolute(const char * path){
	float values[1];
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}
	printf("Vertical Lineto Absolute (%0.1f)\n", values[0]);
	return seek_path_command(path);
}

int SvgFont::parse_path_vertical_lineto_relative(const char * path){
	float values[1];
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}
	printf("Vertical Lineto Relative (%0.1f)\n", values[0]);
	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_absolute(const char * path){
	float values[4];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}
	printf("Quadratic Bezier Absolute (%0.1f, %0.1f), (%0.1f, %0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_relative(const char * path){
	float values[4];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}
	printf("Quadratic Bezier Relative (%0.1f, %0.1f), (%0.1f, %0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_short_absolute(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	printf("Quadratic Bezier Short Absolute (%0.1f, %0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_short_relative(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	printf("Quadratic Bezier Short Relative (%0.1f, %0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_cubic_bezier_absolute(const char * path){
	float values[6];
	if( parse_number_arguments(path, values, 6) != 6 ){
		return -1;
	}
	printf("Cubic Bezier Absolute (%0.1f, %0.1f), (%0.1f, %0.1f), (%0.1f, %0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3],
			values[4],
			values[5]);
	return seek_path_command(path);
}

int SvgFont::parse_path_cubic_bezier_relative(const char * path){
	float values[6];
	if( parse_number_arguments(path, values, 6) != 6 ){
		return -1;
	}
	printf("Cubic Bezier Relative (%0.1f, %0.1f), (%0.1f, %0.1f), (%0.1f, %0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3],
			values[4],
			values[5]);
	return seek_path_command(path);
}

int SvgFont::parse_path_cubic_bezier_short_absolute(const char * path){
	float values[4];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}
	printf("Cubic Bezier Short Absolute (%0.1f, %0.1f), (%0.1f, %0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}

int SvgFont::parse_path_cubic_bezier_short_relative(const char * path){
	float values[4];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}
	printf("Cubic Bezier Short Relative (%0.1f, %0.1f), (%0.1f, %0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}


int SvgFont::parse_close_path(const char * path){
	printf("Close Path\n");
	return 0; //no arguments, cursor doesn't advance
}


