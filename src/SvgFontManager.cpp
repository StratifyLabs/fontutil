/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#include <cmath>
#include <sapi/fmt.hpp>
#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/chrono.hpp>
#include <sapi/hal.hpp>
#include <sapi/sgfx.hpp>
#include "SvgFontManager.hpp"

void sg_point_unmap(Point & d, const sg_vector_map_t & m){
	d.rotate((SG_TRIG_POINTS - m.rotation) % SG_TRIG_POINTS);
	//api::SgfxObject::api()->point_rotate(d, (SG_TRIG_POINTS - m->rotation) % SG_TRIG_POINTS);
	//map to the space
	s32 tmp_x;
	s32 tmp_y;
	//x' = m->region.point.x + ((x + SG_MAX) * m->region.dim.width) / (SG_MAX-SG_MIN);
	//y' = m->region.point.y + ((y + SG_MAX) * m->region.dim.height) / (SG_MAX-SG_MIN);

	//(x' - m->region.point.x)*(SG_MAX-SG_MIN)/ m->region.dim.width - SG_MAX = x
	//printer().message("%d - %d * %d / %d", d->x, m->region.point.x, (SG_MAX-SG_MIN), m->region.dim.width);
	tmp_x = ((d.x() - m.region.point.x)*(SG_MAX-SG_MIN) + m.region.dim.width/2) / m.region.dim.width;
	tmp_y = ((d.y() - m.region.point.y)*(SG_MAX-SG_MIN) + m.region.dim.height/2) / m.region.dim.height;

	d.set(tmp_x - SG_MAX, tmp_y - SG_MAX);

}

SvgFontManager::SvgFontManager() {
	// TODO Auto-generated constructor stub
	m_scale = 0.0f;
	m_object = 0;
	m_character_set = Font::charset();
	m_is_show_canvas = true;
}


int SvgFontManager::convert_file(const ConstString & path){
	int j;
	int i;

	JsonDocument json_document;
	JsonArray font_array = json_document.load_from_file(path).to_array();
	JsonObject top = font_array.at(0).to_object();

	printer().open_object("svg.convert");

	printer().message("File type is %s", top.at("name").to_string().str());

	if( top.at("name").to_string() != "svg" ){
		printer().error("wrong file type (not svg)");
		return -1;
	}

	JsonObject metadata = top.at("childs").to_array().at(0).to_object();
	JsonObject defs = top.at("childs").to_array().at(1).to_object();
	JsonObject font = defs.at("childs").to_array().at(0).to_object();
	JsonArray glyphs = font.at("childs").to_array();
	JsonObject font_face = glyphs.at(0).to_object();
	JsonObject attributes = font_face.at("attrs").to_object();
	JsonObject missing_glyph = glyphs.at(1).to_object();


	printer().message("metadata: %s", metadata.at("name").to_string().str());
	printer().message("defs: %s", defs.at("name").to_string().str());
	printer().message("font: %s", font.at("name").to_string().str());
	printer().message("font-face: %s", font_face.at("name").to_string().str());
	printer().message("fontFamily: %s", attributes.at("fontFamily").to_string().str());
	printer().message("fontStretch: %s", attributes.at("fontStretch").to_string().str());
	u16 units_per_em = attributes.at("unitsPerEm").to_string().to_integer();

	m_scale = (SG_MAP_MAX*1700UL) / (units_per_em*1000);

	String value;
	value = attributes.at("bbox").to_string();
	printer().message("bbox: %s", value.str());

	if( value.is_empty() == false ){
		parse_bounds(value.cstring());
		printer().open_object("SVG bounds");
		printer() << m_bounds;
		printer().close_object();
	} else {
		printer().warning("Failed to find bounding box");
	}

	printer().message("missingGlyph: %s", missing_glyph.at("name").to_string().str());
	printer().message("Glyph count %ld", glyphs.count());

	for(j=2; j < glyphs.count();j++){
		//d is the path
		JsonObject entry = glyphs.at(j).to_object();
		String name = entry.at("name").to_string();
		if( name == "glyph" ){
			JsonObject glyph = glyphs.at(j).to_object().at("attrs").to_object();
			process_glyph(glyph);
		} else if( name == "hkern" ){
			JsonObject kerning = glyphs.at(j).to_object().at("attrs").to_object();
			process_hkern(kerning);
		}
	}

	m_bmp_font_manager.generate_font_file("./font.sbf");


	return 0;
}

