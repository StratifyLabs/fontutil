
#include <sapi/fmt.hpp>
#include <sapi/hal.hpp>
#include <sapi/sys.hpp>
#include <sapi/var.hpp>
#include <sapi/sgfx.hpp>

#include "Util.hpp"
#include "BmpFont.hpp"
#include "SvgFont.hpp"


int main(int argc, char * argv[]){
	String path;

	Cli cli(argc, argv);
	cli.set_publisher("Stratify Labs, Inc");
	cli.handle_version();

	if( cli.is_option("-show") ){
		path = cli.get_option_argument("-show");
		printf("Show Font %s\n", path.c_str());
		Util::show_file_font(path);
		exit(0);
	}

	if( cli.is_option("-clean") ){
		path = cli.get_option_argument("-clean");
		printf("Cleaning directory %s from sbf files\n", path.c_str());
		Util::clean_path(path, "sbf");
		exit(0);
	}

	if( cli.is_option("-system") ){
		printf("Show System font %d\n", cli.get_option_value("-system"));
		Util::show_system_font( cli.get_option_value("-system") );
		exit(0);
	}

	if( cli.is_option("-i") ){
		path = cli.get_option_argument("-i");
	} else {
		path = "/home";
	}

	if( cli.is_option("-bmp") ){
		BmpFont::convert_directory(path.c_str());
	} else if( cli.is_option("-svg") ){
		SvgFont svg_font;
		svg_font.convert_file(path.c_str());
	}

	return 0;
}

