/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#include <cmath>
#include <sapi/fmt.hpp>
#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/chrono.hpp>
#include <sapi/hal.hpp>
#include <sapi/sgfx.hpp>
#include "SvgFontManager.hpp"

SvgFontManager::SvgFontManager() {
	// TODO Auto-generated constructor stub
	m_scale = 0.0f;
	m_object = 0;
	m_character_set = Font::ascii_character_set();
	m_is_show_canvas = true;
	m_bmp_font_manager.set_is_ascii();
}


int SvgFontManager::process_svg_icon_file(const ConstString & source_json){

	JsonDocument json_document;
	JsonObject top = json_document.load_from_file(source_json).to_object();
	String file_type = top.at("name").to_string();

	printer().open_object("svg.convert");
	printer().message("File type is %s", file_type.cstring());

	if( file_type != "svg" ){
		printer().error("wrong file type (not svg)");
		return -1;
	}

	JsonObject entry = top.at("childs").to_array().at(0).to_object();
	printer().message("entry: %s", entry.at("name").to_string().str());

	String value;
	JsonObject attributes = top.at("attrs").to_object();
	value = attributes.at("viewBox").to_string();
	printer().message("bbox: %s", value.str());

	Region bounds;
	Dim canvas_dimensions;

	if( value.is_empty() == false ){
		bounds = parse_bounds(value.cstring());
		canvas_dimensions = calculate_canvas_dimension(bounds, m_canvas_size);
		printer().open_object("SVG bounds") << bounds << printer().close();
		printer().open_object("Canvas Dimensions") << canvas_dimensions << printer().close();
	} else {
		printer().warning("Failed to find bounding box");
		return -1;
	}

	m_scale = ( SG_MAP_MAX * 1.0f ) / (bounds.dim().maximum_dimension());
	printer().message("Scaling factor is %0.2f", m_scale);

	//d is the path
	String name = entry.at("name").to_string();
	if( name == "path" ){
		JsonObject glyph = entry.at("attrs").to_object();
		String drawing_path;
		drawing_path = glyph.at("d").to_string();
		if( drawing_path != "<invalid>" ){
			printer().message("Read value:%s", drawing_path.cstring());

			Bitmap canvas;
			m_vector_path_icon_list = convert_svg_path(canvas, drawing_path, canvas_dimensions, m_pour_grid_size);

			printer().open_object("canvas size") << canvas.dim() << printer().close();
			printer() << canvas;
		}

	} else {
		printer().error("can't process %s", name.cstring());
	}

	return 0;
}

int SvgFontManager::process_svg_font_file(const ConstString & path){
	u32 j;

	JsonDocument json_document;
	JsonArray font_array = json_document.load_from_file(path).to_array();
	JsonObject top = font_array.at(0).to_object();
	String file_type = top.at("name").to_string();
	if( file_type == "<invalid>" ){
		top = json_document.load_from_file(path).to_object();
		file_type = top.at("name").to_string();
	}

	printer().open_object("svg.convert");
	printer().message("File type is %s", file_type.cstring());

	if( file_type != "svg" ){
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

	m_scale = (SG_MAP_MAX*1.7f) / (units_per_em);

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

	String output_name = File::name(path);
	u32 suffix = output_name.find('.');
	if( suffix != String::npos ){
		output_name.erase(suffix);
	}
	output_name << String().format("-%d.sbf", m_point_size / m_downsample.height());
	m_bmp_font_manager.generate_font_file(output_name);


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


		//create a map to move the kerning value to bitmap distance
#if 0
		s16 mapped_kerning = specified_kerning * m_scale; //kerning value in mapped vector space -- needs to be converted to bitmap distance

		VectorMap map;
		Region region(Point(), m_canvas_dimensions);
		map.calculate_for_region(region);

		Point kerning_point(mapped_kerning, 0);

		kerning_point.map(map);

		kerning_point -= region.center();
#else
		sg_size_t mapped_kerning = map_svg_value_to_bitmap(specified_kerning);
#endif
		kerning_pair.horizontal_kerning = mapped_kerning * kerning_sign; //needs to be mapped to canvas size

		printer().message("kerning %c %c %d -> %d", kerning_pair.unicode_first, kerning_pair.unicode_second, specified_kerning, kerning_pair.horizontal_kerning);

		m_bmp_font_manager.kerning_pair_list().push_back(kerning_pair);

	} else {
		return -1;
	}


	return 1;
}

