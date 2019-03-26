#include "BmpFontGenerator.hpp"

BmpFontGenerator::BmpFontGenerator(){
	m_is_ascii = false;
}

int BmpFontGenerator::generate_font_file(const ConstString & destination){
	File font_file;
	u32 max_character_width = 0;
	u32 area;

	String output_name;
	output_name << destination << ".sbf";

	if( font_file.create(output_name) <  0 ){
		return -1;
	}

	//populate the header
	sg_font_header_t header;
	area = 0;
	header.max_height = 0;
	sg_size_t min_offset_x = 65535, min_offset_y = 65535;

	for(u32 i = 0; i < character_list().count(); i++){
		if( character_list().at(i).width > max_character_width ){
			max_character_width = character_list().at(i).width;
		}

		if( character_list().at(i).height > header.max_height ){
			header.max_height = character_list().at(i).height;
		}

		if( character_list().at(i).offset_x < min_offset_x ){
			min_offset_x = character_list().at(i).offset_x;
		}

		if( character_list().at(i).offset_y < min_offset_y ){
			min_offset_y = character_list().at(i).offset_y;
		}

		area += (character_list().at(i).width*character_list().at(i).height);
	}

	printer().debug("Max width is %d", max_character_width);
	printer().debug("Max height is %d", header.max_height);

	for(u32 i=0; i < character_list().count(); i++){
		character_list().at(i).advance_x = character_list().at(i).width+1;
		character_list().at(i).offset_x = 0;
		character_list().at(i).offset_y -= min_offset_y;
	}


	header.version = SG_FONT_VERSION;
	header.bits_per_pixel = bits_per_pixel();
	header.max_word_width = Bitmap::calc_word_width(max_character_width);
	header.character_count = character_list().count();
	header.kerning_pair_count = kerning_pair_list().count();
	header.size = sizeof(header) + header.character_count*sizeof(sg_font_char_t) + header.kerning_pair_count*sizeof(sg_font_kerning_pair_t);
	header.canvas_width = header.max_word_width*2*32;
	header.canvas_height = header.max_height*3/2;

	var::Vector<Bitmap> master_canvas_list = build_master_canvas(header);
	if( master_canvas_list.count() == 0 ){
		return -1;
	}

	for(u32 i=0; i < master_canvas_list.count(); i++){
		printer().open_object(String().format("master canvas %d", i), Printer::DEBUG);
		printer() << master_canvas_list.at(i);
		printer().close_object();
	}

	printer().debug("write header to file");

	printer().info("header bits_per_pixel %d", header.bits_per_pixel);
	printer().info("header max_word_width %d", header.max_word_width);
	printer().info("header character_count %d", header.character_count);
	printer().info("header kerning_pair_count %d", header.kerning_pair_count);
	printer().info("header size %d", header.size);
	printer().info("header canvas_width %d", header.canvas_width);
	printer().info("header canvas_height %d", header.canvas_height);


	if( font_file.write(&header, sizeof(header)) < 0 ){
		return -1;
	}

	printer().debug("write kerning pairs to file");

	printer().open_array("kerning pairs", Printer::DEBUG);

	for(u32 i = 0; i < kerning_pair_list().count(); i++){
		Data kerning_pair;
		kerning_pair.refer_to(&kerning_pair_list().at(i), sizeof(sg_font_kerning_pair_t));
		printer().debug("kerning %d -> %d -- %d",
							 kerning_pair_list().at(i).unicode_first,
							 kerning_pair_list().at(i).unicode_second,
							 kerning_pair_list().at(i).horizontal_kerning);
		if( font_file.write(kerning_pair) != (int)kerning_pair.size() ){
			printer().error("failed to write kerning pair");
			return -1;
		}
	}
	printer().close_array();

	//write characters in order
	printer().info("write characters to file");
	for(u32 i = 0; i < 65535; i++){
		for(u32 j = 0; j < character_list().count(); j++){
			Data character;
			character.refer_to(&character_list().at(j), sizeof(sg_font_char_t));

			if( character_list().at(j).id == i ){
				printer().debug("write character %d to file on %d: %d,%d %dx%d at %d",
									 character_list().at(j).id,
									 character_list().at(j).canvas_idx,
									 character_list().at(j).canvas_x,
									 character_list().at(j).canvas_y,
									 character_list().at(j).width,
									 character_list().at(j).height,
									 font_file.seek(0, File::CURRENT));
				if( font_file.write(character) != (int)character.size() ){
					printer().error("failed to write kerning pair");
					return -1;
				}
				break;
			}
		}
	}

	//write the master canvas
	printer().info("write master canvas to file");
	for(u32 i=0; i < master_canvas_list.count(); i++){
		printer().debug("write master canvas %d to file at %d",
							 i, font_file.seek(0, File::CURRENT));
		if( font_file.write(master_canvas_list.at(i)) != (int)master_canvas_list.at(i).size() ){
			printer().error("Failed to write master canvas %d", i);
			return -1;
		}
	}

	if( is_generate_map() ){
		printer().info("generate map file");
		generate_map_file(header, master_canvas_list);
	}

	font_file.close();

	return 0;
}

