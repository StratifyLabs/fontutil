/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#include "Util.hpp"

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
		printf("Failed to open font: '%s'\n", path);
		perror("Open failed");
		return;
	}

	show_font(ff);
}

void Util::show_font(Font & f){
	Bitmap b;
	int i;

	printf("Alloc bitmap %d x %d\n", f.get_width(), f.get_height());
	b.alloc(f.get_width(), f.get_height());

	for(i=0; i < Font::CHARSET_SIZE; i++){
		printf("Character: %c\n", Font::charset()[i]);
		b.clear();
		f.draw_char(Font::charset()[i], b, sg_point(0,0));
		printf("\twidth:%d height:%d xadvance:%d\n",
				f.character().width, f.character().height, f.character().xadvance);
		b.show();
	}
}

void Util::show_system_font(int idx){
	Assets::init();
	Font * f;

	f = Assets::font(idx);
	if( f != 0 ){
		printf("Show System font %d of %d\n", idx, Assets::font_count());
		show_font(*f);
	} else {
		printf("System font %d does not exist\n", idx);
	}
}
