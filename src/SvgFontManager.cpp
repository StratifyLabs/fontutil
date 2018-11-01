/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#include <cmath>
#include <sapi/fmt.hpp>
#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/hal.hpp>
#include "SvgFontManager.hpp"

void sg_point_unmap(sg_point_t * d, const sg_vector_map_t * m){
	sg_api()->point_rotate(d, (SG_TRIG_POINTS - m->rotation) % SG_TRIG_POINTS);
	//map to the space
	s32 tmp;
	//x' = m->region.point.x + ((x + SG_MAX) * m->region.dim.width) / (SG_MAX-SG_MIN);
	//y' = m->region.point.y + ((y + SG_MAX) * m->region.dim.height) / (SG_MAX-SG_MIN);

	//(x' - m->region.point.x)*(SG_MAX-SG_MIN)/ m->region.dim.width - SG_MAX = x
	printf("%d - %d * %d / %d\n", d->x, m->region.point.x, (SG_MAX-SG_MIN), m->region.dim.width);
	tmp = ((d->x - m->region.point.x)*(SG_MAX-SG_MIN) + m->region.dim.width/2) / m->region.dim.width;
	d->x = tmp - SG_MAX;

	tmp = ((d->y - m->region.point.y)*(SG_MAX-SG_MIN) + m->region.dim.height/2) / m->region.dim.height;
	d->y = tmp - SG_MAX;

}

SvgFontManager::SvgFontManager() {
	// TODO Auto-generated constructor stub
	m_scale = 0.0f;
	m_object = 0;
}


int SvgFontManager::convert_file(const char * path){
	Son font;
	String value;
	value.set_size(2048);
	String access;
	DisplayDevice display;
	sg_point_t pour_points[MAX_FILL_POINTS];
	int j;
	int i;
	int num_fill_points;


	if( font.open_read(path) < 0 ){ return -1; }

	value = font.read_string("glyphs[0].attrs.bbox");
	if( value.is_empty() == false ){
		parse_bounds(value.c_str());
		printf("Bounds: %d,%d %dx%d\n", m_bounds.point.x, m_bounds.point.y, m_bounds.dim.width, m_bounds.dim.height);
	}


	display.initialize("/dev/display0");
	display.set_pen(Pen(1,1));
	VectorMap map(display);
	Bitmap fill(display.dim());

	//map.set_point(sg_point(display.width()/2,display.height()/2));
	//map.set_dim(display.width(), display.width());



	j = 7;
	for(j=150; j < 200;j++){
		//d is the path
		printf("Any errors: %d\n", font.get_error());
		access.sprintf("glyphs[%d].attrs.glyphName", j);
		value = font.read_string(access);
		if( value.is_empty() == false ){
			printf("Glyph Name: %s\n", value.c_str());
		} else {
			printf("SOn error %d\n", font.get_error());
			exit(1);
		}

		access.sprintf("glyphs[%d].attrs.d", j);
		value = font.read_string(access);
		if( value.is_empty() == false ){
			printf("Read value:%s\n", value.c_str());
			if( parse_svg_path(value.c_str()) < 0 ){
				return -1;
			}
		} else {
			printf("Failed to read %s\n", access.c_str());
			return -1;
		}

		printf("%d elements\n", m_object);


		sg_vector_path_t vector_path;

		vector_path.icon.count = m_object;
		vector_path.icon.list = m_path_description_list;
		vector_path.region = display.get_viewable_region();



		//Vector::show(icon);

		display.clear();

		map.set_rotation(0);

		sgfx::Vector::draw_path(display, vector_path, map);

		display.refresh();
		display.wait(1000);

		analyze_icon(display, vector_path, map, true);

		display.wait(1000);
		display.refresh();

		find_all_fill_points(display, fill, display.get_viewable_region(), 6);

#if 0
		display.set_pen_flags(Pen::FLAG_IS_BLEND);
		display.draw_bitmap(sg_point(0,0), fill);
#else

		num_fill_points = calculate_pour_points(display, fill, pour_points, MAX_FILL_POINTS);
		if( num_fill_points < MAX_FILL_POINTS ){
			for(i=0; i < num_fill_points; i++){
				sg_point_t pour_point;
				pour_point = pour_points[i];
				sg_point_unmap(&pour_point, map);
				if( m_object < PATH_DESCRIPTION_MAX ){
					m_path_description_list[m_object] = sgfx::Vector::get_path_pour(pour_point);
					m_object++;
				} else {
					printf("FAILED -- NOT ENOUGH ROOM IN DESCRIPTION LIST\n");
				}
			}
		} else {
			printf("FAILED -- TOO MANY FILL POINTS\n");
		}

		//if entire display is poured, there is a rogue fill point

		display.clear();

		printf("Draw final\n");
		vector_path.icon.count = m_object;

		Timer t;
		t.start();
		sgfx::Vector::draw_path(display, vector_path, map);
		t.stop();
		printf("Draw time is %ld\n", t.usec());
#endif

		display.wait(1000);
		display.refresh();

		printf("Done refresh\n");
	}

	return 0;

}