int BmpFontGenerator::generate_map_file(const sg_font_header_t & header, const var::Vector<Bitmap> & master_canvas_list){
	File map_output;
	printer().message("create map file %s", m_map_output_file.cstring());
	if( map_output.create(m_map_output_file) < 0 ){
		printer().error("Failed to create map output file");
	} else {

		printer().info("create map with %d bits per pixel", master_canvas_list.at(0).bits_per_pixel());
		map_output.write("----------------------------------------------------\n");
		map_output.write(String().format("version:0x%04X\n", header.version));
		map_output.write(String().format("size:%d\n", header.size));
		map_output.write(String().format("character_count:%d\n", header.character_count));
		map_output.write(String().format("max_word_width:%d\n", header.max_word_width));
		map_output.write(String().format("bits_per_pixel:%d\n", header.bits_per_pixel));
		map_output.write(String().format("canvas_width:%d\n", header.canvas_width));
		map_output.write(String().format("canvas_height:%d\n", header.canvas_height));
		map_output.write(String().format("kerning_pair_count:%d\n", header.kerning_pair_count));
		map_output.write(String().format("bits_per_pixel:%d\n", header.bits_per_pixel));
		map_output.write("----------------------------------------------------\n");
		for(u32 i=0; i < kerning_pair_list().count(); i++){
			map_output.write(String().format("kerning %d %d %d\n",
														kerning_pair_list().at(i).unicode_first,
														kerning_pair_list().at(i).unicode_second,
														kerning_pair_list().at(i).horizontal_kerning));
		}

		for(u32 i=0; i < character_list().count(); i++){

			map_output.write("----------------------------------------------------\n");
			map_output.write(String().format("id:%d\n", character_list().at(i).id));
			map_output.write(String().format("width:%d\n", character_list().at(i).width));
			map_output.write(String().format("height:%d\n", character_list().at(i).height));
			map_output.write(String().format("advance_x:%d\n", character_list().at(i).advance_x));
			map_output.write(String().format("offset_x:%d\n", character_list().at(i).offset_x));
			map_output.write(String().format("offset_y:%d\n", character_list().at(i).offset_y));

			Bitmap canvas(Area(character_list().at(i).width, character_list().at(i).height), bits_per_pixel());
			canvas.clear();
			canvas.draw_sub_bitmap(Point(), master_canvas_list.at(character_list().at(i).canvas_idx),
										  Region(
											  Point(character_list().at(i).canvas_x, character_list().at(i).canvas_y),
											  canvas.area()
											  ));
			String pixel_character;
			for(sg_size_t h = 0; h < canvas.height(); h++){
				for(sg_size_t w = 0; w < canvas.width(); w++){
					pixel_character.format("%c", Printer::get_bitmap_pixel_character(canvas.get_pixel(Point(w,h)), bits_per_pixel()));
					map_output.write(pixel_character);
				}
				map_output.write("|\n");
			}
		}

	}

	map_output.close();
	return 0;
}


