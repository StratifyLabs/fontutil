/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#include "BmpFont.hpp"

BmpFont::BmpFont(){}

int BmpFont::convert_directory(const char * dir_path, bool overwrite, int verbose){
	Dir dir;
	Dir dir_size;
	String path;
	String font_bmp;
	String font_sbf;
	String font;
	const char * entry;

	path.assign(dir_path);
	printf("Open %s directory\n", dir_path);
	if( dir.open(path) < 0 ){
		perror("couldn't open path");
		return -1;
	}

	path << "/";

	printf("Read %s directory\n", path.c_str());
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

			printf("Input:\n%s\n%s\n%s...",
					font.c_str(),
					font_bmp.c_str(),
					font_sbf.c_str());

			if( (access(font_sbf.c_str(), R_OK ) == 0) && !overwrite ){
				printf("already exists\n");
			} else {
				printf("converting\n");
				BmpFont::gen_fonts(font, font_bmp, font_sbf, Font::charset(), verbose);
			}
		}
	}

	dir.close();
	return 0;
}

int BmpFont::gen_fonts(const char * def_file, const char * bitmap_file, const char * font_file, const char * charset, int verbose){
	Bmp bmp(bitmap_file);
	File def;
	File font; //create a new font with this file

	String str;
	String strm;
	int max_width;
	int max_height;
	bmpfont_char_t ch;

	sg_font_header_t header;
	sg_font_char_t character;

	int offset;
	unsigned int i;
	int ret;

	u32 current_canvas_idx;
	Region canvas_region;
	u32 canvas_size;
	u32 canvas_offset;
	Bitmap canvas;

	if( bmp.fileno() < 0 ){
		printf("Failed to open bitmap file\n");
		return -1;
	}

	if( def.open(def_file, File::RDONLY) < 0 ){
		printf("Failed to open definition file\n");
		return 0;
	}

	if( font.create(font_file) < 0 ){
		perror("failed to create font file");
		exit(0);
	}

	//load kerning info
	ret = get_kerning_count(def);
	if( ret < 0 ){
		header.kerning_pairs = 0;
	} else {
		printf("%d kerning pairs\n", ret);
		header.kerning_pairs = ret;
	}

	get_max(def, max_width, max_height);
	header.max_word_width = Bitmap::calc_word_width(max_width);
	header.max_height = max_height;
	header.version = SG_VERSION;
	header.bits_per_pixel = Bitmap::bits_per_pixel();

	header.canvas_width = header.max_word_width*32/canvas.bits_per_pixel()*3; //make the canvas eight times wider for efficiency
	header.canvas_height = header.max_height*3;
	if( canvas.alloc(header.canvas_width, header.canvas_height) < 0 ){
		printf("NO CANVAS! %d,%d\n", header.canvas_width, header.max_height);
		exit(1);
	}

	canvas_size = canvas.calc_size();

	if( charset != 0 ){
		header.num_chars = Font::CHARSET_SIZE;
	} else {
		//count the characters -- any chars with ID over 255
		header.num_chars = 0;
		def.seek(0);
		while( scan_char(def, ch) == 0 ){
			if( verbose > 0 ){ printf("Scanned id %d\n", ch.id); }
			if( ch.id > 255 ){
				header.num_chars++;
			}
		}
		printf("Scanned %d chars\n", header.num_chars);
		def.seek(0);
	}

	header.size = sizeof(header) + header.num_chars * sizeof(character) + sizeof(sg_font_kerning_pair_t) * header.kerning_pairs;


	printf("Header Chars: %d Word Width: %d Height: %d\n",
			header.num_chars, header.max_word_width, header.max_height);

	font.write(&header, sizeof(header));

	bmpfont_kerning_t kerning;
	def.seek(0);
	while(get_kerning(def, kerning) == 0 ){
		//write kerning to file
		if( verbose > 0 ){ printf("Kerning %d -> %d = %d\n", kerning.first, kerning.second, kerning.amount); }
		sg_font_kerning_pair_t pair;
		pair.first = kerning.first;
		pair.second = kerning.second;
		pair.kerning = kerning.amount;
		font.write(&pair, sizeof(pair));
	}

	current_canvas_idx = 0;
	canvas.clear();

	for(i=0; i < header.num_chars; i++){
		if( charset != 0 ){
			printf("Lookup %c...", charset[i]);
			fflush(stdout);
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
			printf("Failed to find character\n");
			memset(&character, 0, sizeof(character));
			if( font.write(&character, sizeof(character)) != sizeof(character) ){
				perror("failed to write char entry");
			}
		} else {

			do {
				canvas_region = save_region_on_canvas(canvas, sg_dim(ch.width, ch.height), 4);
				if (canvas_region.x() < 0 ){
					if( verbose > 1 ){
						printf("\n");
						canvas.show();
					}
					//save the canvas
					canvas.clear();
					current_canvas_idx++;
				}
			} while( canvas_region.x() < 0 );

			character.id = ch.id;
			//character.resd = 0;
			character.height = ch.height;
			character.width = ch.width;
			character.canvas_idx = current_canvas_idx;
			character.canvas_x = canvas_region.x();
			character.canvas_y = canvas_region.y();
			character.yoffset = ch.yoffset;
			character.xoffset = ch.xoffset;
			character.xadvance = ch.xadvance;
			if( verbose > 0 ){
				printf("Char %d is %d x %d at %ld advance %d offset:%d %d...",
						ch.id,
						character.width,
						character.height,
						current_canvas_idx,
						character.xadvance,
						ch.xoffset, ch.yoffset);
			}

			if( font.write(&character, sizeof(character)) != sizeof(character) ){
				perror("failed to write char entry");
			}
		}

		printf("\n");
	}

	def.seek(0);

	current_canvas_idx = 0;
	canvas.clear();

	offset = sizeof(sg_font_header_t) + sizeof(sg_font_kerning_pair_t)*header.kerning_pairs;
	for(i=0; i < header.num_chars; i++){
		font.read(offset, &character, sizeof(sg_font_char_t));
		offset += sizeof(sg_font_char_t);
		if( charset != 0 ){
			printf("Lookup %c...", charset[i]);
			fflush(stdout);
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
			printf("Failed to find load char\n");
		} else {
			//load the bitmap information and write it to the font file
			if( verbose > 0 ){
				printf("Load Bitmap at %d, %d -- %d x %d for %d write to offset %d %d,%d...",
						ch.x, ch.y, ch.width, ch.height, ch.id, character.canvas_idx, character.canvas_x, character.canvas_y);
			}

			if( current_canvas_idx != character.canvas_idx ){
				//save the canvas to the file
				canvas_offset = header.size + current_canvas_idx*canvas_size;
				printf("Write canvas to %ld\n", canvas_offset);
				font.write(canvas_offset, canvas.data(), canvas_size);
				current_canvas_idx = character.canvas_idx;
				if( verbose > 1 ){
					printf("\n");
					canvas.show();
				}
				canvas.clear();
			}

			get_bitmap(bmp, ch, canvas, sg_point(character.canvas_x, character.canvas_y));
		}
		printf("\n");
	}

	if( verbose > 1 ){
		printf("\n");
		canvas.show();
	}


	//save the final canvas
	canvas_offset = header.size + character.canvas_idx*canvas_size;
	printf("Write canvas to %ld\n", canvas_offset);
	font.write(canvas_offset, canvas.data(), canvas_size);

	printf("Total Font file size: %d\n", font.seek(0, File::CURRENT));

	font.close();
	def.close();
	bmp.close();
	return 0;

}