void SvgFontManager::analyze_icon(Bitmap & bitmap, sg_vector_path_t & vector_path, const VectorMap & map, bool recheck){
	Region region = sgfx::Vector::find_active_region(bitmap);
	Region target;
	sg_size_t width = bitmap.width() - bitmap.margin_left() - bitmap.margin_right();
	sg_point_t map_shift;
	sg_point_t bitmap_shift;

	target.set_point(Point(0, 0));
	target.set_dim(Dim(width, width));

	printf("Active region %d,%d %dx%d\n", region.x(), region.y(), region.width(), region.height());
	printf("Target region %d,%d %dx%d\n", target.x(), target.y(), target.width(), target.height());

	bitmap_shift.x = (bitmap.width()/2 - region.width()/2) - region.x();
	bitmap_shift.y = (bitmap.width()/2 - region.height()/2) - region.y();
	printf("Offset Error is %d,%d\n", bitmap_shift.x, bitmap_shift.y);

	map_shift.x = (bitmap_shift.x * SG_MAX*2 + map.width()/2) / map.width();
	map_shift.y = (bitmap_shift.y * SG_MAX*2 + map.height()/2) / map.height();

	printf("Map Shift is %d,%d\n", map_shift.x, map_shift.y);



	if( recheck ){
		shift_icon(vector_path.icon, map_shift);

		bitmap.clear();

		sgfx::Vector::draw_path(bitmap, vector_path, map);

		bitmap.refresh();

		analyze_icon(bitmap, vector_path, map, false);
	}



	//bitmap.draw_rectangle(region.point(), region.dim());
}

void SvgFontManager::shift_icon(sg_vector_path_icon_t & icon, Point shift){
	u32 i;
	for(i=0; i < icon.count; i++){
		sg_vector_path_description_t * description = (sg_vector_path_description_t *)icon.list + i;
		switch(description->type){
		case SG_VECTOR_PATH_MOVE:
			description->move.point = Point(description->move.point) + shift;
			break;
		case SG_VECTOR_PATH_LINE:
			description->line.point = Point(description->line.point) + shift;
			break;
		case SG_VECTOR_PATH_POUR:
			description->pour.point = Point(description->pour.point) + shift;
			break;
		case SG_VECTOR_PATH_QUADRATIC_BEZIER:
			description->quadratic_bezier.control = Point(description->quadratic_bezier.control) + shift;
			description->quadratic_bezier.point = Point(description->quadratic_bezier.point) + shift;
			break;
		case SG_VECTOR_PATH_CUBIC_BEZIER:
			description->cubic_bezier.control[0] = Point(description->cubic_bezier.control[0]) + shift;
			description->cubic_bezier.control[1] = Point(description->cubic_bezier.control[1]) + shift;
			description->cubic_bezier.point = Point(description->cubic_bezier.point) + shift;
			break;
		case SG_VECTOR_PATH_CLOSE:
			break;
		}
	}
}

void SvgFontManager::scale_icon(sg_vector_path_icon_t & icon, float scale){
	u32 i;
	for(i=0; i < icon.count; i++){
		sg_vector_path_description_t * description = (sg_vector_path_description_t *)icon.list + i;
		switch(description->type){
		case SG_VECTOR_PATH_MOVE:
			description->move.point = Point(description->move.point) * scale;
			break;
		case SG_VECTOR_PATH_LINE:
			description->line.point = Point(description->line.point) * scale;
			break;
		case SG_VECTOR_PATH_POUR:
			description->pour.point = Point(description->pour.point) * scale;
			break;
		case SG_VECTOR_PATH_QUADRATIC_BEZIER:
			description->quadratic_bezier.control = Point(description->quadratic_bezier.control) * scale;
			description->quadratic_bezier.point = Point(description->quadratic_bezier.point) * scale;
			break;
		case SG_VECTOR_PATH_CUBIC_BEZIER:
			description->cubic_bezier.control[0] = Point(description->cubic_bezier.control[0]) * scale;
			description->cubic_bezier.control[1] = Point(description->cubic_bezier.control[1]) * scale;
			description->cubic_bezier.point = Point(description->cubic_bezier.point) * scale;
			break;
		case SG_VECTOR_PATH_CLOSE:
			break;
		}
	}
}