var::Vector<Bitmap> BmpFontGenerator::build_master_canvas(const sg_font_header_t & header){
	var::Vector<Bitmap> master_canvas_list;
	Bitmap master_canvas;

	if( master_canvas.set_bits_per_pixel(header.bits_per_pixel) < 0 ){
		printer().error("sgfx does not support %d bits per pixel", header.bits_per_pixel);
		return master_canvas_list;
	}

	printer().info("max word width %d", header.max_word_width);

	Area master_dim(header.canvas_width, header.canvas_height);

	printer().info("master width %d", master_dim.width());
	printer().info("master height %d", master_dim.height());

	if( master_canvas.allocate(master_dim) < 0 ){
		printer().error("Failed to allocate memory for master canvas");
		return master_canvas_list;
	}
	master_canvas.clear();

	if( bitmap_list().count() != character_list().count() ){
		printer().error("bitmap list count (%d) != character list count (%d)", bitmap_list().count(), character_list().count());
		return master_canvas_list;
	}

	for(u32 i = 0; i < character_list().count(); i++){
		//find a place for the character on the master bitmap
		Region region;
		Area character_dim(character_list().at(i).width, character_list().at(i).height);

		if( character_dim.width() && character_dim.height() ){
			do {

				for(u32 j = 0; j < master_canvas_list.count(); j++){
					region = find_space_on_canvas(master_canvas_list.at(j), bitmap_list().at(i).area());
					if( region.is_valid() ){
						printer().debug("allocate %d (%c) to master canvas %d at %d,%d",
											 character_list().at(i).id,
											 character_list().at(i).id,
											 j,
											 region.x(), region.y());
						character_list().at(i).canvas_x = region.x();
						character_list().at(i).canvas_y = region.y();
						character_list().at(i).canvas_idx = j;
						break;
					}
				}

				if( region.is_valid() == false ){
					if( master_canvas_list.push_back(master_canvas) < 0 ){
						printer().error("failed to add additional canvas");
						return -1;
					}
				}

			} while( !region.is_valid() );
		}
	}

	for(u32 i = 0; i < character_list().count(); i++){
		Point p(character_list().at(i).canvas_x, character_list().at(i).canvas_y);
		master_canvas_list.at(character_list().at(i).canvas_idx).draw_bitmap(p, bitmap_list().at(i));
	}

	return master_canvas_list;
}

String BmpFontGenerator::parse_map_line(const var::ConstString & title, const String & line){
	Tokenizer tokens(line, ":\n");

	if( title == "id" ){
		if( tokens.count() == 1 ){
			return String(":");
		}
	}

	if( tokens.count() != 2 ){
		printer().error("Wrong token number with %s", line.cstring());
		return String("");
	}
	if( tokens.at(0) != title ){
		return String("");
	}
	return tokens.at(1);
}

String BmpFontGenerator::get_map_value(const var::Vector<var::String> & lines, const var::ConstString & title){

	for(u32 i = 0; i < lines.count(); i++){
		Tokenizer tokens(lines.at(i), ":\n");

		if( title == "id" ){
			if( tokens.count() == 1 ){
				return String(":");
			}
		}

		if( tokens.at(0) == title ){
			return tokens.at(1);
		}
	}
	return String();
}

