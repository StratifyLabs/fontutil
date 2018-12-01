/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved

#include <sapi/fmt.hpp>
#include <sapi/hal.hpp>
#include <sapi/sys.hpp>
#include <sapi/var.hpp>
#include <sapi/sgfx.hpp>

#include "Util.hpp"
#include "BmpFontManager.hpp"
#include "SvgFontManager.hpp"

void show_usage(const Cli & cli);

int main(int argc, char * argv[]){
	String path;
	bool overwrite;
	int verbose;

	Cli cli(argc, argv);
	cli.set_publisher("Stratify Labs, Inc");
	cli.handle_version();

	if( cli.is_option("-show") ){
		path = cli.get_option_argument("-show");
		Ap::printer().message("Show Font %s", path.cstring());
		Util::show_file_font(path);
		exit(0);
	}

	if( cli.is_option("-clean") ){
		path = cli.get_option_argument("-clean");
		Ap::printer().message("Cleaning directory %s from sbf files", path.cstring());
		Util::clean_path(path, "sbf");
		exit(0);
	}

	if( cli.is_option("-system") ){
		Ap::printer().message("Show System font %d\n", cli.get_option_value("-system"));
		Util::show_system_font( cli.get_option_value("-system") );
		exit(0);
	}

	if( cli.is_option("-i") ){
		path = cli.get_option_argument("-i");
	} else {
		path = "/home";
	}

	if( cli.is_option("-overwrite") ){
		overwrite = true;
	} else {
		overwrite = false;
	}

	if( cli.is_option("-verbose") ){
		verbose = cli.get_option_value("-verbose");
	} else {
		verbose = 0;
	}

	if( cli.is_option("-bmp") ){
		BmpFontManager bmp_font;
		bmp_font.convert_directory(path, overwrite, verbose);
	} else if( cli.is_option("-svg") ){
		SvgFontManager svg_font;
		Dim downsample(1,1);

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

		svg_font.set_downsample_factor(downsample);
		svg_font.set_canvas_size( cli.get_option_value("-canvas_size") );
		svg_font.set_pour_grid_size( cli.get_option_value("-pour_grid_size"));
		if( cli.is_option("-character_set") ){
			svg_font.set_character_set( cli.get_option_argument("-character_set"));
		}

		if( cli.is_option("-all") ){
			svg_font.set_character_set("");
		}
		svg_font.process_svg_font_file(path);
	} else if( cli.is_option("-icon") ){
		SvgFontManager svg_font;
		svg_font.set_pour_grid_size( cli.get_option_value("-pour_grid_size"));

		svg_font.set_canvas_size( cli.get_option_value("-canvas_size") );
		svg_font.process_svg_icon_file(path);
	}

	return 0;
}

void show_usage(const Cli & cli){

}

