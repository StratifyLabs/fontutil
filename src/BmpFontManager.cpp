/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#include "BmpFontManager.hpp"

BmpFontManager::BmpFontManager(){
	m_is_ascii = false;
}

int BmpFontManager::convert_directory(const ConstString & dir_path, bool overwrite, int verbose){
	Dir dir;
	String path;
	String font_bmp;
	String font_sbf;
	String font;
	const char * entry;

	path.assign(dir_path);
	Ap::printer().message("Open %s directory", dir_path.cstring());
	if( dir.open(path) < 0 ){
		Ap::printer().error("couldn't open path");
		return -1;
	}

	path << "/";

	Ap::printer().message("Read %s directory", path.cstring());
	while( (entry = dir.read()) != 0 ){
		font = entry;

		if( font.compare( font.size() - 4, 4, ".txt") == 0 ){
			font.cdata()[ font.size() - 3] = 0; //strip txt

			font_bmp.clear();
			font_bmp << path << font << "bmp";
			font_sbf.clear();
			font_sbf << path << font << "sbf";

			font.clear();
			font << path << entry;

			Ap::printer().message("Input:\n%s\n%s\n%s...",
										 font.cstring(),
										 font_bmp.cstring(),
										 font_sbf.cstring());

			if( (access(font_sbf.cstring(), R_OK ) == 0) && !overwrite ){
				Ap::printer().message("already exists");
			} else {
				Ap::printer().message("converting");
				BmpFontManager::load_font_from_bmp_files(font, font_bmp, font_sbf, Font::ascii_character_set(), verbose);
			}
		}
	}

	dir.close();
	return 0;
}

