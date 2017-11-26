/*
 * Svg.cpp
 *
 *  Created on: Nov 25, 2017
 *      Author: tgil
 */

#include <cmath>
#include <sapi/fmt.hpp>
#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/hal.hpp>
#include "SvgFont.hpp"

SvgFont::SvgFont() {
	// TODO Auto-generated constructor stub
	m_scale = 0.0f;
	m_object = 0;
}


int SvgFont::convert_file(const char * path){
	Son<4> font;
	String value(2048);
	String access;

	if( font.open_read(path) < 0 ){ return -1; }

	if( font.read_str("glyphs[0].attrs.bbox", value) >= 0 ){
		parse_bounds(value.c_str());
	}

	int j;

	DisplayDev display;

	display.init("/dev/display0");
	display.set_pen(Pen(1,1));

	for(j=100; j < 200; j++){

		//d is the path
		access.sprintf("glyphs[%d].attrs.d", j);
		if( font.read_str(access, value) >= 0 ){
			if( parse_svg_path(value.c_str()) < 0 ){
				return -1;
			}
		} else {
			printf("Failed to read %s\n", access.c_str());
			return -1;
		}

		printf("%d elements\n", m_object);

		sg_vector_icon_t icon;

		icon.fill_total = 0;
		icon.primitives = m_objs;
		icon.total = m_object;

		Vector::show(icon);



		VectorMap map(display);

		int i;
		for(i=0; i < SG_TRIG_POINTS; i+=10){
			display.clear();

			map.set_rotation(i);

			Vector::draw(display, icon, map);

			display.refresh();
			Timer::wait_msec(30);

		}

		display.clear();

		map.set_rotation(0);

		Vector::draw(display, icon, map);

		display.refresh();

	}


	return 0;

}


int SvgFont::parse_svg_path(const char * d){
	int len = strlen(d);
	int i;
	int ret;
	char command_char;

	//M, m is moveto
	//L, l, H, h, V, v is lineto uppercase is absolute, lower case is relative H is horiz, V is vertical
	//C, c, S, s is cubic bezier
	//Q, q, T, t is quadratic bezier
	//A, a elliptical arc
	//Z, z is closepath

	m_object = 0;

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
				return -1;
			}
			if( ret >= 0 ){
				i += ret;
			} else {
				printf("Parsing failed at %s\n", d + i - 1);
				return -1;
			}
		} else {
			printf("Invalid at %s\n", d+i);
			return -1;
		}

	} while( (i < len) && (m_object < OBJECT_MAX) );

	if( m_object == OBJECT_MAX ){
		printf("Max Objects\n");
		return -1;
	}

	return 0;

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

Point SvgFont::convert_svg_coord(float x, float y, bool is_absolute){
	float temp_x;
	float temp_y;
	sg_point_t point;

	if( is_absolute ){
		//shift
		temp_x = x - m_bounds.top_left.x;
		temp_y = y - m_bounds.top_left.y;
	} else {
		temp_x = x;
		temp_y = y;
	}

	//scale
	temp_x = temp_x * m_scale;
	temp_y = temp_y * m_scale *-1;

	if( is_absolute ){
		//shift
		temp_x = temp_x - SG_MAP_MAX/2 + 2*m_bounds.top_left.x*m_scale;
		temp_y = temp_y + SG_MAP_MAX/2 - m_bounds.top_left.y*m_scale;
	}

	point.x = roundf(temp_x);
	point.y = roundf(temp_y);

	return point;
}

int SvgFont::seek_path_command(const char * path){
	int i = 0;
	int len = strlen(path);
	while( (is_command_char(path[i]) == false) && (i < len) ){
		i++;
	}
	return i;
}