sg_size_t SvgFontManager::map_svg_value_to_bitmap(u32 value){

	//create a map to move the kerning value to bitmap distance
	VectorMap map;
	Region region(Point(), m_canvas_dimensions);
	map.calculate_for_region(region);

	s16 mapped_value = value * m_scale; //kerning value in mapped vector space -- needs to be converted to bitmap distance
	if( mapped_value < 0 ){
		mapped_value = 32767;
	}
	printer().message("Mapped value is %d * %f = %d", value, m_scale, mapped_value);
	Point point(mapped_value, 0);

	point.map(map);

	point -= region.center();

	return point.x();
}

int SvgFontManager::process_glyph(const JsonObject & glyph){

	String glyph_name = glyph.at("glyphName").to_string();
	String unicode = glyph.at("unicode").to_string();
	u8 ascii_value;

	bool is_in_character_set = false;
	if( m_character_set.is_empty() == false ){
		if( unicode.length() == 1 ){
			ascii_value = unicode.at(0);
			if( m_character_set.find(ascii_value) != String::npos ){
				is_in_character_set = true;
			}
		} else {
			if( glyph_name == "ampersand" ){
				if( m_character_set.find("&") != String::npos ){
					ascii_value = '&';
					is_in_character_set = true;
				}
			}

			if( glyph_name == "quotesinglbase" ){
				if( m_character_set.find("\"") != String::npos ){
					ascii_value = '"';
					is_in_character_set = true;
				}
			}

			if( glyph_name == "less" ){
				if( m_character_set.find("\"") != String::npos ){
					ascii_value = '<';
					is_in_character_set = true;
				}
			}

			if( glyph_name == "greater" ){
				if( m_character_set.find("\"") != String::npos ){
					ascii_value = '>';
					is_in_character_set = true;
				}
			}
		}
	} else {
		is_in_character_set = true;
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
		Bitmap canvas;
		drawing_path = glyph.at("d").to_string();
		if( drawing_path != "<invalid>" ){
			printer().message("Read value:%s", drawing_path.cstring());
			if( parse_svg_path(drawing_path.cstring()) < 0 ){
				printer().error("failed to parse svg path");
				printer().close_object();
				return -1;
			}

			canvas.allocate(Dim(m_canvas_dimensions.width()+4,m_canvas_dimensions.height()+4));
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

			var::Vector<sg_point_t> fill_points = calculate_pour_points(canvas, fill);
			for(u32 i=0; i < fill_points.count(); i++){
				Point pour_point;
				pour_point = fill_points.at(i);
				pour_point.unmap(map);
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

			if( m_is_show_canvas ){
				printer().open_object(String().format("character-%s", unicode.cstring()));
				printer() << canvas;
				printer().close_object();
			}
#endif
		}

		Region active_region = canvas.calculate_active_region();
		Bitmap active_canvas(active_region.dim());
		active_canvas.draw_sub_bitmap(sg_point(0,0), canvas, active_region);

		Dim downsampled;
		downsampled.set_width( (active_canvas.width() + m_downsample.width()/2) / m_downsample.width() );
		downsampled.set_height( (active_canvas.height() + m_downsample.height()/2) / m_downsample.height() );
		Bitmap active_canvas_downsampled(downsampled);


		active_canvas_downsampled.clear();
		active_canvas_downsampled.downsample_bitmap(active_canvas, m_downsample);

		//find region inhabited by character

		sg_font_char_t character;

		character.id = ascii_value; //unicode value
		character.advance_x = map_svg_value_to_bitmap( x_advance.to_integer() ) / m_downsample.width(); //value from SVG file -- needs to translate to bitmap

		//derive width, height, offset_x, offset_y from image
		//for offset_x and offset_y what is the standard?

		character.width = active_canvas_downsampled.width(); //width of bitmap
		character.height = active_canvas_downsampled.height(); //height of the bitmap
		printer().message("(%d - %d) / %d is offset x", active_region.point().x(), m_canvas_origin.x(), m_downsample.width());
		character.offset_x = (active_region.point().x() - m_canvas_origin.x() + m_downsample.width()/2) / m_downsample.width(); //x offset when drawing the character
		printer().message("(%d - %d) / %d is offset x", active_region.point().y(), m_canvas_origin.y(), m_downsample.height());
		character.offset_y = (active_region.point().y() + m_downsample.height()/2) / m_downsample.height(); //y offset when drawing the character

		//add character to master canvas, canvas_x and canvas_y are location on the master canvas
		character.canvas_x = 0; //x location on master canvas -- set when font is generated
		character.canvas_y = 0; //y location on master canvas -- set when font is generated

		m_bmp_font_manager.character_list().push_back(character);
		m_bmp_font_manager.bitmap_list().push_back(active_canvas_downsampled);

#if !SHOW_ORIGIN
		if( m_is_show_canvas ){
			printer().open_object(String().format("active character-%s", unicode.cstring()));
			{
				printer() << active_region;
				printer() << active_canvas;
				printer() << active_canvas_downsampled;
				printer().open_object("character");
				{
					printer().key("advance x", "%d", character.advance_x);
					printer().key("offset x", "%d", character.offset_x);
					printer().key("offset y", "%d", character.offset_y);
					printer().key("width", "%d", character.width);
					printer().key("height", "%d", character.height);
					printer().close_object();
				}
				printer().close_object();
			}
		}
#endif


	}
	return 0;

}

void SvgFontManager::fit_icon_to_canvas(Bitmap & bitmap, VectorPath & vector_path, const VectorMap & map, bool recheck){
	Region region = bitmap.calculate_active_region();
	Point map_shift;
	Point bitmap_shift;
	float map_scale, width_scale, height_scale;

	printer().open_object("active region");
	printer() << region;
	printer().close_object();

	bitmap_shift = Point(bitmap.width()/2 - region.dim().width()/2 - region.point().x(),
								bitmap.height()/2 - region.dim().height()/2 - region.point().y());
	printer().message("Offset Error is %d,%d", bitmap_shift.x(), bitmap_shift.y());

	width_scale = bitmap.width() * 1.0f / region.dim().width();
	height_scale = bitmap.height() * 1.0f / region.dim().height();
	map_scale = (width_scale < height_scale ? width_scale : height_scale) * 0.99f;
	printer().message("scale %0.2f", map_scale);

	Region map_region = map.region();
	map_shift = Point(((s32)bitmap_shift.x() * SG_MAX*2 + map_region.dim().width()/2) / map_region.dim().width(),
							((s32)bitmap_shift.y() * SG_MAX*2 + map_region.dim().height()/2) / map_region.dim().height() * -1);

	printer().message("Map Shift is %d,%d", map_shift.x(), map_shift.y());

	if( recheck ){
		vector_path += map_shift;
		vector_path *= map_scale;

		bitmap.clear();

		sgfx::Vector::draw_path(bitmap, vector_path, map);

		fit_icon_to_canvas(bitmap, vector_path, map, false);
	}

	//bitmap.draw_rectangle(region.point(), region.dim());
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
		printer().message("Parse command char %c", d[i]);
		if( is_command_char(d[i]) ){
			ret = -1;
			command_char = d[i];
			i++;
			if( is_command_char(command_char) ){
				m_last_command_character = command_char;
			} else {
				command_char = m_last_command_character;
			}
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

	return path_commands().find(c) != String::npos;
}

Point SvgFontManager::convert_svg_coord(float x, float y, bool is_absolute){
	float temp_x;
	float temp_y;
	sg_point_t point;


	temp_x = x;
	temp_y = y;

	//scale
	temp_x = temp_x * m_scale;
	temp_y = temp_y * m_scale * -1.0f;

	if( is_absolute ){
		//shift
		temp_x = temp_x - SG_MAP_MAX/2.0f;
		temp_y = temp_y + SG_MAP_MAX/2.0f;
	}

	if( temp_x > SG_MAP_MAX ){
		printf("Can't map this point");
		exit(1);
	}

	if( temp_y > SG_MAP_MAX ){
		printf("Can't map this point");
		exit(1);
	}

	if( temp_x < -1*SG_MAP_MAX ){
		printf("Can't map this point");
		exit(1);
	}

	if( temp_y < -1*SG_MAP_MAX ){
		printf("Can't map this point");
		exit(1);
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

Region SvgFontManager::parse_bounds(const ConstString & value){
	Region result;
	Token bounds_tokens(value, " ");

	result << Point(roundf(bounds_tokens.at(0).to_float()), roundf(bounds_tokens.at(1).to_float()));
	result << Dim(roundf(bounds_tokens.at(2).to_float()) - m_bounds.point().x(), roundf(bounds_tokens.at(3).to_float()) - result.point().y());

	return result;

#if 0
	Dim canvas_dimensions;
	//how do bounds aspect ratio map to canvas size
	canvas_dimensions = result.dim();

	float scale = 1.0f * m_canvas_size / canvas_dimensions.maximum_dimension();

	m_canvas_dimensions = canvas_dimensions * scale;

	m_canvas_origin = Point(m_bounds.point().x()*-1 * m_canvas_dimensions.width() / (m_bounds.dim().width()),
									(m_bounds.dim().height() + m_bounds.point().y()) * m_canvas_dimensions.height() / (m_bounds.dim().height()) );

	m_point_size = unit_per_em * scale;

	return 0;
#endif
}

Dim SvgFontManager::calculate_canvas_dimension(const Region & bounds, sg_size_t canvas_size){
	float scale = 1.0f * canvas_size / bounds.dim().maximum_dimension();
	printer().message("scale:%f %d %d\n", scale, canvas_size, bounds.dim().maximum_dimension());
	return bounds.dim() * scale;
}

Point SvgFontManager::calculate_canvas_origin(const Region & bounds, const Dim & canvas_dimensions){
	return Point(bounds.point().x()* -1 * canvas_dimensions.width() / (bounds.dim().width()),
					 (bounds.dim().height() + bounds.point().y()) * canvas_dimensions.height() / (bounds.dim().height()) );
}

sg_size_t SvgFontManager::calculate_canvas_point_size(const Region & bounds, sg_size_t canvas_size, u32 units_per_em){
	float scale = 1.0f * canvas_size / bounds.dim().maximum_dimension();
	return units_per_em * scale;
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
	p[1] = m_current_point + convert_svg_coord(values[0], values[1], false);
	p[2] = m_current_point + convert_svg_coord(values[2], values[3], false);

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

var::Vector<sg_point_t> SvgFontManager::calculate_pour_points(Bitmap & bitmap, const Bitmap & fill_points){

	Region region;
	var::Vector<sg_point_t> result;

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

var::Vector<sg_vector_path_description_t> SvgFontManager::convert_svg_path(Bitmap & canvas, const var::ConstString & d, const Dim & canvas_dimensions, sg_size_t grid_size){
	var::Vector<sg_vector_path_description_t> elements;

	elements = process_svg_path(d);
	if( elements.count() > 0 ){
		canvas.allocate(canvas_dimensions);

		canvas.set_pen(Pen(1,1));
		VectorMap map(canvas);
		Bitmap fill(canvas.dim());


		sgfx::VectorPath vector_path;
		vector_path << elements << canvas.get_viewable_region();

		canvas.clear();

		map.set_rotation(0);
		sgfx::Vector::draw_path(canvas, vector_path, map);

		fit_icon_to_canvas(canvas, vector_path, map, true);

		printer().open_object("viewable canvas") << canvas.get_viewable_region() << printer().close();
		find_all_fill_points(canvas, fill, canvas.get_viewable_region(), grid_size);

		var::Vector<sg_point_t> fill_points = calculate_pour_points(canvas, fill);
		printer().message("%d fill points", fill_points.count());
		for(u32 i=0; i < fill_points.count(); i++){
			Point pour_point;
			pour_point = fill_points.at(i);
			pour_point.unmap(map);
			printer().message("add pour point");
			elements.push_back(sgfx::Vector::get_path_pour(pour_point));
		}

		vector_path << elements << canvas.get_viewable_region();
		canvas.clear();
		sgfx::Vector::draw_path(canvas, vector_path, map);
		Region active_region = canvas.calculate_active_region();
		printer() << vector_path;

		Bitmap active_bitmap( active_region.dim() );
		active_bitmap.clear();
		active_bitmap.draw_sub_bitmap(Point(), canvas, active_region);

		//canvas = active_bitmap;

		//The vector path needs to be adjusted so that the active region fills the entire vector space

		//analyze_icon(canvas, vector_path, map, true);


	}

	return elements;
}


var::Vector<sg_vector_path_description_t> SvgFontManager::process_svg_path(const ConstString & path){
	String modified_path;
	bool is_command;
	for(u32 i=0; i < path.length(); i++){
		is_command = path_commands_sign().find(path.at(i)) != String::npos;

		if( (path.at(i) == '-' &&
			  (path_commands().find(path.at(i-1)) == String::npos)) ||
			 path.at(i) != '-'){
			if( is_command ){ modified_path << " "; }
		}
		//- should have a space between numbers but not commands
		modified_path << path.at(i);
		//if( is_command ){ modified_path << " "; }
	}


	printer().message("modified path %s", modified_path.cstring());
	var::Vector<sg_vector_path_description_t> result;
	Token path_tokens(modified_path, " \n\t\r");
	u32 i = 0;
	char command_char = 0;
	Point current_point, control_point;
	while(i < path_tokens.count()){
		JsonObject object;
		float arg;
		float x,y,x1,y1,x2,y2;
		Point p;
		Point points[3];
		if( is_command_char(path_tokens.at(i).at(0)) ){
			command_char = path_tokens.at(i).at(0);
			if( path_tokens.at(i).length() > 1 ){
				String scan_string;
				scan_string.format("%c%%f", command_char, &arg);
				sscanf(path_tokens.at(i++).cstring(), scan_string.cstring(), &arg);
			}
		} else {
			printer().message("Repeat of %c", command_char);
			arg = path_tokens.at(i++).to_float();
		}


		switch(command_char){
			case 'M':
				//ret = parse_path_moveto_absolute(d+i);
				x = arg;
				y = path_tokens.at(i++).to_float();
				current_point = convert_svg_coord(x, y);
				result.push_back(sgfx::Vector::get_path_move(current_point));

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'm':
				//ret = parse_path_moveto_relative(d+i);
				x = arg;
				y = path_tokens.at(i++).to_float();
				current_point += convert_svg_coord(x, y, false);
				result.push_back(sgfx::Vector::get_path_move(current_point));

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'L':
				//ret = parse_path_lineto_absolute(d+i);
				x = arg;
				y = path_tokens.at(i++).to_float();
				p = convert_svg_coord(x, y);
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'l':
				//ret = parse_path_lineto_relative(d+i);
				x = arg;
				y = path_tokens.at(i++).to_float();
				p = convert_svg_coord(x, y, false);
				printer() << p << current_point;
				p += current_point;
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				printer() << sgfx::Vector::get_path_line(p);
				break;
			case 'H':
				//ret = parse_path_horizontal_lineto_absolute(d+i);

				x = arg;
				p = convert_svg_coord(x, current_point.y());
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'h':
				//ret = parse_path_horizontal_lineto_relative(d+i);

				x = arg;
				p = convert_svg_coord(x, 0, false);
				p.set(p.x() + current_point.x(), current_point.y());
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'V':
				//ret = parse_path_vertical_lineto_absolute(d+i);

				y = arg;
				p = convert_svg_coord(0, y);
				p.set(current_point.x(), p.y());
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'v':
				//ret = parse_path_vertical_lineto_relative(d+i);

				y = arg;
				p = convert_svg_coord(0, y, false);
				p.set(current_point.x(), p.y() + current_point.y());
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'C':
				//ret = parse_path_cubic_bezier_absolute(d+i);

				x1 = arg;
				y1 = path_tokens.at(i++).to_float();
				x2 = path_tokens.at(i++).to_float();
				y2 = path_tokens.at(i++).to_float();
				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				points[0] = convert_svg_coord(x1, y1);
				points[1] = convert_svg_coord(x2, y2);
				points[2] = convert_svg_coord(x, y);

				result.push_back(sgfx::Vector::get_path_cubic_bezier(points[0], points[1], points[2]));
				control_point = points[1];
				current_point = points[2];


				object.insert("command", JsonInteger(command_char));
				object.insert("x1", JsonReal( arg ));
				object.insert("y1", JsonReal( y1 ));
				object.insert("x2", JsonReal( x2 ));
				object.insert("y2", JsonReal( y2 ));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'c':
				//ret = parse_path_cubic_bezier_relative(d+i);

				x1 = arg;
				y1 = path_tokens.at(i++).to_float();
				x2 = path_tokens.at(i++).to_float();
				y2 = path_tokens.at(i++).to_float();
				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				points[0] = current_point + convert_svg_coord(x1, y1, false);
				points[1] = current_point + convert_svg_coord(x2, y2, false);
				points[2] = current_point + convert_svg_coord(x, y, false);

				result.push_back(sgfx::Vector::get_path_cubic_bezier(points[0], points[1], points[2]));
				control_point = points[1];
				current_point = points[2];

				object.insert("command", JsonInteger(command_char));
				object.insert("x1", JsonReal( arg ));
				object.insert("y1", JsonReal( y1 ));
				object.insert("x2", JsonReal( x2 ));
				object.insert("y2", JsonReal( y2 ));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'S':
				//ret = parse_path_cubic_bezier_short_absolute(d+i);

				x2 = arg;
				y2 = path_tokens.at(i++).to_float();
				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				points[0] = current_point*2 - control_point;
				points[0].set(2*current_point.x() - control_point.x(), 2*current_point.y() - control_point.y());
				points[1] = convert_svg_coord(x2, y2);
				points[2] = convert_svg_coord(x, y);

				result.push_back(sgfx::Vector::get_path_cubic_bezier(points[0], points[1], points[2]));

				control_point = points[1];
				current_point = points[2];

				object.insert("command", JsonInteger(command_char));
				object.insert("x2", JsonReal( x2 ));
				object.insert("y2", JsonReal( y2 ));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 's':
				//ret = parse_path_cubic_bezier_short_relative(d+i);

				x2 = arg;
				y2 = path_tokens.at(i++).to_float();
				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				points[0].set(2*current_point.x() - control_point.x(), 2*current_point.y() - control_point.y());
				//! \todo should convert_svg_coord be relative??
				points[1] = current_point + convert_svg_coord(x2, y2, false);
				points[2] = current_point + convert_svg_coord(x, y, false);

				result.push_back(sgfx::Vector::get_path_cubic_bezier(points[0], points[1], points[2]));
				control_point = points[1];
				current_point = points[2];

				object.insert("command", JsonInteger(command_char));
				object.insert("x2", JsonReal( x2 ));
				object.insert("y2", JsonReal( y2 ));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'Q':
				//ret = parse_path_quadratic_bezier_absolute(d+i);

				x1 = arg;
				y1 = path_tokens.at(i++).to_float();
				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				points[0] = convert_svg_coord(x1, y1);
				points[1] = convert_svg_coord(x, y);

				result.push_back(sgfx::Vector::get_path_quadratic_bezier(points[0], points[1]));
				control_point = points[0];
				current_point = points[1];

				m_path_description_list[m_object] = sgfx::Vector::get_path_quadratic_bezier(points[0], points[1]);
				m_object++;


				object.insert("command", JsonInteger(command_char));
				object.insert("x1", JsonReal( x1 ));
				object.insert("y1", JsonReal( y1 ));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'q':
				//ret = parse_path_quadratic_bezier_relative(d+i);

				x1 = arg;
				y1 = path_tokens.at(i++).to_float();
				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				points[0] = current_point + convert_svg_coord(x1, y1, false);
				points[1] = current_point + convert_svg_coord(x, y, false);

				result.push_back(sgfx::Vector::get_path_quadratic_bezier(points[0], points[1]));
				control_point = points[0];
				current_point = points[1];

				object.insert("command", JsonInteger(command_char));
				object.insert("x1", JsonReal( x1 ));
				object.insert("y1", JsonReal( y1 ));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'T':
				//ret = parse_path_quadratic_bezier_short_absolute(d+i);

				x = arg;
				y = path_tokens.at(i++).to_float();

				points[0].set(2*current_point.x() - control_point.x(), 2*current_point.y() - control_point.y());
				points[1] = convert_svg_coord(x, y);

				result.push_back(sgfx::Vector::get_path_quadratic_bezier(points[0], points[1]));
				control_point = points[0];
				current_point = points[1];

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 't':
				//ret = parse_path_quadratic_bezier_short_relative(d+i);

				x = arg;
				y = path_tokens.at(i++).to_float();

				points[0] = current_point*2 - control_point;

				points[0].set(2*current_point.x() - control_point.x(), 2*current_point.y() - control_point.y());
				points[1] = current_point + convert_svg_coord(x, y, false);

				result.push_back(sgfx::Vector::get_path_quadratic_bezier(points[0], points[1]));
				control_point = points[0];
				current_point = points[1];

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;

			case 'A':
				object.insert("command", JsonInteger(command_char));
				object.insert("rx", JsonReal( arg ));
				object.insert("ry", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("axis-rotation", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("large-arc-flag", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("sweep-flag", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("x", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("y", JsonReal( path_tokens.at(i++).to_float() ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;

			case 'a':
				object.insert("command", JsonInteger(command_char));
				object.insert("rx", JsonReal( arg ));
				object.insert("ry", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("axis-rotation", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("large-arc-flag", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("sweep-flag", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("x", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("y", JsonReal( path_tokens.at(i++).to_float() ));
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;

			case 'Z':
			case 'z':
				//ret = parse_close_path(d+i);

				result.push_back(sgfx::Vector::get_path_close());

				object.insert("command", JsonInteger(command_char));
				i++;
				printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			default:
				printer().message("Unhandled command char %c", command_char);
				return result;
		}
	}

	return result;
}