int BmpFontManager::load_font_from_bmp_files(const ConstString & def_file, const ConstString & bitmap_file, const ConstString & font_file, const ConstString & charset, int verbose){
	Bmp bmp(bitmap_file);
	File def;
	File font; //create a new font with this file

	int max_width;
	int max_height;
	bmpfont_char_t ch;

	sg_font_header_t header;
	sg_font_char_t character;

	int offset;
	unsigned int i;
	int ret;

	Region canvas_region;
	u32 canvas_size;
	u32 canvas_offset;
	Bitmap canvas;

	if( bmp.fileno() < 0 ){
		Ap::printer().message("Failed to open bitmap file");
		return -1;
	}

	if( def.open(def_file, File::RDONLY) < 0 ){
		Ap::printer().message("Failed to open definition file");
		return 0;
	}

	if( font.create(font_file) < 0 ){
		Ap::printer().error("failed to create font file");
		exit(0);
	}

	//load kerning info
	ret = get_kerning_count(def);
	if( ret < 0 ){
		header.kerning_pair_count = 0;
	} else {
		Ap::printer().message("%d kerning pairs", ret);
		header.kerning_pair_count = ret;
	}

	get_max(def, max_width, max_height);
	header.max_word_width = Bitmap::calc_word_width(max_width);
	header.max_height = max_height;
	header.version = SG_VERSION;
	header.bits_per_pixel = 1;

	header.canvas_width = header.max_word_width*32/canvas.bits_per_pixel()*3; //make the canvas eight times wider for efficiency
	header.canvas_height = header.max_height*3;
	if( canvas.alloc(header.canvas_width, header.canvas_height) < 0 ){
		Ap::printer().message("NO CANVAS! %d,%d", header.canvas_width, header.max_height);
		exit(1);
	}

	canvas_size = canvas.calc_size();

	if( charset != 0 ){
		header.character_count = Font::ascii_character_set().length();
	} else {
		//count the characters -- any chars with ID over 255
		header.character_count = 0;
		def.seek(0);
		while( scan_char(def, ch) == 0 ){
			if( verbose > 0 ){ Ap::printer().message("Scanned id %d", ch.id); }
			if( ch.id > 255 ){
				header.character_count++;
			}
		}
		Ap::printer().message("Scanned %d chars", header.character_count);
		def.seek(0);
	}

	header.size = sizeof(header) + header.character_count * sizeof(character) + sizeof(sg_font_kerning_pair_t) * header.kerning_pair_count;


	Ap::printer().message("Header Chars: %d Word Width: %d Height: %d",
								 header.character_count, header.max_word_width, header.max_height);

	font.write(&header, sizeof(header));

	bmpfont_kerning_t kerning;
	def.seek(0);
	while(get_kerning(def, kerning) == 0 ){
		//write kerning to file
		if( verbose > 0 ){
			Ap::printer().message("Kerning %d -> %d = %d", kerning.first, kerning.second, kerning.amount);
		}
		sg_font_kerning_pair_t pair;
		pair.unicode_first = kerning.first;
		pair.unicode_second = kerning.second;
		pair.horizontal_kerning = kerning.amount;
		font.write(&pair, sizeof(pair));
	}

	canvas.clear();

	for(i=0; i < header.character_count; i++){
		if( charset != 0 ){
			Ap::printer().message("Lookup %c...", charset[i]);
			ret = get_char(def, ch, charset[i]);
		} else {
			do {
				ch.id = 256;
				scan_char(def, ch);
			} while( ch.id <= 255 );

			if( ch.id == 256 ){
				ret = -1;
			} else {
				ret = 0;
			}
		}


		if( ret < 0 ){
			Ap::printer().message("Failed to find character");
			memset(&character, 0, sizeof(character));
			if( font.write(&character, sizeof(character)) != sizeof(character) ){
				Ap::printer().error("failed to write char entry");
			}
		} else {

			do {
				canvas_region = save_region_on_canvas(canvas, sg_dim(ch.width, ch.height), 4);
				if (canvas_region.point().x() < 0 ){
					if( verbose > 1 ){
						Ap::printer().message("\n");
						canvas.show();
					}
					//save the canvas
					canvas.clear();
				}
			} while( canvas_region.point().x() < 0 );

			character.id = ch.id;
			//character.resd = 0;
			character.height = ch.height;
			character.width = ch.width;
			character.canvas_idx = 0;
			character.canvas_x = 0;
			character.canvas_y = 0;
			character.offset_y = ch.yoffset;
			character.offset_x = ch.xoffset;
			character.advance_x = ch.xadvance;

			character_list().push_back(character);

#if 0
			if( verbose > 0 ){
				Ap::printer().message("Char %d is %d x %d at " F32U " advance %d offset:%d %d...",
											 ch.id,
											 character.width,
											 character.height,
											 current_canvas_idx,
											 character.advance_x,
											 ch.xoffset, ch.yoffset);
			}

			if( font.write(&character, sizeof(character)) != sizeof(character) ){
				Ap::printer().error("failed to write char entry");
			}

#endif
		}

	}

	def.seek(0);

	canvas.clear();

	offset = sizeof(sg_font_header_t) + sizeof(sg_font_kerning_pair_t)*header.kerning_pair_count;
	for(i=0; i < header.character_count; i++){
		font.read(offset, &character, sizeof(sg_font_char_t));
		offset += sizeof(sg_font_char_t);
		if( charset != 0 ){
			Ap::printer().message("Lookup %c...", charset[i]);
			ret = get_char(def, ch, charset[i]);
		} else {
			do {
				ch.id = 256;
				scan_char(def, ch);
			} while( ch.id <= 255 );
			if( ch.id == 256 ){
				ret = -1;
			} else {
				ret = 0;
			}		}

		if( ret < 0 ){
			Ap::printer().message("Failed to find load char");
		} else {
			//load the bitmap information and write it to the font file
			if( verbose > 0 ){
				Ap::printer().message("Load Bitmap at %d, %d -- %d x %d for %d write to offset %d %d,%d...",
											 ch.x, ch.y, ch.width, ch.height, ch.id, character.canvas_idx, character.canvas_x, character.canvas_y);
			}

		}

		canvas.clear();
		get_bitmap(bmp, ch, canvas, sg_point(character.canvas_x, character.canvas_y));
		bitmap_list().push_back(canvas);
	}
	Ap::printer().message("\n");


	if( verbose > 1 ){
		Ap::printer().message("\n");
		canvas.show();
	}


	//save the final canvas
	canvas_offset = header.size + character.canvas_idx*canvas_size;
	Ap::printer().message("Write canvas to " F32U, canvas_offset);
	font.write(canvas_offset, canvas.data(), canvas_size);

	Ap::printer().message("Total Font file size: %d\n", font.seek(0, File::CURRENT));

	font.close();
	def.close();
	bmp.close();
	return 0;

}

