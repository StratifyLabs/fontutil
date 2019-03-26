/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved

#include <sapi/fmt.hpp>
#include <sapi/hal.hpp>
#include <sapi/sys.hpp>
#include <sapi/var.hpp>
#include <sapi/sgfx.hpp>

#include "Util.hpp"
#include "BmpFontManager.hpp"
#include "SvgFontManager.hpp"
#include "ApplicationPrinter.hpp"

void show_usage(const Cli & cli);

int main(int argc, char * argv[]){
	String path;
	String bpp;
	String map;
	bool overwrite;
	int verbose;

	Cli cli(argc, argv);
	cli.set_publisher("Stratify Labs, Inc");
	cli.handle_version();

	Ap::printer().set_verbose_level( cli.get_option("verbose") );

	path = cli.get_option("show");
	if( path.is_empty() == false ){
		if( path == "true" ){
			Ap::printer().error("use --show=<path>");
			exit(0);
		}

		bool is_details = cli.get_option("details") == "true";

		Util::show_file_font(path, is_details);
		exit(0);
	}

	bpp = cli.get_option("bpp");
	if( bpp.is_empty() ){
		bpp = "1";
	} else if( bpp == "true" ){
		Ap::printer().error("use --bpp=[1,2,4,8]");
		exit(0);
	}

	map = cli.get_option("map");

	switch(bpp.to_integer()){
		case 1:
		case 2:
		case 4:
		case 8:
			break;
		default:
			Ap::printer().error("use --bpp=[1,2,4,8]");
			exit(0);
	}


	if( cli.get_option("show_icon") == "true" ){
		path = cli.get_option_argument("-show_icon");
		Ap::printer().message("Show Font %s", path.cstring());
		sg_size_t canvas_size = cli.get_option_value("-canvas_size");
		if( canvas_size == 0 ){
			canvas_size = 128;
		}
		Util::show_icon_file(path, canvas_size);
		exit(0);
	}


	if( cli.get_option("clean") == "true" ){
		path = cli.get_option_argument("-clean");
		Ap::printer().message("Cleaning directory %s from sbf files", path.cstring());
		Util::clean_path(path, "sbf");
		exit(0);
	}


	path = cli.get_option("input");
	if( path.is_empty() ){
		path = "/home";
	}

	if( cli.get_option("overwrite") == "true" ){
		overwrite = true;
	} else {
		overwrite = false;
	}

	verbose = cli.get_option("verbose").to_integer();

	path = cli.get_option("convert-bmp");
	if( path.is_empty() == false ){
		if( path == "true" ){
			printf("use --convert-bmp=<name>");
			exit(1);
		}
		BmpFontManager bmp_font_manager;
		bmp_font_manager.set_bits_per_pixel(bpp.to_integer());
		if( map == "true" ){
			bmp_font_manager.set_generate_map(true);
		}
		bmp_font_manager.convert_font(path);
		exit(0);
	}

	if( cli.is_option("-svg") ){
		SvgFontManager svg_font;
		Area downsample(1,1);

		if( cli.is_option("-downsample") ){
			downsample.set_width( cli.get_option_value("-downsample") );
			downsample.set_height( downsample.width() );
		}

		if( cli.is_option("-downsample_width") ){
			downsample.set_width( cli.get_option_value("-downsample_width") );
		}

		if( cli.is_option("-downsample_height") ){
			downsample.set_height( cli.get_option_value("-downsample_height") );
		}

		if( cli.is_option("-map") ){
			svg_font.set_map_output_file( cli.get_option_argument("-map") );
		}

		svg_font.set_downsample_factor(downsample);
		svg_font.set_canvas_size( cli.get_option_value("-canvas_size") );
		svg_font.set_pour_grid_size( cli.get_option_value("-pour_grid_size"));
		if( cli.is_option("-character_set") ){
			svg_font.set_character_set( cli.get_option_argument("-character_set"));
		}

		svg_font.set_flip_y(true);

		if( cli.is_option("-all") ){
			svg_font.set_character_set("");
		}
		svg_font.process_svg_font_file(path);
	} else if( cli.get_option("icon") == "true"){
		SvgFontManager svg_font;
		svg_font.set_pour_grid_size( cli.get_option_value("-pour_grid_size"));

		svg_font.set_flip_y(false);

		svg_font.set_canvas_size( cli.get_option("canvas_size").to_integer() );

		svg_font.process_svg_icon_file(path, "icons.svic");

	} else if( cli.get_option("from_map") == "true" ){

		if( map.is_empty() ){
			Ap::printer().error("specify map file with --map=<map> '-map.txt' is added automatically");
			return 1;
		}

		BmpFontGenerator bmp_font_generator;
		if( bmp_font_generator.import_map(map) == 0 ){
			if( bmp_font_generator.generate_font_file(map) < 0 ){
				return 1;
			}
		}
	}

	return 0;
}

void show_usage(const Cli & cli){

}