int SvgFontManager::parse_svg_path(const char * d){
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
			printf("Invalid at -%s-\n", d+i);
			return -1;
		}

	} while( (i < len) && (m_object < PATH_DESCRIPTION_MAX) );

	if( m_object == PATH_DESCRIPTION_MAX ){
		printf("Max Objects\n");
		return -1;
	}

	return 0;

}

bool SvgFontManager::is_command_char(char c){
	int i;
	int len = strlen(path_commands());
	for(i=0; i < len; i++){
		if( c == path_commands()[i] ){
			return true;
		}
	}
	return false;
}

Point SvgFontManager::convert_svg_coord(float x, float y, bool is_absolute){
	float temp_x;
	float temp_y;
	sg_point_t point;


	temp_x = x;
	temp_y = y;

	//scale
	temp_x = temp_x * m_scale;
	temp_y = temp_y * m_scale *-1;

	if( is_absolute ){
		//shift
		temp_x = temp_x - SG_MAP_MAX/2;
		temp_y = temp_y + SG_MAP_MAX/2;
	}

	point.x = rintf(temp_x);
	point.y = rintf(temp_y);

	return point;
}

int SvgFontManager::seek_path_command(const char * path){
	int i = 0;
	int len = strlen(path);
	while( (is_command_char(path[i]) == false) && (i < len) ){
		i++;
	}
	return i;
}

int SvgFontManager::parse_bounds(const char * value){
	float values[4];
	float scale_factor;

	if( parse_number_arguments(value, values, 4) != 4 ){
		return -1;
	}
	m_bounds.point.x = roundf(values[0]);
	m_bounds.point.y = roundf(values[1]);
	m_bounds.dim.width = roundf(values[2]) - m_bounds.point.x + 1;
	m_bounds.dim.height = roundf(values[3]) - m_bounds.point.y + 1;

	if( m_bounds.dim.width > m_bounds.dim.height ){
		scale_factor = m_bounds.dim.width;
	} else {
		scale_factor = m_bounds.dim.height;
	}

	m_scale = (float)(SG_MAP_MAX*2) / scale_factor ;

	printf("Bounds: (%d, %d) (%dx%d)\n",
			m_bounds.point.x,
			m_bounds.point.y,
			m_bounds.dim.width,
			m_bounds.dim.height);
	return 0;
}

int SvgFontManager::parse_number_arguments(const char * path, float * dest, u32 n){
	u32 i;
	Token arguments;
	arguments.assign(path);
	arguments.parse(path_commands_space());

	for(i=0; i < n; i++){
		if( i < arguments.size() ){
			dest[i] = arguments.at(i).atoff();
		} else {
			return i;
		}
	}
	return n;
}