int BmpFont::load_info(bmpfont_hdr_t & hdr, const Token & t){
	String str;
	int val;
	unsigned int i;

	for(i=1; i < t.size(); i++){
		Token m(t.at(i), "=");
		if( m.size() == 2 ){
			str = m.at(0);
			val = atoi(m.at(1));
			if( str == "size" ){ hdr.size = val; }
		}
	}
	return 0;
}

int BmpFont::load_kerning(bmpfont_kerning_t & c, const Token & t){
	String str;
	int val;
	unsigned int i;

	for(i=1; i < t.size(); i++){
		Token m(t.at(i), "=");
		if( m.size() == 2 ){
			str = m.at(0);
			val = atoi(m.at(1));
			if( str == "first" ){ c.first = val; }
			else if ( str == "second" ){ c.second = val; }
			else if ( str == "amount" ){ c.amount = val; }
		}
	}
	return 0;
}

int BmpFont::get_bitmap(Bmp & bmp, bmpfont_char_t c, Bitmap & canvas, sg_point_t loc){
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

		//printf("%02u ", j);
		for(i=0; i < width; i++){
			bmp.read(pixel, 3);
			avg = (pixel[0] + pixel[1] + pixel[2]) / 3;
			if( avg == 0xFF ){
				avg = 0;
				//printf("  ");
			} else {
				//printf("8");
				canvas.draw_pixel(sg_point(i+loc.x,j+loc.y));
			}
		}

		//printf(" %02u\n", j);
	}
	//printf("\n");
	return 0;
}

void BmpFont::show_char(Bmp & bmp, bmpfont_char_t c){
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

		printf("%02u ", j);
		for(i=0; i < width; i++){
			bmp.read(pixel, 3);
			avg = (pixel[0] + pixel[1] + pixel[2]) / 3;
			if( avg == 0xFF ){
				avg = 0;
				printf("  ");
			} else {
				printf("8");
			}
		}

		printf(" %02u\n", j);
	}
	printf("\n");
}

int BmpFont::load_char(bmpfont_char_t & c, const Token & t){
	String str;
	int val;
	unsigned int i;

	for(i=1; i < t.size(); i++){
		Token m(t.at(i), "=");
		if( m.size() == 2 ){
			str = m.at(0);
			val = atoi(m.at(1));
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

int BmpFont::get_max(File & def, int & w, int &h){
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

int BmpFont::get_kerning(File & def, bmpfont_kerning_t & k){
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

int BmpFont::get_char(File & def, bmpfont_char_t & d, uint8_t ascii){
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

int BmpFont::get_kerning_count(File & def){
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
					ret = atoi(t.at(2));
					return ret;
				}
			}

		}

	}
	return -1;
}


int BmpFont::scan_char(File & def, bmpfont_char_t & d){
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

Region BmpFont::save_region_on_canvas(Bitmap & canvas, Dim dimensions, int grid){
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