int SvgFontManager::process_hkern(const JsonObject & kerning){

	if( kerning.is_object() ){
		char first;
		char second;

		String first_string;
		String second_string;

		first_string = kerning.at("u1").to_string();
		if( first_string == "<invalid>" || first_string.length() != 1 ){
			return 0;
		}

		if( m_character_set.find(first_string) == String::npos ){
			return 0;
		}

		second_string = kerning.at("u2").to_string();
		if( second_string == "<invalid>" || second_string.length() != 1 ){
			return 0;
		}

		if( m_character_set.find(second_string) == String::npos ){
			return 0;
		}

		first = first_string.at(0);
		second = second_string.at(0);

		sg_font_kerning_pair_t kerning_pair;
		kerning_pair.unicode_first = first;
		kerning_pair.unicode_second = second;

		s16 specified_kerning = kerning.at("k").to_integer();
		int kerning_sign;
		if( specified_kerning < 0 ){
			kerning_sign = -1;
			specified_kerning *= kerning_sign;
		} else {
			kerning_sign = 1;
		}
		s16 mapped_kerning = specified_kerning * m_scale; //kerning value in mapped vector space -- needs to be converted to bitmap distance

		//create a map to move the kerning value to bitmap distance
		VectorMap map;
		Region region(Point(), m_canvas_dimensions);
		map.calculate_for_region(region);

		Point kerning_point(mapped_kerning, 0);

		kerning_point.map(map);

		kerning_point -= region.center();

		kerning_pair.horizontal_kerning = kerning_point.x() * kerning_sign; //needs to be mapped to canvas size

		printer().message("kerning %c %c %d -> %d", kerning_pair.unicode_first, kerning_pair.unicode_second, specified_kerning, kerning_pair.horizontal_kerning);
		m_bmp_font_manager.kerning_pair_list().push_back(kerning_pair);

	} else {
		return -1;
	}


	return 1;
}