int BmpFontManager::generate_font_file(const ConstString & destination){
	File font_file;
	Bitmap master_canvas;
	u32 max_character_width = 0;
	u32 area;

	if( font_file.create(destination) <  0 ){
		return -1;
	}

	//populate the header
	sg_font_header_t header;
	area = 0;
	header.max_height = 0;

	for(u32 i = 0; i < character_list().count(); i++){
		if( character_list().at(i).width > max_character_width ){
			max_character_width = character_list().at(i).width;
		}

		if( character_list().at(i).height > header.max_height ){
			header.max_height = character_list().at(i).height;
		}

		area += (character_list().at(i).width*character_list().at(i).height);
	}

	header.version = SG_FONT_VERSION;
	header.bits_per_pixel = 1;
	header.max_word_width = Bitmap::calc_word_width(max_character_width);
	header.character_count = character_list().count();
	header.kerning_pair_count = kerning_pair_list().count();
	header.size = sizeof(header) + header.character_count*sizeof(sg_font_char_t) + header.kerning_pair_count*sizeof(sg_font_kerning_pair_t);
	header.canvas_width = header.max_word_width*2*32;
	header.canvas_height = header.max_height*3/2;
	header.bits_per_pixel = 1;

	if( master_canvas.set_bits_per_pixel(header.bits_per_pixel) < 0 ){
		printer().error("sgfx does not support %d bits per pixel", header.bits_per_pixel);
	}

	printer().key("max word width", "%d", header.max_word_width);
	printer().key("area", "%d", area);

	Dim master_dim(header.canvas_width, header.canvas_height);

	printer().key("master width", "%d", master_dim.width());
	printer().key("master height", "%d", master_dim.height());

	var::Vector<Bitmap> master_canvas_list;

	if( master_canvas.allocate(master_dim) < 0 ){
		printer().error("Failed to allocate memory for master canvas");
		return -1;
	}
	master_canvas.clear();

	if( bitmap_list().count() != character_list().count() ){
		printer().error("bitmap list count (%d) != character list count (%d)", bitmap_list().count(), character_list().count());
		return -1;
	}

	for(u32 i = 0; i < character_list().count(); i++){
		//find a place for the character on the master bitmap
		Region region;
		Dim character_dim(character_list().at(i).width, character_list().at(i).height);

		if( character_dim.width() && character_dim.height() ){
			do {

				for(u32 j = 0; j < master_canvas_list.count(); j++){
					region = find_space_on_canvas(master_canvas_list.at(j), bitmap_list().at(i).dim());
					if( region.is_valid() ){
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

#if SHOW_MASTER_CANVAS
	for(u32 i=0; i < master_canvas_list.count(); i++){
		printer().open_object(String().format("master canvas %d", i));
		printer() << master_canvas_list.at(i);
		printer().close_object();
	}
#endif

	if( font_file.write(&header, sizeof(header)) < 0 ){
		return -1;
	}

	for(u32 i = 0; i < kerning_pair_list().count(); i++){
		Data kerning_pair;
		kerning_pair.refer_to(&kerning_pair_list().at(i), sizeof(sg_font_kerning_pair_t));
		if( font_file.write(kerning_pair) != (int)kerning_pair.size() ){
			printer().error("failed to write kerning pair");
			return -1;
		}
	}

	//write characters in order
	for(u32 i = 0; i < 65535; i++){
		for(u32 j = 0; j < character_list().count(); j++){
			Data character;
			character.refer_to(&character_list().at(j), sizeof(sg_font_char_t));
			if( character_list().at(j).id == i ){
				if( font_file.write(character) != (int)character.size() ){
					printer().error("failed to write kerning pair");
					return -1;
				}
				break;
			}
		}
	}


	for(u32 i=0; i < master_canvas_list.count(); i++){
		if( font_file.write(master_canvas_list.at(i)) != (int)master_canvas_list.at(i).size() ){
			printer().error("Failed to write master canvas %d", i);
			return -1;
		}
	}

	font_file.close();

	//write the master canvas
	return 0;
}


int BmpFontManager::load_info(bmpfont_hdr_t & hdr, const Token & t){
	String str;
	int val;
	unsigned int i;

	for(i=1; i < t.size(); i++){
		Token m(t.at(i), "=");
		if( m.size() == 2 ){
			str = m.at(0);
			val = m.at(1).atoi();
			if( str == "size" ){ hdr.size = val; }
		}
	}
	return 0;
}

int BmpFontManager::load_kerning(bmpfont_kerning_t & c, const Token & t){
	String str;
	int val;
	unsigned int i;

	for(i=1; i < t.size(); i++){
		Token m(t.at(i), "=");
		if( m.size() == 2 ){
			str = m.at(0);
			val = m.at(1).atoi();
			if( str == "first" ){ c.first = val; }
			else if ( str == "second" ){ c.second = val; }
			else if ( str == "amount" ){ c.amount = val; }
		}
	}
	return 0;
}

int BmpFontManager::get_bitmap(Bmp & bmp, bmpfont_char_t c, Bitmap & canvas, sg_point_t loc){
	unsigned int i, j;
	int x = c.x;
	int y = c.y;
	int bytes = bmp.bits_per_pixel()/8;
	int offset = x*bytes;
	unsigned int width = c.width;
	unsigned int height = c.height;
	int avg;
	u8 pixel[3];

	for(j=0; j < height; j++){
		bmp.seek_row(y + j);
		bmp.seek(offset, Bmp::CURRENT);

		//Ap::printer().message("%02u ", j);
		for(i=0; i < width; i++){
			bmp.read(pixel, 3);
			avg = (pixel[0] + pixel[1] + pixel[2]) / 3;
			if( avg == 0xFF ){
				avg = 0;
				//Ap::printer().message("  ");
			} else {
				//Ap::printer().message("8");
				canvas.draw_pixel(sg_point(i+loc.x,j+loc.y));
			}
		}

		//Ap::printer().message(" %02u\n", j);
	}
	//Ap::printer().message("\n");
	return 0;
}

void BmpFontManager::show_char(Bmp & bmp, bmpfont_char_t c){
	unsigned int i, j;
	int x = c.x;
	int y = c.y;
	int bytes = bmp.bits_per_pixel()/8;
	int offset = x*bytes;
	unsigned int width = c.width;
	unsigned int height = c.height;
	int avg;
	uint8_t pixel[3];

	for(j=0; j < height; j++){
		bmp.seek_row(y + j);
		bmp.seek(offset, Bmp::CURRENT);

		Ap::printer().message("%02u ", j);
		for(i=0; i < width; i++){
			bmp.read(pixel, 3);
			avg = (pixel[0] + pixel[1] + pixel[2]) / 3;
			if( avg == 0xFF ){
				avg = 0;
				Ap::printer().message("  ");
			} else {
				Ap::printer().message("8");
			}
		}

		Ap::printer().message(" %02u\n", j);
	}
	Ap::printer().message("\n");
}

int BmpFontManager::load_char(bmpfont_char_t & c, const Token & t){
	String str;
	int val;
	unsigned int i;

	for(i=1; i < t.size(); i++){
		Token m(t.at(i), "=");
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

int BmpFontManager::get_max(File & def, int & w, int &h){
	def.seek(0);
	String str;
	String strm;

	w = 0;
	h = 0;

	while( def.gets(str) != 0 ){
		Token t(str, " \n");

		if( t.size() > 0 ){
			strm = t.at(0);
			bmpfont_char_t d;
			bmpfont_kerning_t k;
			memset(&d, 0, sizeof(d));
			memset(&k, 0, sizeof(k));

			if( strm == "char" ){
				if( load_char(d, t) == 0 ){
					if( d.width > w ){
						w = d.width;
					}

					if( d.height > h ){
						h = d.height;
					}

				}
			}

		}
	}
	return 0;
}

int BmpFontManager::get_kerning(File & def, bmpfont_kerning_t & k){
	String str;
	String strm;

	while( def.gets(str) != 0 ){
		Token t(str, " \n");

		if( t.size() > 0 ){
			strm = t.at(0);
			memset(&k, 0, sizeof(k));

			if( strm == "kerning" ){
				if( load_kerning(k, t) == 0 ){
					return 0;
				}
			}

		}
	}
	return -1;
}

int BmpFontManager::get_char(File & def, bmpfont_char_t & d, uint8_t ascii){
	def.seek(0);
	String str;
	String strm;

	str.set_capacity(256);

	while( def.gets(str) != 0 ){
		Token t(str, " \n");

		if( t.size() > 0 ){
			strm = t.at(0);
			memset(&d, 0, sizeof(d));

			if( strm == "char" ){
				if( load_char(d, t) == 0 ){
					if( d.id == ascii ){
						return 0;
					}

				}
			}

		}
	}
	return -1;
}

int BmpFontManager::get_kerning_count(File & def){
	def.seek(0);
	String str;
	String strm;
	int ret = -1;


	while( def.gets(str) ){
		Token t(str, " =\n");

		if( t.size() > 0 ){
			strm = t.at(0);
			if( strm == "kernings" ){
				if( t.size() > 1 ){
					ret = t.at(2).atoi();
					return ret;
				}
			}

		}

	}
	return -1;
}


int BmpFontManager::scan_char(File & def, bmpfont_char_t & d){
	String str;
	String strm;

	while( def.gets(str) != 0 ){
		Token t(str, " \n");

		if( t.size() > 0 ){
			strm = t.at(0);
			bmpfont_kerning_t k;
			memset(&d, 0, sizeof(d));
			memset(&k, 0, sizeof(k));

			if( strm == "char" ){
				if( load_char(d, t) == 0 ){
					return 0;
				}
			}
		}
	}
	return -1;
}

Region BmpFontManager::find_space_on_canvas(Bitmap & canvas, Dim dimensions){
	Region region;
	sg_point_t point;

	region << dimensions;

	for(point.y = 0; point.y < canvas.height() - dimensions.height(); point.y++){
		for(point.x = 0; point.x < canvas.width() - dimensions.width(); point.x++){
			region << point;
			if( canvas.is_empty(region) ){
				canvas.draw_rectangle(region.point(), region.dim());
				return region;
			}
		}
	}

	return Region();
}


Region BmpFontManager::save_region_on_canvas(Bitmap & canvas, Dim dimensions, int grid){
	sg_point_t point;
	sg_region_t region;
	bool is_free = true;

	region.dim = dimensions;

	for( region.point.x = 0; region.point.x <= canvas.width() - dimensions.width(); region.point.x++){
		for( region.point.y = 0; region.point.y <= canvas.height() - dimensions.height(); region.point.y++){
			is_free = true;
			for(point.y = region.point.y; point.y < region.point.y + dimensions.height(); point.y++){
				for(point.x = region.point.x; point.x < region.point.x + dimensions.width(); point.x++){
					if( canvas.get_pixel(point) != 0 ){
						is_free = false;
						break;
					}
				}
				if( is_free == false ){
					break;
				}
			}

			if( is_free ){
				canvas.set_pen(Pen(1,1));
				canvas.draw_rectangle(region);
				return region;
			}
		}
	}

	region.point.x = -1;
	region.point.y = -1;
	return region;

}

