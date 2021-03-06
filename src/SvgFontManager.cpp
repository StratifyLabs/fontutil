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
	m_is_show_canvas = true;
	m_bmp_font_generator.set_is_ascii();
	m_scale_sign_y = -1;
}


int SvgFontManager::process_svg_icon_file(const ConstString & source, const ConstString & dest){
	JsonDocument json_document;
	JsonArray array = json_document.load_from_file(source).to_array();

	if( array.count() == 0 ){
		printer().error("not icons in %s", source.cstring());
		return -1;
	}

	Svic vector_collection;

	if( vector_collection.create(dest) < 0 ){
		printer().error("Failed to create output file %s", dest.cstring());
		return -1;
	}



	for(u32 i=0; i < array.count(); i++){
		sg_vector_icon_header_t header;
		memset(&header, 0, sizeof(header));
		process_svg_icon(array.at(i).to_object());

		if( m_vector_path_icon_list.count() > 0 ){
			String name = array.at(i).to_object().at("title").to_string();
			if( name == "<invalid>" ){
				name = array.at(i).to_object().at("attrs").to_object().at("data-icon").to_string();
			}
			name.erase( name.find(".json") );
			if( vector_collection.append(name, m_vector_path_icon_list) < 0 ){
				printer().error("Failed to add %s to vector collection", name.cstring());
			}
		}

	}
	return 0;
}