int SvgFontManager::process_glyph(const JsonObject & glyph){

	String glyph_name = glyph.at("glyphName").to_string();
	String unicode = glyph.at("unicode").to_string();


	bool is_in_character_set = false;
	if( unicode.length() == 1 ){
		char c = unicode.at(0);
		if( m_character_set.find(unicode) != String::npos ){
			is_in_character_set = true;
		}
	}

	if( is_in_character_set ){

		if( glyph_name != "<invalid>" ){
			printer().message("Glyph Name: %s", glyph_name.cstring());
			printer().message("Unicode is %s", unicode.cstring());
		} else {
			printer().error("Glyph name not found");
			return -1;
		}

		String x_advance = glyph.at("horizAdvX").to_string();

		// \todo Add to character info advance_x

		String drawing_path;
		drawing_path = glyph.at("d").to_string();
		if( drawing_path != "<invalid>" ){
			printer().message("Read value:%s", drawing_path.cstring());
			if( parse_svg_path(drawing_path.cstring()) < 0 ){
				printer().error("failed to parse svg path");
				printer().close_object();
				return -1;
			}

			Bitmap canvas(m_canvas_dimensions.width()+4,m_canvas_dimensions.height()+4);
			canvas.set_margin_left(2);
			canvas.set_margin_right(2);
			canvas.set_margin_top(2);
			canvas.set_margin_bottom(2);

			canvas.set_pen(Pen(1,1));
			printer().message("Canvas %dx%d", canvas.width(), canvas.height());
			VectorMap map(canvas);
			Bitmap fill(canvas.dim());

			printer().message("%d elements", m_object);

			sg_vector_path_t vector_path;
			vector_path.icon.count = m_object;
			vector_path.icon.list = m_path_description_list;
			vector_path.region = canvas.get_viewable_region();

			//Vector::show(icon);

			canvas.clear();

			map.set_rotation(0);
			sgfx::Vector::draw_path(canvas, vector_path, map);

			//analyze_icon(canvas, vector_path, map, true);

			find_all_fill_points(canvas, fill, canvas.get_viewable_region(), m_pour_grid_size);

			Vector<sg_point_t> fill_points = calculate_pour_points(canvas, fill);
			for(u32 i=0; i < fill_points.count(); i++){
				Point pour_point;
				pour_point = fill_points.at(i);
				sg_point_unmap(pour_point, map);
				if( m_object < PATH_DESCRIPTION_MAX ){
					m_path_description_list[m_object] = sgfx::Vector::get_path_pour(pour_point);
					m_object++;
				} else {
					printer().message("FAILED -- NOT ENOUGH ROOM IN DESCRIPTION LIST");
				}
			}

			//if entire display is poured, there is a rogue fill point

			//save the character to the output file

			canvas.clear();

			printer().message("Draw final");
			vector_path.icon.count = m_object;
			sgfx::Vector::draw_path(canvas, vector_path, map);
			printer().message("Canvas %dx%d", canvas.width(), canvas.height());

#if SHOW_ORIGIN
			canvas.draw_line(Point(m_canvas_origin.x(), 0), Point(m_canvas_origin.x(), canvas.y_max()));
			canvas.draw_line(Point(0, m_canvas_origin.y()), Point(canvas.x_max(), m_canvas_origin.y()));
#endif

			if( m_is_show_canvas ){
				printer().open_object(String().format("character-%s", unicode.cstring()));
				printer() << canvas;
				printer().close_object();
			}

			Region active_region = canvas.calculate_active_region();
			Bitmap active_canvas(active_region.dim());
			active_canvas.draw_sub_bitmap(sg_point(0,0), canvas, active_region);

			//find region inhabited by character

			sg_font_char_t character;

			character.id = unicode.at(0); //unicode value
			character.advance_x = x_advance.to_integer(); //value from SVG file


			//derive width, height, offset_x, offset_y from image
			//for offset_x and offset_y what is the standard?

			character.width = active_canvas.width(); //width of bitmap
			character.height = active_canvas.height(); //height of the bitmap
			character.offset_x = active_region.point().x() - m_canvas_origin.x(); //x offset when drawing the character
			character.offset_y = active_region.point().y(); //y offset when drawing the character

			//add character to master canvas, canvas_x and canvas_y are location on the master canvas
			character.canvas_x = 0; //x location on master canvas -- set when font is generated
			character.canvas_y = 0; //y location on master canvas -- set when font is generated

			m_bmp_font_manager.character_list().push_back(character);
			m_bmp_font_manager.bitmap_list().push_back(active_canvas);

#if !SHOW_ORIGIN
			if( m_is_show_canvas ){
				printer().open_object(String().format("active character-%s", unicode.cstring()));
				printer() << active_region;
				printer() << active_canvas;
				printer().close_object();
			}
#endif

		} else {
			printer().error("%s has no d value", glyph_name.str());
			return -1;
		}
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


	printer().open_object("active region");
	printer() << region;
	printer().close_object();

	printer().open_object("target region");
	printer() << region;
	printer().close_object();

	bitmap_shift.x = (bitmap.width()/2 - region.dim().width()/2) - region.point().x();
	bitmap_shift.y = (bitmap.width()/2 - region.dim().height()/2) - region.point().y();
	printer().message("Offset Error is %d,%d", bitmap_shift.x, bitmap_shift.y);

	Region map_region = map.region();
	map_shift.x = (bitmap_shift.x * SG_MAX*2 + map_region.dim().width()/2) / map_region.dim().width();
	map_shift.y = (bitmap_shift.y * SG_MAX*2 + map_region.dim().height()/2) / map_region.dim().height();

	printer().message("Map Shift is %d,%d", map_shift.x, map_shift.y);

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

	printer().message("Parse (%d) %s", len, d);
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
					printer().message("Unhandled command char %c", command_char);
					return -1;
			}
			if( ret >= 0 ){
				i += ret;
			} else {
				printer().message("Parsing failed at %s", d + i - 1);
				return -1;
			}
		} else {
			printer().message("Invalid at -%s-", d+i);
			return -1;
		}

	} while( (i < len) && (m_object < PATH_DESCRIPTION_MAX) );

	if( m_object == PATH_DESCRIPTION_MAX ){
		printer().message("Max Objects");
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
	temp_y = temp_y * m_scale *-1.0f;

	if( is_absolute ){
		//shift
		temp_x = temp_x - SG_MAP_MAX/2.0f;
		temp_y = temp_y + SG_MAP_MAX/2.0f;
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

	Dim canvas_dimensions;
	if( parse_number_arguments(value, values, 4) != 4 ){
		return -1;
	}
	m_bounds << Point(roundf(values[0]), roundf(values[1]));
	m_bounds << Dim(roundf(values[2]) - m_bounds.point().x() + 1, roundf(values[3]) - m_bounds.point().y() + 1);

	//how do bounds aspect ratio map to canvas size
	canvas_dimensions = m_bounds.dim();

	sg_size_t max_dimension = canvas_dimensions.width() > canvas_dimensions.height() ? canvas_dimensions.width() : canvas_dimensions.height();

	float scale = 1.0f * m_canvas_size / max_dimension;

	m_canvas_dimensions = canvas_dimensions * scale;

	m_canvas_origin = Point(m_bounds.point().x()*-1 * m_canvas_dimensions.width() / (m_bounds.dim().width()),
									(m_bounds.dim().height() + m_bounds.point().y()) * m_canvas_dimensions.height() / (m_bounds.dim().height()) );


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


	printer().message("Moveto Absolute (%0.1f,%0.1f) -> (%d,%d)",
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

	printer().message("Moveto Relative (%0.1f,%0.1f)", values[0], values[1]);
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

	printer().message("Lineto Absolute (%0.1f,%0.1f)", values[0], values[1]);
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

	printer().message("Lineto Relative (%0.1f,%0.1f) -> (%d,%d)",
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

	printer().message("Horizontal Lineto Absolute (%0.1f) -> (%d,%d)",
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

	printer().message("Horizontal Lineto Relative (%0.1f) -> (%d,%d)",
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

	printer().message("Vertical Lineto Absolute (%0.1f) -> (%d,%d)",
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

	printer().message("Vertical Lineto Relative (%0.1f) -> (%d,%d)",
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

	printer().message("Quadratic Bezier Absolute (%0.1f,%0.1f), (%0.1f,%0.1f)",
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

	printer().message("Quadratic Bezier Relative (%0.1f,%0.1f), (%0.1f,%0.1f) -> (%d,%d), (%d,%d), (%d, %d)",
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

	printer().message("Quadratic Bezier Short Absolute (%0.1f,%0.1f)", values[0], values[1]);
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

	printer().message("Quadratic Bezier Short Relative (%0.1f,%0.1f) -> (%d,%d), (%d,%d), (%d, %d)",
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

	printer().message("Cubic Bezier Absolute (%0.1f,%0.1f), (%0.1f,%0.1f), (%0.1f,%0.1f)",
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

	printer().message("Cubic Bezier Relative (%0.1f,%0.1f), (%0.1f,%0.1f), (%0.1f,%0.1f)",
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

	printer().message("Cubic Bezier Short Absolute (%0.1f,%0.1f), (%0.1f,%0.1f)",
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

	printer().message("Cubic Bezier Short Relative (%0.1f,%0.1f), (%0.1f,%0.1f)",
							values[0],
			values[1],
			values[2],
			values[3]);
	return seek_path_command(path);
}

int SvgFontManager::parse_close_path(const char * path){


	m_path_description_list[m_object] = sgfx::Vector::get_path_close();
	m_object++;

	printer().message("Close Path");

	//need to figure out where to put the fill

	return seek_path_command(path); //no arguments, cursor doesn't advance
}

Vector<sg_point_t> SvgFontManager::calculate_pour_points(Bitmap & bitmap, const Bitmap & fill_points){

	Region region;
	Vector<sg_point_t> result;
	int pour_points = 0;

	region = bitmap.get_viewable_region();

	printer().open_object("region");
	printer() << region;
	printer().close_object();

	sg_point_t point;
	for(point.x = 0; point.x < fill_points.width(); point.x++){
		for(point.y = 0; point.y < fill_points.height(); point.y++){
			if( (fill_points.get_pixel(point) != 0) && (bitmap.get_pixel(point) == 0) ){
				printer().message("Pour on %d,%d", point.x, point.y);
				bitmap.draw_pour(point, region);
				result.push_back(point);
			}
		}
	}

	return result;

}

void SvgFontManager::find_all_fill_points(const Bitmap & bitmap, Bitmap & fill_points, const Region & region, sg_size_t grid){
	fill_points.clear();
	bool is_fill;
	for(sg_int_t x = region.point().x(); x < region.dim().width(); x+=grid){
		for(sg_int_t y = 0; y < region.dim().height(); y+=grid){
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
	width = region.point().x() + region.dim().width();
	height = region.point().y() + region.dim().height();

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
		while( (temp.x >= region.point().x()) && (bitmap.get_pixel(temp) == 0) ){
			temp.x--;
		}

		if( temp.x >= region.point().x() ){
			boundary_count++;
			while( (temp.x >= region.point().x()) && (bitmap.get_pixel(temp) != 0) ){
				temp.x--;
			}

			if( temp.x >= region.point().x() ){
			}
		}
	} while( temp.x >= region.point().x() );

	if ((boundary_count % 2) == 0){
		return false;
	}

	temp = point;
	boundary_count = 0;
	do {
		while( (temp.y >= region.point().y()) && (bitmap.get_pixel(temp) == 0) ){
			temp.y--;
		}

		if( temp.y >= region.point().y() ){
			boundary_count++;
			while( (temp.y >= region.point().y()) && (bitmap.get_pixel(temp) != 0) ){
				temp.y--;
			}

			if( temp.y >= region.point().y() ){
			}
		}
	} while( temp.y >= region.point().y() );

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


