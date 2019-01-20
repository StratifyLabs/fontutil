/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#include "BmpFontManager.hpp"

BmpFontManager::BmpFontManager(){

}

int BmpFontManager::convert_directory(const ConstString & dir_path, bool overwrite){
	Dir dir;
	String path;
	String font_bmp;
	String font_sbf;
	String font;
	String entry;

	path.assign(dir_path);
	printer().message("Open %s directory", dir_path.cstring());
	if( dir.open(path) < 0 ){
		printer().error("couldn't open path");
		return -1;
	}

	path << "/";

	printer().message("Read %s directory", path.cstring());
	while( (entry = dir.read()) != 0 ){
		font = entry;

		printer().debug("check entry %s", font.cstring());

		if( font.find(".txt") != String::npos ){
			font.erase(".txt");

			font_bmp.clear();
			font_bmp << path << font << ".bmp";
			font_sbf.clear();
			font_sbf << path << font << ".sbf";

			font.clear();
			font << path << entry;

			printer().debug("Input: %s %s %s...",
								 font.cstring(),
								 font_bmp.cstring(),
								 font_sbf.cstring());

			printer().message("converting");
			convert_font(font);
			//BmpFontManager::load_font_from_bmp_files(font, font_bmp, font_sbf, Font::ascii_character_set(), verbose);

		}
	}

	dir.close();
	return 0;
}

int BmpFontManager::convert_font(const ConstString & path){

	String definition_path;
	String bmp_path;
	String destintation_path;
	String map_path;

	File definition_file;

	definition_path << path << ".txt";
	bmp_path << path << ".bmp";
	destintation_path << path;
	map_path << path << "-map.txt";

	if( definition_file.open(definition_path, File::READONLY) <	0 ){
		printer().error("failed to open definition file %s", definition_path.cstring());
		return -1;
	}

	Bmp bmp_file(bmp_path);

	printer().info("import character definitions and bitmaps");
	populate_lists_from_bitmap_definition(definition_file, bmp_file);
	printer().info("import kerning pairs");
	populate_kerning_pair_list_from_bitmap_definition(definition_file);
	printer().info("generate font file");
	m_generator.set_bits_per_pixel(bits_per_pixel());
	if( is_generate_map() ){
		m_generator.set_generate_map();
		m_generator.set_map_output_file(map_path);

	}

	m_generator.generate_font_file(destintation_path);

	return 0;
}

int BmpFontManager::populate_lists_from_bitmap_definition(const File & def, const Bmp & bitmap_file){
	bmpfont_char_t bmp_definition;

	create_color_index(bitmap_file);

	load_bmp_characters(def);
	int ascii_count = 0;
	if( is_ascii() ){
		printer().message("search for ascii characters");
		//see if there are any ascii chars
		for(u32 i=1; i < m_bmp_characters.count(); i++){
			if( m_bmp_characters.at(i).id < 126 && m_bmp_characters.at(i).id > 32 ){
				ascii_count++;
				break;
			}
		}

		if( ascii_count == 0 ){
			printer().info("no ascii characters found -- convert all");
			set_character_set("");
		}
	}


	if( character_set().length() == 0 ){
		//just convert all items in the file
		for(u32 i=0; i < m_bmp_characters.count(); i++){
			add_character_to_lists(m_bmp_characters.at(i), bitmap_file);
		}
	} else {

		//first char is 1 because ' ' is omitted
		for(u32 i=1; i < character_set().length(); i++){

			if( get_char(def, bmp_definition, character_set().at(i)) < 0 ){
				printer().error("Failed to load character %d", Font::ascii_character_set().at(i));
			} else {
				add_character_to_lists(bmp_definition, bitmap_file);
#if 0
				sg_font_character.advance_x = bmp_definition.xadvance;
				sg_font_character.id = bmp_definition.id;
				sg_font_character.offset_x = bmp_definition.xoffset; //this is populate later
				sg_font_character.offset_y = bmp_definition.yoffset;

				Bitmap character_bitmap = get_bitmap(bitmap_file, bmp_definition);
				printer().open_object("loaded character", Printer::DEBUG) << character_bitmap << printer().close();
				m_generator.bitmap_list().push_back(character_bitmap);

				sg_font_character.height = character_bitmap.height();
				sg_font_character.width = character_bitmap.width();

				//load the bitmap from the bitmap file
				m_generator.character_list().push_back(sg_font_character);
#endif
			}
		}
	}

	return 0;
}

int BmpFontManager::add_character_to_lists(const bmpfont_char_t & d, const Bmp & bitmap_file){
	sg_font_char_t sg_font_character;
	sg_font_character.advance_x = d.xadvance;
	sg_font_character.id = d.id;
	sg_font_character.offset_x = d.xoffset; //this is populate later
	sg_font_character.offset_y = d.yoffset;

	Bitmap character_bitmap = get_bitmap(bitmap_file, d);
	printer().open_object("loaded character", Printer::DEBUG) << character_bitmap << printer().close();
	m_generator.bitmap_list().push_back(character_bitmap);

	sg_font_character.height = character_bitmap.height();
	sg_font_character.width = character_bitmap.width();

	//load the bitmap from the bitmap file
	m_generator.character_list().push_back(sg_font_character);
	return 0;
}