int SvgFontManager::process_svg_icon(const JsonObject & object){


	String file_type = object.at("name").to_string();

	printer().open_object("svg.convert");
	printer().message("File type is %s", file_type.cstring());

	if( file_type != "svg" ){
		printer().error("wrong file type (not svg)");
		return -1;
	}

	JsonObject entry = object.at("childs").to_array().at(0).to_object();
	printer().message("entry: %s", entry.at("name").to_string().str());

	String value;
	JsonObject attributes = object.at("attrs").to_object();
	value = attributes.at("viewBox").to_string();
	printer().message("viewBox %s", value.str());


	if( value.is_empty() == false ){
		m_bounds = parse_bounds(value.cstring());
		m_canvas_dimensions = calculate_canvas_dimension(m_bounds, m_canvas_size);
		printer().open_object("SVG bounds") << m_bounds << printer().close();
		printer().open_object("Canvas Dimensions") << m_canvas_dimensions << printer().close();
	} else {
		printer().warning("Failed to find bounding box");
		return -1;
	}

	m_scale = ( SG_MAP_MAX * 1.0f ) / (m_bounds.area().maximum_dimension());
	printer().message("Scaling factor is %0.2f", m_scale);

	//d is the path
	String name = entry.at("name").to_string();
	if( name == "path" ){
		JsonObject glyph = entry.at("attrs").to_object();
		String drawing_path;
		drawing_path = glyph.at("d").to_string();
		if( drawing_path != "<invalid>" ){

			Bitmap canvas;
			m_vector_path_icon_list = convert_svg_path(canvas, drawing_path, m_canvas_dimensions, m_pour_grid_size, true);

			printer().open_object("canvas size") << canvas.area() << printer().close();
			printer() << canvas;
		}

	} else {
		printer().error("can't process %s", name.cstring());
	}

	printer().close_object();

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

	m_scale = (SG_MAP_MAX*1.0f) / (units_per_em);

	String value;
	value = attributes.at("bbox").to_string();
	printer().message("bbox: %s", value.str());

	if( value.is_empty() ){
		printer().warning("Failed to find bounding box");
		return -1;
	}

	m_bounds = parse_bounds(value.cstring());
	m_canvas_dimensions = calculate_canvas_dimension(m_bounds, m_canvas_size);
	m_canvas_origin = calculate_canvas_origin(m_bounds, m_canvas_dimensions);
	printer().open_object("SVG bounds") << m_bounds << printer().close();

	m_point_size = 1.0f * m_canvas_dimensions.height() * units_per_em / m_bounds.area().height() * SG_MAP_MAX / (SG_MAX);

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
	output_name << String().format("-%d", m_point_size / m_downsample.height());

	m_bmp_font_generator.set_generate_map( is_generate_map() );
	if( is_generate_map() ){
		String map_file;
		map_file << output_name << "-map.txt";
		m_bmp_font_generator.set_map_output_file(map_file);
	}

	m_bmp_font_generator.generate_font_file(output_name);

	printer().message("Created %s", output_name.cstring());

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

		if( character_set().find(first_string) == String::npos ){
			return 0;
		}

		second_string = kerning.at("u2").to_string();
		if( second_string == "<invalid>" || second_string.length() != 1 ){
			return 0;
		}

		if( character_set().find(second_string) == String::npos ){
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

		m_bmp_font_generator.kerning_pair_list().push_back(kerning_pair);

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
	Point point(mapped_value, 0);

	point.map(map);

	point -= region.center();

	return point.x();
}

int SvgFontManager::process_glyph(const JsonObject & glyph){

	String glyph_name = glyph.at("glyphName").to_string();
	String unicode = glyph.at("unicode").to_string();
	u8 ascii_value = ' ';

	bool is_in_character_set = false;
	if( character_set().is_empty() == false ){
		if( unicode.length() == 1 ){
			ascii_value = unicode.at(0);
			if( character_set().find(ascii_value) != String::npos ){
				is_in_character_set = true;
			}
		} else {
			if( glyph_name == "ampersand" ){
				if( character_set().find("&") != String::npos ){
					ascii_value = '&';
					is_in_character_set = true;
				}
			}

			if( (glyph_name == "quotesinglbase") || (glyph_name == "quotedbl") ){
				if( character_set().find("\"") != String::npos ){
					ascii_value = '"';
					is_in_character_set = true;
				}
			}

			if( glyph_name == "less" ){
				if( character_set().find("\"") != String::npos ){
					ascii_value = '<';
					is_in_character_set = true;
				}
			}

			if( glyph_name == "greater" ){
				if( character_set().find("\"") != String::npos ){
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

		String drawing_path;
		drawing_path = glyph.at("d").to_string();
		if( drawing_path == "<invalid>" ){
			printer().error("drawing path not found");
			return -1;
		}

		Bitmap canvas;
		m_vector_path_icon_list = convert_svg_path(canvas, drawing_path, m_canvas_dimensions, m_pour_grid_size, false);

#if 0
		printer().open_object("origin") << m_canvas_origin << printer().close();
		canvas.draw_line( Point(m_canvas_origin.x(), 0), Point(m_canvas_origin.x(), canvas.y_max()));
		canvas.draw_line(Point(0, m_canvas_origin.y()), Point(canvas.x_max(), m_canvas_origin.y()));

		if( m_is_show_canvas ){
			printer().open_object(String().format("character-%s", unicode.cstring()));
			printer() << canvas;
			printer().close_object();
		}
#endif


		Region active_region = canvas.calculate_active_region();
		Bitmap active_canvas(active_region.area());
		active_canvas.draw_sub_bitmap(sg_point(0,0), canvas, active_region);

		Area downsampled;
		downsampled.set_width( (active_canvas.width() + m_downsample.width()/2) / m_downsample.width() );
		downsampled.set_height( (active_canvas.height() + m_downsample.height()/2) / m_downsample.height() );
		Bitmap active_canvas_downsampled(downsampled);


		active_canvas_downsampled.clear();
		active_canvas_downsampled.downsample_bitmap(active_canvas, m_downsample);

		//find region inhabited by character

		sg_font_char_t character;

		character.id = ascii_value; //unicode value
		character.advance_x = (map_svg_value_to_bitmap( x_advance.to_integer() ) + m_downsample.width()/2) / m_downsample.width(); //value from SVG file -- needs to translate to bitmap

		//derive width, height, offset_x, offset_y from image
		//for offset_x and offset_y what is the standard?

		character.width = active_canvas_downsampled.width(); //width of bitmap
		character.height = active_canvas_downsampled.height(); //height of the bitmap
		character.offset_x = (active_region.point().x() - m_canvas_origin.x() + m_downsample.width()/2) / m_downsample.width(); //x offset when drawing the character
		printer().message("offset y %d - (%d - %d)", active_region.point().y(), m_canvas_origin.y(), m_point_size);
		character.offset_y = (active_region.point().y() - (m_canvas_origin.y() - m_point_size) + m_downsample.height()/2) / m_downsample.height(); //y offset when drawing the character

		//add character to master canvas, canvas_x and canvas_y are location on the master canvas
		character.canvas_x = 0; //x location on master canvas -- set when font is generated
		character.canvas_y = 0; //y location on master canvas -- set when font is generated

		m_bmp_font_generator.character_list().push_back(character);
		m_bmp_font_generator.bitmap_list().push_back(active_canvas_downsampled);

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

	bitmap_shift = Point(bitmap.width()/2 - region.area().width()/2 - region.point().x(),
								bitmap.height()/2 - region.area().height()/2 - region.point().y());
	printer().message("Offset Error is %d,%d", bitmap_shift.x(), bitmap_shift.y());

	width_scale = bitmap.width() * 1.0f / region.area().width();
	height_scale = bitmap.height() * 1.0f / region.area().height();
	map_scale = (width_scale < height_scale ? width_scale : height_scale) * 0.99f;
	printer().message("scale %0.2f", map_scale);

	Region map_region = map.region();
	map_shift = Point(((s32)bitmap_shift.x() * SG_MAX*2 + map_region.area().width()/2) / map_region.area().width(),
							((s32)bitmap_shift.y() * SG_MAX*2 + map_region.area().height()/2) / map_region.area().height() * m_scale_sign_y);

	printer().message("Map Shift is %d,%d", map_shift.x(), map_shift.y());

	if( recheck ){
		vector_path += map_shift;
		vector_path *= map_scale;

		bitmap.clear();

		sgfx::Vector::draw(bitmap, vector_path, map);

		fit_icon_to_canvas(bitmap, vector_path, map, false);
	}

	//bitmap.draw_rectangle(region.point(), region.area());
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
	temp_y = temp_y * m_scale * m_scale_sign_y;

	if( is_absolute ){
		//shift
		temp_x = temp_x - SG_MAP_MAX/2.0f;
		temp_y = temp_y - m_scale_sign_y * SG_MAP_MAX/2.0f;
	}

	if( temp_x > SG_MAX ){
		printer().message("Can't map this point x %0.2f > %d", temp_x, SG_MAX);
		exit(1);
	}

	if( temp_y > SG_MAX ){
		printer().message("Can't map this point y %0.2f > %d", temp_y, SG_MAX);
		exit(1);
	}

	if( temp_x < -1*SG_MAX ){
		printer().message("Can't map this point x %0.2f < %d", temp_x, -1*SG_MAX);
		exit(1);
	}

	if( temp_y < -1*SG_MAX ){
		printer().message("Can't map this point y %0.2f < %d", temp_y, -1*SG_MAX);
		exit(1);
	}


	point.x = rintf(temp_x);
	point.y = rintf(temp_y);

	return point;
}

Region SvgFontManager::parse_bounds(const ConstString & value){
	Region result;
	Tokenizer bounds_tokens(value, " ");

	result << Point(roundf(bounds_tokens.at(0).to_float()), roundf(bounds_tokens.at(1).to_float()));
	result << Area(roundf(bounds_tokens.at(2).to_float()) - result.point().x(), roundf(bounds_tokens.at(3).to_float()) - result.point().y());

	return result;
}

Area SvgFontManager::calculate_canvas_dimension(const Region & bounds, sg_size_t canvas_size){
	float scale = 1.0f * canvas_size / bounds.area().maximum_dimension();
	return bounds.area() * scale;
}

Point SvgFontManager::calculate_canvas_origin(const Region & bounds, const Area & canvas_dimensions){
	return Point( (-1*bounds.point().x()) * canvas_dimensions.width() / (bounds.area().width()),
					  (bounds.area().height() + bounds.point().y()) * canvas_dimensions.height() / (bounds.area().height()) );
}

var::Vector<sg_point_t> SvgFontManager::calculate_pour_points(Bitmap & bitmap, const var::Vector<FillPoint> & fill_points){

	Region region;
	var::Vector<sg_point_t> result;

	region = bitmap.get_viewable_region();

	printer().open_object("region");
	printer() << region;
	printer().close_object();

	for(u32 i=0; i < fill_points.count(); i++){
		Point p = fill_points.at(i).point();
		if( bitmap.get_pixel(p) == 0 ){
			bitmap.draw_pour(p, region);
			result.push_back(p);
		}
	}

	return result;

}

var::Vector<FillPoint> SvgFontManager::find_all_fill_points(const Bitmap & bitmap, const Region & region, sg_size_t grid){
	sg_size_t spacing;
	var::Vector<FillPoint> fill_point_spacing;
	for(sg_int_t x = region.point().x(); x < region.area().width(); x+=grid){
		for(sg_int_t y = 0; y < region.area().height(); y+=grid){
			spacing = is_fill_point(bitmap, sg_point(x,y), region);
			if( spacing ){
				fill_point_spacing.push_back(FillPoint(Point(x,y), spacing));
			}
		}
	}
	fill_point_spacing.sort( var::Vector<FillPoint>::descending );
	return fill_point_spacing;
}

sg_size_t SvgFontManager::is_fill_point(const Bitmap & bitmap, sg_point_t point, const Region & region){
	int boundary_count;
	sg_color_t color;
	sg_point_t temp = point;

	color = bitmap.get_pixel(point);
	if( color != 0 ){
		return false;
	}

	sg_size_t width;
	sg_size_t height;

	sg_size_t spacing;

	width = region.point().x() + region.area().width();
	height = region.point().y() + region.area().height();

	boundary_count = 0;

	do {
		while( (temp.x < width) && (bitmap.get_pixel(temp) == 0) ){
			temp.x++;
		}

		spacing = temp.x - point.x;

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

		//distance to first boundary
		if( point.x - temp.x < spacing ){
			spacing = point.x - temp.x;
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
		return 0;
	}

	temp = point;
	boundary_count = 0;
	do {
		while( (temp.y >= region.point().y()) && (bitmap.get_pixel(temp) == 0) ){
			temp.y--;
		}

		//distance to first boundary
		if( point.y - temp.y < spacing ){
			spacing = point.y - temp.y;
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
		return 0;
	}

	//return spacing;

	temp = point;
	boundary_count = 0;
	do {
		while( (temp.y < height) && (bitmap.get_pixel(temp) == 0) ){
			temp.y++;
		}

		//distance to first boundary
		if( temp.y - point.y < spacing ){
			spacing = temp.y - point.y;
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
		return 0;
	}

	return spacing;

}

var::Vector<sg_vector_path_description_t> SvgFontManager::convert_svg_path(Bitmap & canvas,
																									const var::ConstString & d,
																									const Area & canvas_dimensions,
																									sg_size_t grid_size,
																									bool is_fit_icon){
	var::Vector<sg_vector_path_description_t> elements;

	elements = process_svg_path(d);
	if( elements.count() > 0 ){
		canvas.allocate(canvas_dimensions);

		canvas.set_pen(Pen(1,1,true));
		VectorMap map(canvas);


		sgfx::VectorPath vector_path;
		vector_path << elements << canvas.get_viewable_region();

		canvas.clear();

		map.set_rotation(0);
		sgfx::Vector::draw(canvas, vector_path, map);

		if( is_fit_icon ){
			fit_icon_to_canvas(canvas, vector_path, map, true);
		}

		var::Vector<FillPoint> fill_spacing_points;
		fill_spacing_points = find_all_fill_points(canvas, canvas.get_viewable_region(), grid_size);

		var::Vector<sg_point_t> fill_points = calculate_pour_points(canvas, fill_spacing_points);
		for(u32 i=0; i < fill_points.count(); i++){
			Point pour_point;
			pour_point = fill_points.at(i);
			pour_point.unmap(map);
			elements.push_back(sgfx::Vector::get_path_pour(pour_point));
		}

		vector_path << elements << canvas.get_viewable_region();
		canvas.clear();
		printer().open_object("vector path") << vector_path << printer().close();
		sgfx::Vector::draw(canvas, vector_path, map);
		canvas.set_pen(Pen(0));
		for(u32 i=0; i < fill_points.count(); i++){
			printer().info("pour at %d,%d", fill_points.at(i).x, fill_points.at(i).y);
			canvas.draw_pixel(fill_points.at(i));
		}
		Region active_region = canvas.calculate_active_region();

		Bitmap active_bitmap( active_region.area() );
		active_bitmap.clear();
		active_bitmap.draw_sub_bitmap(Point(), canvas, active_region);
		//canvas = active_bitmap;



	}

	return elements;
}


var::Vector<sg_vector_path_description_t> SvgFontManager::process_svg_path(const ConstString & path){
	String modified_path;
	bool is_command;
	bool has_dot = false;
	for(u32 i=0; i < path.length(); i++){
		is_command = path_commands_sign().find(path.at(i)) != String::npos;

		if( (path.at(i) == ' ') || is_command ){
			has_dot = false;
		}

		if( path.at(i) == '.' ){
			if( has_dot == true ){
				modified_path << " ";
			} else {
				has_dot = true;
			}
		}

		if( (path.at(i) == '-' &&
			  (path_commands().find(path.at(i-1)) == String::npos)) ||
			 path.at(i) != '-'){
			if( is_command ){ modified_path << " "; }
		}
		//- should have a space between numbers but not commands
		modified_path << path.at(i);
	}


	//printer().message("modified path %s", modified_path.cstring());
	var::Vector<sg_vector_path_description_t> result;
	printer().message("modified path %s", modified_path.cstring());
	Tokenizer path_tokens(modified_path, " \n\t\r");
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
			arg = path_tokens.at(i++).to_float();
		}

#if 0
		if( result.count() > 7 ){
			return result;
		}
#endif

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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'l':
				//ret = parse_path_lineto_relative(d+i);
				x = arg;
				y = path_tokens.at(i++).to_float();
				p = convert_svg_coord(x, y, false);
				p += current_point;
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			case 'H':
				//ret = parse_path_horizontal_lineto_absolute(d+i);

				x = arg;
				p = convert_svg_coord(x, 0);
				p.set(p.x(), current_point.y());
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("command", JsonInteger(command_char));
				object.insert("x", JsonReal( x ));
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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

				object.insert("command", JsonInteger(command_char));
				object.insert("x1", JsonReal( x1 ));
				object.insert("y1", JsonReal( y1 ));
				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
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
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;

			case 'A':

				object.insert("command", JsonInteger(command_char));
				object.insert("rx", JsonReal( arg ));
				object.insert("ry", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("axis-rotation", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("large-arc-flag", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("sweep-flag", JsonReal( path_tokens.at(i++).to_float() ));

				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				p = convert_svg_coord(x, y);
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());


				break;

			case 'a':
				object.insert("command", JsonInteger(command_char));
				object.insert("rx", JsonReal( arg ));
				object.insert("ry", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("axis-rotation", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("large-arc-flag", JsonReal( path_tokens.at(i++).to_float() ));
				object.insert("sweep-flag", JsonReal( path_tokens.at(i++).to_float() ));

				x = path_tokens.at(i++).to_float();
				y = path_tokens.at(i++).to_float();

				p = current_point + convert_svg_coord(x, y, false);
				result.push_back(sgfx::Vector::get_path_line(p));
				current_point = p;

				object.insert("x", JsonReal( x ));
				object.insert("y", JsonReal( y ));
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;

			case 'Z':
			case 'z':
				//ret = parse_close_path(d+i);

				result.push_back(sgfx::Vector::get_path_close());

				object.insert("command", JsonInteger(command_char));
				i++;
				//printer().message("%c: %s", command_char, JsonDocument().stringify(object).cstring());
				break;
			default:
				printer().message("Unhandled command char %c", command_char);
				return result;
		}
	}

	return result;
}


