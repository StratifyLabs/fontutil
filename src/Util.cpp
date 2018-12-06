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

	Svic icon_collection(icon_file);

	Bitmap canvas(canvas_size, canvas_size);
	sg_vector_icon_header_t icon_header;
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
		printf("Failed to open path: '%s'\n", path.str());
	}

	while( (entry = d.read()) != 0 ){
		str = File::suffix(entry);
		if( str == "sbf" ){
			str.clear();
			str << path << "/" << entry;
			printf("Removing file: %s\n", str.str());
			File::remove(str);
		}
	}
}

void Util::show_file_font(const ConstString & path){
	FileFont ff;

	printf("Show font: %s\n", path.str());

	if( ff.set_file(path) < 0 ){
		printf("Failed to open font: '%s'\n", path.str());
		perror("Open failed");
		return;
	}

	show_font(ff);
}

void Util::show_font(Font & f){
	Bitmap b;
	u32 i;

	printf("Alloc bitmap %d x %d\n", f.get_width(), f.get_height());
	b.allocate(Area(f.get_width()*8/4, f.get_height()*5/4));

	for(i=0; i < Font::ascii_character_set().length(); i++){
		printf("Character: %c\n", Font::ascii_character_set().at(i));
		b.clear();
		String string;
		string << Font::ascii_character_set().at(i);
		if( i < Font::ascii_character_set().length()-1 ){
			string << Font::ascii_character_set().at(i+1);
		}
		f.draw(string, b, Point(2, 0));
		printf("\twidth:%d height:%d xadvance:%d offsetx:%d offsety:%d\n",
				 f.character().width, f.character().height,
				 f.character().advance_x,
				 f.character().offset_x,
				 f.character().offset_y);
		b.show();
	}
}

void Util::show_system_font(int idx){

#if defined __link
	printf("System fonts not available\n");

#else
	Assets::init();
	Font * f;

	f = Assets::font(idx);
	if( f != 0 ){
		printf("Show System font %d of %d\n", idx, Assets::font_count());
		show_font(*f);
	} else {
		printf("System font %d does not exist\n", idx);
	}
#endif
}