int SvgFontManager::parse_path_moveto_absolute(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	m_current_point = convert_svg_coord(values[0], values[1]);
	//m_start_point = m_current_point;

	m_path_description_list[m_object] = sgfx::Vector::get_path_move(m_current_point);
	m_object++;


	printf("Moveto Absolute (%0.1f,%0.1f) -> (%d,%d)\n",
			values[0], values[1],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}


int SvgFontManager::parse_path_moveto_relative(const char * path){
	float values[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}
	m_current_point += convert_svg_coord(values[0], values[1], false);

	m_path_description_list[m_object] = sgfx::Vector::get_path_move(m_current_point);
	m_object++;

	printf("Moveto Relative (%0.1f,%0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFontManager::parse_path_lineto_absolute(const char * path){
	float values[2];
	Point p;
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p = convert_svg_coord(values[0], values[1]);
	m_path_description_list[m_object] = sgfx::Vector::get_path_line(p);
	m_object++;
	m_current_point = p;

	printf("Lineto Absolute (%0.1f,%0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFontManager::parse_path_lineto_relative(const char * path){
	float values[2];
	Point p;
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p = m_current_point + convert_svg_coord(values[0], values[1], false);
	m_path_description_list[m_object] = sgfx::Vector::get_path_line(p);
	m_object++;
	m_current_point = p;

	printf("Lineto Relative (%0.1f,%0.1f) -> (%d,%d)\n",
			values[0], values[1],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}

int SvgFontManager::parse_path_horizontal_lineto_absolute(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(values[0], 0);
	p.y = m_current_point.y();
	m_path_description_list[m_object] = sgfx::Vector::get_path_line(p);

	m_object++;
	m_current_point = p;

	printf("Horizontal Lineto Absolute (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}


int SvgFontManager::parse_path_horizontal_lineto_relative(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(values[0], 0, false);
	p.x += m_current_point.x();
	p.y = m_current_point.y();
	m_path_description_list[m_object] = sgfx::Vector::get_path_line(p);
	m_object++;
	m_current_point = p;

	printf("Horizontal Lineto Relative (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}


int SvgFontManager::parse_path_vertical_lineto_absolute(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(0, values[0]);
	p.x = m_current_point.x();
	m_path_description_list[m_object] = sgfx::Vector::get_path_line(p);
	m_object++;
	m_current_point = p;

	printf("Vertical Lineto Absolute (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}

int SvgFontManager::parse_path_vertical_lineto_relative(const char * path){
	float values[1];
	sg_point_t p;
	if( parse_number_arguments(path, values, 1) != 1 ){
		return -1;
	}

	p = convert_svg_coord(0, values[0], false);
	p.x = m_current_point.x();
	p.y += m_current_point.y();
	m_path_description_list[m_object] = sgfx::Vector::get_path_line(p);
	m_object++;
	m_current_point = p;

	printf("Vertical Lineto Relative (%0.1f) -> (%d,%d)\n",
			values[0],
			m_current_point.x(), m_current_point.y());
	return seek_path_command(path);
}

int SvgFontManager::parse_path_quadratic_bezier_absolute(const char * path){
	float values[4];
	Point p[2];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}

	p[0] = convert_svg_coord(values[0], values[1]);
	p[1] = convert_svg_coord(values[2], values[3]);

	m_path_description_list[m_object] = sgfx::Vector::get_path_quadratic_bezier(p[0], p[1]);
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

int SvgFontManager::parse_path_quadratic_bezier_relative(const char * path){
	float values[4];
	Point p[2];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}

	p[0] = m_current_point + convert_svg_coord(values[0], values[1], false);
	p[1] = m_current_point + convert_svg_coord(values[2], values[3], false);

	m_path_description_list[m_object] = sgfx::Vector::get_path_quadratic_bezier(p[0], p[1]);
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

int SvgFontManager::parse_path_quadratic_bezier_short_absolute(const char * path){
	float values[2];
	Point p[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = convert_svg_coord(values[0], values[1]);

	m_path_description_list[m_object] = sgfx::Vector::get_path_quadratic_bezier(p[0], p[1]);
	m_object++;
	m_control_point = p[0];
	m_current_point = p[1];

	printf("Quadratic Bezier Short Absolute (%0.1f,%0.1f)\n", values[0], values[1]);
	return seek_path_command(path);
}

int SvgFontManager::parse_path_quadratic_bezier_short_relative(const char * path){
	float values[2];
	Point p[2];
	if( parse_number_arguments(path, values, 2) != 2 ){
		return -1;
	}

	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = m_current_point + convert_svg_coord(values[0], values[1], false);

	m_path_description_list[m_object] = sgfx::Vector::get_path_quadratic_bezier(p[0], p[1]);
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

int SvgFontManager::parse_path_cubic_bezier_absolute(const char * path){
	float values[6];
	Point p[3];
	if( parse_number_arguments(path, values, 6) != 6 ){
		return -1;
	}

	p[0] = convert_svg_coord(values[0], values[1]);
	p[1] = convert_svg_coord(values[2], values[3]);
	p[2] = convert_svg_coord(values[4], values[5]);

	m_path_description_list[m_object] = sgfx::Vector::get_path_cubic_bezier(p[0], p[1], p[2]);
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

int SvgFontManager::parse_path_cubic_bezier_relative(const char * path){
	float values[6];
	Point p[3];
	if( parse_number_arguments(path, values, 6) != 6 ){
		return -1;
	}

	p[0] = m_current_point + convert_svg_coord(values[0], values[1]);
	p[1] = m_current_point + convert_svg_coord(values[2], values[3]);
	p[2] = m_current_point + convert_svg_coord(values[4], values[5]);

	m_path_description_list[m_object] = sgfx::Vector::get_path_cubic_bezier(p[0], p[1], p[2]);
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

int SvgFontManager::parse_path_cubic_bezier_short_absolute(const char * path){
	float values[4];
	Point p[3];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}


	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = convert_svg_coord(values[0], values[1]);
	p[2] = convert_svg_coord(values[2], values[3]);

	m_path_description_list[m_object] = sgfx::Vector::get_path_cubic_bezier(p[0], p[1], p[2]);
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

int SvgFontManager::parse_path_cubic_bezier_short_relative(const char * path){
	float values[4];
	Point p[3];
	if( parse_number_arguments(path, values, 4) != 4 ){
		return -1;
	}

	p[0].set(2*m_current_point.x() - m_control_point.x(), 2*m_current_point.y() - m_control_point.y());
	p[1] = m_current_point + convert_svg_coord(values[0], values[1]);
	p[2] = m_current_point + convert_svg_coord(values[2], values[3]);

	m_path_description_list[m_object] = sgfx::Vector::get_path_cubic_bezier(p[0], p[1], p[2]);
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

int SvgFontManager::parse_close_path(const char * path){


	m_path_description_list[m_object] = sgfx::Vector::get_path_close();
	m_object++;

	printf("Close Path\n");

	//need to figure out where to put the fill

	return seek_path_command(path); //no arguments, cursor doesn't advance
}

int SvgFontManager::calculate_pour_points(Bitmap & bitmap, const Bitmap & fill_points, sg_point_t * points, sg_size_t max_points){

	Region region;
	int pour_points = 0;

	region = bitmap.get_viewable_region();

	printf("Region is %d,%d %dx%d\n", region.x(), region.y(), region.width(), region.height());

	sg_point_t point;
	for(point.x = 0; point.x < fill_points.width(); point.x++){
		for(point.y = 0; point.y < fill_points.height(); point.y++){
			if( (fill_points.get_pixel(point) != 0) && (bitmap.get_pixel(point) == 0) ){
				printf("Pour on %d,%d\n", point.x, point.y);
				bitmap.draw_pour(point, region);
				points[pour_points] = point;
				pour_points++;
				if( pour_points == max_points ){
					return max_points;
				}
				//return 1;
			}
		}
	}

	return pour_points;

}

void SvgFontManager::find_all_fill_points(const Bitmap & bitmap, Bitmap & fill_points, const Region & region, sg_size_t grid){
	fill_points.clear();
	bool is_fill;
	for(sg_int_t x = region.x(); x < region.width(); x+=grid){
		for(sg_int_t y = 0; y < region.height(); y+=grid){
			is_fill = is_fill_point(bitmap, sg_point(x,y), region);
			if( is_fill ){
				fill_points.draw_pixel(sg_point(x,y));
			}
		}
	}
}

bool SvgFontManager::is_fill_point(const Bitmap & bitmap, sg_point_t point, const Region & region){
	int boundary_count;
	sg_color_t color;
	sg_point_t temp = point;

	color = bitmap.get_pixel(point);
	if( color != 0 ){
		return false;
	}

	sg_size_t width;
	sg_size_t height;
	width = region.x() + region.width();
	height = region.y() + region.height();

	boundary_count = 0;

	do {
		while( (temp.x < width) && (bitmap.get_pixel(temp) == 0) ){
			temp.x++;
		}

		if( temp.x < width ){
			boundary_count++;
			while( (temp.x < width) && (bitmap.get_pixel(temp) != 0) ){
				temp.x++;
			}

			if( temp.x < width ){
			}
		}
	} while( temp.x < width );

	if ((boundary_count % 2) == 0){
		return false;
	}

	temp = point;
	boundary_count = 0;
	do {
		while( (temp.x >= region.x()) && (bitmap.get_pixel(temp) == 0) ){
			temp.x--;
		}

		if( temp.x >= region.x() ){
			boundary_count++;
			while( (temp.x >= region.x()) && (bitmap.get_pixel(temp) != 0) ){
				temp.x--;
			}

			if( temp.x >= region.x() ){
			}
		}
	} while( temp.x >= region.x() );

	if ((boundary_count % 2) == 0){
		return false;
	}

	temp = point;
	boundary_count = 0;
	do {
		while( (temp.y >= region.y()) && (bitmap.get_pixel(temp) == 0) ){
			temp.y--;
		}

		if( temp.y >= region.y() ){
			boundary_count++;
			while( (temp.y >= region.y()) && (bitmap.get_pixel(temp) != 0) ){
				temp.y--;
			}

			if( temp.y >= region.y() ){
			}
		}
	} while( temp.y >= region.y() );

	if ((boundary_count % 2) == 0){
		return false;
	}

	return true;

	temp = point;
	boundary_count = 0;
	do {
		while( (temp.y < height) && (bitmap.get_pixel(temp) == 0) ){
			temp.y++;
		}

		if( temp.y < height ){
			boundary_count++;
			while( (temp.y < height) && (bitmap.get_pixel(temp) != 0) ){
				temp.y++;
			}

			if( temp.y < height ){
			}
		}
	} while( temp.y < height );

	if ((boundary_count % 2) == 0){
		return false;
	}

	return true;

}