int BmpFontManager::populate_kerning_pair_list_from_bitmap_definition(const File & def){

	def.seek(0);

	String line;
	while( (line = def.gets()).is_empty() == false ){
		Tokenizer tokens(line, " \t");

		if( tokens.at(0) == "kerning" ){
			Tokenizer first(tokens.at(1),"=");
			Tokenizer second(tokens.at(2),"=");
			Tokenizer amount(tokens.at(3),"=");

			sg_font_kerning_pair_t kerning_pair;
			kerning_pair.unicode_first = first.at(1).to_integer();
			kerning_pair.unicode_second = second.at(1).to_integer();
			kerning_pair.horizontal_kerning = amount.at(1).to_integer()*-1;
			m_generator.kerning_pair_list().push_back(kerning_pair);
		}
	}

	return 0;
}

int BmpFontManager::create_color_index(const Bmp & bitmap_file){
	u32 i,j,k;
	u8 pixel[3];

	for(j=0; j < bitmap_file.height(); j++){
		bitmap_file.seek_row(j);
		for(i=0; i < bitmap_file.width(); i++){
			bitmap_file.read(pixel, 3);
			u32 color = (pixel[0] + pixel[1] + pixel[2]) / 3;

			//see if the color exists in the index
			for(k=0; k < m_bmp_color_index.count(); k++){
				if( color == m_bmp_color_index.at(k) ){
					break;
				}
			}
			if( k == m_bmp_color_index.count() ){
				m_bmp_color_index.push_back(color);
			}
		}
	}
	printer().message("%d colors in bitmap", m_bmp_color_index.count());
	m_bmp_color_index.sort(Vector<u32>::ascending);
	printer().set_flags(Printer::PRINT_32 | Printer::PRINT_UNSIGNED);
	printer().open_object("color index", Printer::DEBUG) << m_bmp_color_index << printer().close();
	return 0;
}



Bitmap BmpFontManager::get_bitmap(const Bmp & bmp, bmpfont_char_t c){
	unsigned int i, j;
	int x = c.x;
	int y = c.y;
	int bytes = bmp.bits_per_pixel()/8;
	int offset = x*bytes;
	unsigned int width = c.width;
	unsigned int height = c.height;
	int avg;
	u8 pixel[3];
	u32 num_colors = 1<< bits_per_pixel();
	u32 idx;

	Bitmap result(Area(width, height), bits_per_pixel());
	result.clear();

	for(j=0; j < height; j++){
		bmp.seek_row(y + j);
		bmp.seek(offset, Bmp::CURRENT);

		for(i=0; i < width; i++){
			bmp.read(pixel, 3);
			avg = (pixel[0] + pixel[1] + pixel[2]) / 3;

			//where is brightness in index
			idx = m_bmp_color_index.find(avg);
			sg_color_t bitmap_color;
			bitmap_color = num_colors - idx * num_colors / m_bmp_color_index.count() - 1;
			result.set_pen_color(bitmap_color);
			result.draw_pixel(Point(i,j));
		}
	}

	Region active_region = result.calculate_active_region();
	Region full_height_active_region(Point(active_region.point().x(), 0),
												Area(active_region.width(), result.height()));

	Bitmap active_result(full_height_active_region.area(), bits_per_pixel());

	active_result.draw_sub_bitmap(Point(0,0), result, full_height_active_region);

	return active_result;
}

int BmpFontManager::get_char(const File & def, bmpfont_char_t & d, uint8_t ascii){

	for(u32 i=0; i < m_bmp_characters.count(); i++){
		if( ascii == m_bmp_characters.at(i).id ){
			d = m_bmp_characters.at(i);
			return 0;
		}
	}

	return -1;
}

int BmpFontManager::load_bmp_characters(const File & def){
	def.seek(0);
	String str;
	String strm;
	bmpfont_char_t d;

	str.set_capacity(256);

	while( def.gets(str) != 0 ){
		Tokenizer t(str, " \n");
		if( t.size() > 0 ){
			strm = t.at(0);
			memset(&d, 0, sizeof(d));
			if( strm == "char" ){
				if( load_char(d, t) == 0 ){
					m_bmp_characters.push_back(d);
				}
			}
		}
	}
	return 0;

}

int BmpFontManager::load_char(bmpfont_char_t & c, const Tokenizer & t){
	String str;
	int val;
	unsigned int i;

	for(i=1; i < t.size(); i++){
		Tokenizer m(t.at(i), "=");
		if( m.size() == 2 ){
			str = m.at(0);
			val = m.at(1).atoi();
			if( str == "id" ){ c.id = val; }
			else if ( str == "x" ){ c.x = val; }
			else if ( str == "y" ){ c.y = val; }
			else if ( str == "width" ){ c.width = val; }
			else if ( str == "height" ){ c.height = val; }
			else if ( str == "xoffset" ){ c.xoffset = val; }
			else if ( str == "yoffset" ){ c.yoffset = val; }
			else if ( str == "xadvance" ){ c.xadvance = val; }
			else if ( str == "page" ){ c.page = val; }
			else if ( str == "chnl" ){ c.channel = val; }
		}
	}
	return 0;
}


