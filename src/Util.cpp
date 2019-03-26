/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved

#include <sapi/fmt.hpp>
#include "Util.hpp"

void Util::show_icon_file(const ConstString & path, sg_size_t canvas_size){
	File icon_file;
	Printer p;

	if( icon_file.open(path, File::RDONLY) < 0 ){
		printf("failed to open vector icon file\n");
		return;
	}

	Svic icon_collection(path);

	Bitmap canvas(canvas_size, canvas_size);
	VectorMap map(canvas);

	p.message("%d icons in collection", icon_collection.count());

	for(u32 i=0; i < icon_collection.count(); i++){
		p.key("name", icon_collection.name_at(i));
		VectorPath vector_path = icon_collection.at(i);
		vector_path << canvas.get_viewable_region();
		canvas.clear();
		sgfx::Vector::draw(canvas, vector_path, map);
		p.open_object("icon") << canvas << p.close();
	}

}


void Util::clean_path(const ConstString & path, const ConstString & suffix){
	Dir d;
	const char * entry;
	String str;

	if( d.open(path) < 0 ){
		printf("Failed to open path: '%s'\n", path.cstring());
	}

	while( (entry = d.read()) != 0 ){
		str = File::suffix(entry);
		if( str == "sbf" ){
			str.clear();
			str << path << "/" << entry;
			printf("Removing file: %s\n", str.cstring());
			File::remove(str);
		}
	}
}

void Util::show_file_font(const ConstString & path, bool is_details){

	FileFont ff;
	Ap::printer().info("Show font: %s\n", path.cstring());

	if( ff.set_file(path) < 0 ){
		printf("Failed to open font: '%s'\n", path.cstring());
		perror("Open failed");
		return;
	}

	show_font(ff);

	if( is_details ){
		File f;

		if( f.open(path, File::READONLY) < 0 ){
			Ap::printer().error("Failed to open file '%s'", path.cstring());
			return;
		}

		StructuredData<sg_font_header_t> header;
		if( f.read(header) != sizeof(sg_font_header_t) ){
			Ap::printer().error("failed to read header");
			return;
		}

		Vector<sg_font_kerning_pair_t> kerning_pairs;
		for(u32 i=0; i < header->kerning_pair_count; i++){
			sg_font_kerning_pair_t pair;
			if( f.read(&pair, sizeof(pair)) != sizeof(sg_font_kerning_pair_t) ){
				Ap::printer().error("Failed to read kerning pair");
				return;
			}
			kerning_pairs.push_back(pair);
		}

		Vector<sg_font_char_t> characters;
		for(u32 i=0; i < header->character_count; i++){
			sg_font_char_t character;
			printer().debug("read character from %d", f.seek(0, File::CURRENT));
			if( f.read(&character, sizeof(sg_font_char_t)) != sizeof(sg_font_char_t) ){
				Ap::printer().error("Failed to read kerning pair");
				return;
			}
			printer().debug("push character %d", character.id);
			characters.push_back(character);
		}

		Ap::printer().open_object("header");
		Ap::printer().key("version", "%d", header->version);
		Ap::printer().key("character_count", "%d", header->character_count);
		Ap::printer().key("max_word_width", "%d", header->max_word_width);
		Ap::printer().key("max_height", "%d", header->max_height);
		Ap::printer().key("bits_per_pixel", "%d", header->bits_per_pixel);
		Ap::printer().key("size", "%d", header->size);
		Ap::printer().key("kerning_pair_count", "%d", header->kerning_pair_count);
		Ap::printer().key("canvas_width", "%d", header->canvas_width);
		Ap::printer().key("canvas_height", "%d", header->canvas_height);
		Ap::printer().close_object();

		if( header->kerning_pair_count > 0 ){
			Ap::printer().open_array("kerning pairs");
			for(u32 i=0; i < kerning_pairs.count(); i++){
				Ap::printer().key("kerning", "%d %d %d",
										kerning_pairs.at(i).unicode_first,
										kerning_pairs.at(i).unicode_second,
										kerning_pairs.at(i).horizontal_kerning);

			}
			Ap::printer().close_array();
		} else {
			Ap::printer().info("no kerning pairs present");
		}

		Ap::printer().open_array("characters");
		for(u32 i=0; i < characters.count(); i++){
			Ap::printer().key("character", "%d canvas %d->%d,%d %dx%d advancex->%d offset->%d,%d",
									characters.at(i).id,
									characters.at(i).canvas_idx,
									characters.at(i).canvas_x,
									characters.at(i).canvas_y,
									characters.at(i).width,
									characters.at(i).height,
									characters.at(i).canvas_x,
									characters.at(i).advance_x,
									characters.at(i).offset_x,
									characters.at(i).offset_y);

		}
		Ap::printer().close_array();
	}
}

void Util::show_font(Font & f){
	Bitmap b;
	u32 i;


	Ap::printer().info("Alloc bitmap %d x %d with %d bpp",
							 f.get_width(),
							 f.get_height(),
							 f.bits_per_pixel());
	b.set_bits_per_pixel(f.bits_per_pixel());
	b.allocate(Area(f.get_width()*8/4, f.get_height()*5/4));

	for(i=0; i < Font::ascii_character_set().length(); i++){
		b.clear();
		String string;
		string << Font::ascii_character_set().at(i);

		if( i < Font::ascii_character_set().length()-1 ){
			string << Font::ascii_character_set().at(i+1);
			Ap::printer().info("Character: %c", Font::ascii_character_set().at(i+1));
		} else {
			Ap::printer().info("Character: %c", Font::ascii_character_set().at(i));
		}
		f.draw(string, b, Point(2, 0));
		Ap::printer().info("\twidth:%d height:%d xadvance:%d offsetx:%d offsety:%d",
								 f.character().width, f.character().height,
								 f.character().advance_x,
								 f.character().offset_x,
								 f.character().offset_y);

		Ap::printer() << b;
	}
}

void Util::show_system_font(int idx){

#if defined __link
	printf("System fonts not available\n");

#else
	printf("Not implemented\n");
#endif
}