int SvgFont::parse_bounds(const char * value){
	float values[4];
	sg_dim_t scale;
	float scale_factor;

	if( parse_number_arguments(value, values, 4) != 4 ){
		return -1;
	}
	m_bounds.top_left.x = roundf(values[0]);
	m_bounds.top_left.y = roundf(values[1]);
	m_bounds.bottom_right.x = roundf(values[2]);
	m_bounds.bottom_right.y = roundf(values[3]);

	scale.width = m_bounds.bottom_right.x - m_bounds.top_left.x;
	scale.height = m_bounds.bottom_right.y - m_bounds.top_left.y;

	if( scale.width > scale.height ){
		scale_factor = scale.width;
	} else {
		scale_factor = scale.height;
	}

	m_scale = (SG_MAP_MAX) / scale_factor;


	printf("Bounds: (%d, %d) (%d, %d)\n",
			m_bounds.top_left.x,
			m_bounds.top_left.y,
			m_bounds.bottom_right.x,
			m_bounds.bottom_right.y);
	return 0;
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
	m_current_point = convert_svg_coord(values[0], values[1]);
	m_start_point = m_current_point;

	printf("Moveto Absolute (%0.1f,%0.1f) -> (%d,%d)\n",
			values[0], values[1],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}


int SvgFont::parse_path_moveto_relative(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	m_current_point += convert_svg_coord(values[0], values[1], false);
	printf("Moveto Relative (%0.1f,%0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_lineto_absolute(const char * path){
	float values[2];
	Point p;
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p = convert_svg_coord(values[0], values[1]);
	m_objs[m_object] = Vector::line(m_current_point, p);
	m_object++;
	m_current_point = p;

	printf("Lineto Absolute (%0.1f,%0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_lineto_relative(const char * path){
	float values[2];
	Point p;
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p = m_current_point + convert_svg_coord(values[0], values[1], false);
	m_objs[m_object] = Vector::line(m_current_point, p);
	m_object++;
	m_current_point = p;

	printf("Lineto Relative (%0.1f,%0.1f) -> (%d,%d)\n",
			values[0], values[1],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}

int SvgFont::parse_path_horizontal_lineto_absolute(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(values[0], 0);
	p.y = m_current_point.y();
	m_objs[m_object] = Vector::line(m_current_point, p);

	m_object++;
	m_current_point = p;

	printf("Horizontal Lineto Absolute (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}


int SvgFont::parse_path_horizontal_lineto_relative(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(values[0], 0, false);
	p.x += m_current_point.x();
	p.y = m_current_point.y();
	m_objs[m_object] = Vector::line(m_current_point, p);
	m_object++;
	m_current_point = p;

	printf("Horizontal Lineto Relative (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}


int SvgFont::parse_path_vertical_lineto_absolute(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(0, values[0]);
	p.x = m_current_point.x();
	m_objs[m_object] = Vector::line(m_current_point, p);
	m_object++;
	m_current_point = p;

	printf("Vertical Lineto Absolute (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}

int SvgFont::parse_path_vertical_lineto_relative(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(0, values[0], false);
	p.x = m_current_point.x();
	p.y += m_current_point.y();
	m_objs[m_object] = Vector::line(m_current_point, p);
	m_object++;
	m_current_point = p;

	printf("Vertical Lineto Relative (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_absolute(const char * path){
	float values[4];
	Point p[2];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}

	p[0] = convert_svg_coord(values[0], values[1]);
	p[1] = convert_svg_coord(values[2], values[3]);

	m_objs[m_object] = Vector::quadratic_bezier(m_current_point, p[0], p[1]);
	m_object++;
	m_control_point = p[0];
	m_current_point = p[1];

	printf("Quadratic Bezier Absolute (%0.1f,%0.1f), (%0.1f,%0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_relative(const char * path){
	float values[4];
	Point p[2];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}

	p[0] = m_current_point + convert_svg_coord(values[0], values[1], false);
	p[1] = m_current_point + convert_svg_coord(values[2], values[3], false);

	m_objs[m_object] = Vector::quadratic_bezier(m_current_point, p[0], p[1]);
	m_object++;

	printf("Quadratic Bezier Relative (%0.1f,%0.1f), (%0.1f,%0.1f) -> (%d,%d), (%d,%d), (%d, %d)\n",
			values[0], values[1],
			values[2], values[3],
			m_current_point.x(), m_current_point.y(),
			p[0].x(), p[0].y(),
			p[1].x(), p[1].y()
	);

	m_control_point = p[0];
	m_current_point = p[1];

	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_short_absolute(const char * path){
	float values[2];
	Point p[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = convert_svg_coord(values[0], values[1]);

	m_objs[m_object] = Vector::quadratic_bezier(m_current_point, p[0], p[1]);
	m_object++;
	m_control_point = p[0];
	m_current_point = p[1];

	printf("Quadratic Bezier Short Absolute (%0.1f,%0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFont::parse_path_quadratic_bezier_short_relative(const char * path){
	float values[2];
	Point p[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = m_current_point + convert_svg_coord(values[0], values[1], false);

	m_objs[m_object] = Vector::quadratic_bezier(m_current_point, p[0], p[1]);
	m_object++;

	printf("Quadratic Bezier Short Relative (%0.1f,%0.1f) -> (%d,%d), (%d,%d), (%d, %d)\n",
			values[0], values[1],
			m_current_point.x(), m_current_point.y(),
			p[0].x(), p[0].y(),
			p[1].x(), p[1].y()
	);

	m_control_point = p[0];
	m_current_point = p[1];

	return seek_path_command(path);
}

int SvgFont::parse_path_cubic_bezier_absolute(const char * path){
	float values[6];
	Point p[3];
	if( parse_number_arguments(path, values, 6) != 6 ){
		return -1;
	}

	p[0] = convert_svg_coord(values[0], values[1]);
	p[1] = convert_svg_coord(values[2], values[3]);
	p[2] = convert_svg_coord(values[4], values[5]);

	m_objs[m_object] = Vector::cubic_bezier(m_current_point, p[0], p[1], p[2]);
	m_object++;
	m_control_point = p[1];
	m_current_point = p[2];

	printf("Cubic Bezier Absolute (%0.1f,%0.1f), (%0.1f,%0.1f), (%0.1f,%0.1f)\n",
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
	Point p[3];
	if( parse_number_arguments(path, values, 6) != 6 ){
		return -1;
	}

	p[0] = m_current_point + convert_svg_coord(values[0], values[1]);
	p[1] = m_current_point + convert_svg_coord(values[2], values[3]);
	p[2] = m_current_point + convert_svg_coord(values[4], values[5]);

	m_objs[m_object] = Vector::cubic_bezier(m_current_point, p[0], p[1], p[2]);
	m_object++;
	m_control_point = p[1];
	m_current_point = p[2];

	printf("Cubic Bezier Relative (%0.1f,%0.1f), (%0.1f,%0.1f), (%0.1f,%0.1f)\n",
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
	Point p[3];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}


	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = convert_svg_coord(values[0], values[1]);
	p[2] = convert_svg_coord(values[2], values[3]);

	m_objs[m_object] = Vector::cubic_bezier(m_current_point, p[0], p[1], p[2]);
	m_object++;
	m_control_point = p[1];
	m_current_point = p[2];

	printf("Cubic Bezier Short Absolute (%0.1f,%0.1f), (%0.1f,%0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}

int SvgFont::parse_path_cubic_bezier_short_relative(const char * path){
	float values[4];
	Point p[3];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}

	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = m_current_point + convert_svg_coord(values[0], values[1]);
	p[2] = m_current_point + convert_svg_coord(values[2], values[3]);

	m_objs[m_object] = Vector::cubic_bezier(m_current_point, p[0], p[1], p[2]);
	m_object++;
	m_control_point = p[1];
	m_current_point = p[2];

	printf("Cubic Bezier Short Relative (%0.1f,%0.1f), (%0.1f,%0.1f)\n",
			values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}


int SvgFont::parse_close_path(const char * path){


	m_objs[m_object] = Vector::line(m_current_point, m_start_point);
	m_object++;

	printf("Close Path\n");

	//need to figure out where to put the fill

	return seek_path_command(path); //no arguments, cursor doesn't advance
}