int BmpFontGenerator::import_map(const ConstString & map){
	File map_file;
	String path;

	path << map << "-map.txt";

	if( map_file.open(path, File::RDONLY) < 0 ){
		printer().error("Failed to open map file %s", path.cstring());
		return -1;
	}
	sg_font_header_t header;


	//pull the maps in
	String line;
	int line_number = 0;
	Bitmap canvas;

	line = map_file.gets();
	if( line.find("----") != 0 ){
		printer().error("top line should be ----");
		return -1;
	}

	Vector<String> header_lines;

	printer().debug("loading header");
	while( (line = map_file.gets()).find("----") == String::npos ){
		line.replace("\n", "");
		header_lines.push_back(line);
		printer().debug("loaded header line '%s'", line.cstring());
	}

	printer().debug("loaded %d lines for header", header_lines.count());

	header.version = get_map_value(header_lines, "version").to_long(16);
	if( header.version == 0 ){
		printer().warning("failed to read version");
	}
	header.character_count = get_map_value(header_lines, "character_count").to_integer();
	if( header.character_count == 0 ){
		printer().warning("failed to read character_count");
	}
	header.max_word_width = get_map_value(header_lines, "max_word_width").to_integer();
	if( header.max_word_width == 0 ){
		printer().warning("failed to read max_word_width");
	}
	header.max_height = get_map_value(header_lines, "max_height").to_integer();
	if( header.max_height == 0 ){
		printer().warning("failed to read max_height");
	}
	header.bits_per_pixel = get_map_value(header_lines, "bits_per_pixel").to_integer();
	if( header.bits_per_pixel == 0 ){
		printer().error("map must specify non-zero value of 1,2,4 or 8 for bits per pixel");
		return -1;
	}
	set_bits_per_pixel(header.bits_per_pixel);

	header.size = get_map_value(header_lines, "size").to_integer();
	if( header.size == 0 ){
		printer().warning("failed to read size");
	}
	header.kerning_pair_count = get_map_value(header_lines, "kerning_pair_count").to_integer();
	if( header.kerning_pair_count == 0 ){
		printer().warning("failed to read kerning_pair_count");
	}
	header.canvas_width = get_map_value(header_lines, "canvas_width").to_integer();
	if( header.canvas_width == 0 ){
		printer().warning("failed to read canvas_width");
	}
	header.canvas_height = get_map_value(header_lines, "canvas_height").to_integer();
	if( header.version == 0 ){
		printer().warning("failed to read canvas_height");
	}

	//read the kerning
	Vector<String> kerning_lines;

	while( (line = map_file.gets()).find("----") == String::npos ){
		line.replace("\n", "");
		kerning_lines.push_back(line);
		printer().debug("loaded kerning line '%s'", line.cstring());
	}


	if( kerning_lines.count() != header.kerning_pair_count ){
		printer().error("kerning lines does not match header %d != %d",
							 header.kerning_pair_count, kerning_lines.count());
		return -1;
	}

	for(u32 i=0; i < kerning_lines.count(); i++){
		Tokenizer tokens(kerning_lines.at(i), " ");
		if( tokens.count() != 4 ){
			printer().error("malformed kerning line %s", kerning_lines.at(i).cstring());
			return -1;
		}

		sg_font_kerning_pair_t pair;
		pair.unicode_first = tokens.at(1).to_integer();
		pair.unicode_second = tokens.at(2).to_integer();
		pair.horizontal_kerning = tokens.at(3).to_integer();
		m_kerning_pair_list.push_back(pair);
	}

	Vector<String> character_lines;
	character_list().resize(0);

	do {
		sg_font_char_t character;
		line_number++;

		character_lines.resize(0);

		while( ((line = map_file.gets()).find("----") == String::npos) && (line.is_empty() == false) ){
			line.replace("\n", "");
			character_lines.push_back(line);
			printer().debug("loaded character line '%s'", line.cstring());
		}

		if( character_lines.count() > 0 ){
			character.id = get_map_value(character_lines, "id").at(0);
			printer().debug("loaded %d lines for character %d", character_lines.count(), character.id);
			character.width = get_map_value(character_lines, "width").to_integer();
			character.height = get_map_value(character_lines, "height").to_integer();
			character.advance_x = get_map_value(character_lines, "advance_x").to_integer();
			character.offset_x = get_map_value(character_lines, "offset_x").to_integer();
			character.offset_y = get_map_value(character_lines, "offset_y").to_integer();

			printer().debug("checking lines vs character height for %d (%d -6 == %d)",
								 character.id, character_lines.count(), character.height);

			if( character_lines.count() - 6 != character.height ){
				printer().error("character %c height mismatch %d != %d",
									 character.id,
									 character.height,
									 character_lines.count() - 6);
				return -1;
			}

			canvas.allocate(Area(character.width, character.height));
			canvas.clear();
			for(sg_size_t h = 0; h < character.height; h++){
				line = character_lines.at(h+6); line_number++;
				if( line.find("|") != character.width ){
					printer().error("Error on line:%d | is misaligned", line_number);
				}
				for(sg_size_t w = 0; w < character.width; w++){
					sg_color_t color = 0;
					if( line.at(w) != ' ' ){ color = 1; }
					canvas.set_pen_color(color);
					canvas.draw_pixel(Point(w,h));
				}
			}
			if( canvas.height() == 0 ){
				canvas.allocate(Area(32, 1));
				canvas.clear();
			}
			bitmap_list().push_back(canvas);
			character_list().push_back(character);
			printer().open_object(String().format("id:%d", character.id)) << canvas << printer().close();
		}
	} while( character_lines.count() );

	map_file.close();

	if( bitmap_list().count() != character_list().count() ){
		printer().error("Bitmap and character mismatch %d != %d", bitmap_list().count(), character_list().count());
		return -1;
	}

	for(u32 i=0; i < character_list().count(); i++){
		printer().debug("character list[%d] = %d", i, character_list().at(i).id);
	}

	return 0;

}

Region BmpFontGenerator::find_space_on_canvas(Bitmap & canvas, Area dimensions){
	Region region;
	sg_point_t point;

	region << dimensions;

	for(point.y = 0; point.y < canvas.height() - dimensions.height(); point.y++){
		for(point.x = 0; point.x < canvas.width() - dimensions.width(); point.x++){
			region << point;
			if( canvas.is_empty(region) ){
				canvas.draw_rectangle(region.point(), region.area());
				return region;
			}
		}
	}

	return Region();
}
