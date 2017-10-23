
#include <sapi/fmt.hpp>
#include <sapi/hal.hpp>
#include <sapi/sys.hpp>
#include <sapi/var.hpp>
#include <sapi/sgfx.hpp>

typedef struct {
	u16 num_chars;
	u16 num_kernings;
	u16 size;
} bmpfont_hdr_t;

typedef struct {
	u16 id;
	u16 x;
	u16 y;
	u16 width;
	u16 height;
	s16 xoffset;
	s16 yoffset;
	u16 xadvance;
	u16 page;
	u16 channel;
} bmpfont_char_t;

typedef struct {
	u16 first;
	u16 second;
	s16 amount;
} bmpfont_kerning_t;

static int load_char(bmpfont_char_t & c, const Token & t);
static int load_kerning(bmpfont_kerning_t & c, const Token & t);
static int load_info(bmpfont_hdr_t & hdr, const Token & t);
static void show_char(Bmp & bmp, bmpfont_char_t c);

static int get_max(File & def, int & w, int &h);
static int get_kerning_count(File & def);
static int get_char(File & def, bmpfont_char_t & d, uint8_t ascii);
static int get_kerning(File & def, bmpfont_kerning_t & k);
static int scan_char(File & def, bmpfont_char_t & d);
static int get_bitmap(Bmp & bmp, bmpfont_char_t c, Bitmap * master);

static void show_file_font(const char * path);
static void show_font(Font & f);
static void show_system_font(int idx);
static void clean_path(const char * path, const char * suffix);

static int gen_fonts(const char * def_file,
		const char * bitmap_file,
		const char * font_file,
		const char * charset = 0);

int main(int argc, char * argv[]){
	Dir dir;
	Dir dir_size;
	String path;
	String font;
	String font_bmp;
	String font_sbf;
	const char * entry;

	Cli cli(argc, argv);

	if( cli.is_option("-show") ){
		path = cli.get_option_argument("-show");
		printf("Show Font %s\n", path.c_str());
		show_file_font(path);
		exit(0);
	}

	if( cli.is_option("-clean") ){
		path = cli.get_option_argument("-clean");
		printf("Cleaning directory %s from sbf files\n", path.c_str());
		clean_path(path, "sbf");
		exit(0);
	}

	if( cli.is_option("-system") ){
		printf("Show System font %d\n", cli.get_option_value("-system"));
		show_system_font( cli.get_option_value("-system") );
		exit(0);
	}

	if( cli.is_option("-i") ){
		path = cli.get_option_argument("-i");
	} else {
		path = "/home";
	}

	printf("Open /home/font directory\n");
	if( dir.open(path.c_str()) < 0 ){
		perror("couldn't open path");
		exit(1);
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

			if( access(font_sbf.c_str(), R_OK ) == 0 ){
				printf("already exists\n");
			} else {
				printf("converting\n");
				gen_fonts(font, font_bmp, font_sbf, Font::charset());
			}
		}
	}

	dir.close();

	return 0;
}

int gen_fonts(const char * def_file, const char * bitmap_file, const char * font_file, const char * charset){
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

	unsigned int i;
	int offset;
	int ret;

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


	if( charset != 0 ){
		header.num_chars = Font::CHARSET_SIZE;
	} else {
		//count the characters -- any chars with ID over 255
		header.num_chars = 0;
		def.seek(0);
		while( scan_char(def, ch) == 0 ){
			printf("Scanned id %d\n", ch.id);
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

	printf("Header size is %ld\n", header.size);

	font.write(&header, sizeof(header));

	bmpfont_kerning_t kerning;
	def.seek(0);
	while(get_kerning(def, kerning) == 0 ){
		//write kerning to file
		printf("Kerning %d -> %d = %d\n", kerning.first, kerning.second, kerning.amount);
		sg_font_kerning_pair_t pair;
		pair.first = kerning.first;
		pair.second = kerning.second;
		pair.kerning = kerning.amount;
		font.write(&pair, sizeof(pair));
	}

	offset = header.size;

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
			character.id = ch.id;
			character.resd = 0;
			character.height = ch.height;
			character.width = ch.width;
			character.offset = offset;
			character.yoffset = ch.yoffset;
			character.xoffset = ch.xoffset;
			character.xadvance = ch.xadvance;
			printf("Char %d is %d x %d at %d advance %d index:%d\n",
					ch.id,
					character.width,
					character.height,
					offset,
					character.xadvance,
					font.seek(0, File::CURRENT));
			if( font.write(&character, sizeof(character)) != sizeof(character) ){
				perror("failed to write char entry");
			}
			offset += Bitmap::calc_size(character.width, character.height);
		}

	}

	def.seek(0);
	Bitmap bitmap(max_width, max_height);

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
			printf("Load Bitmap at %d, %d -- %d x %d for %d write to offset %d\n",
					ch.x, ch.y, ch.width, ch.height, ch.id, character.offset);

			Bitmap b((sg_bmap_data_t*)bitmap.data(), ch.width, ch.height);
			get_bitmap(bmp, ch, &b);
			b.show();

			font.write(character.offset, b.data(), b.calc_size());
		}
	}


	font.close();
	def.close();
	bmp.close();
	return 0;

}


int load_info(bmpfont_hdr_t & hdr, const Token & t){
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

int load_kerning(bmpfont_kerning_t & c, const Token & t){
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

int get_bitmap(Bmp & bmp, bmpfont_char_t c, Bitmap * master){
	unsigned int i, j;
	int x = c.x;
	int y = c.y;
	int bytes = bmp.bits_per_pixel()/8;
	int offset = x*bytes;
	unsigned int width = c.width;
	unsigned int height = c.height;
	int avg;
	uint8_t pixel[3];

	master->clear();

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
				master->draw_pixel(sg_point(i,j));
			}
		}

		//printf(" %02u\n", j);
	}
	//printf("\n");
	return 0;
}

static void show_char(Bmp & bmp, bmpfont_char_t c){
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

int load_char(bmpfont_char_t & c, const Token & t){
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

int get_max(File & def, int & w, int &h){
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

int get_kerning(File & def, bmpfont_kerning_t & k){
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

int get_char(File & def, bmpfont_char_t & d, uint8_t ascii){
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

int get_kerning_count(File & def){
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

int scan_char(File & def, bmpfont_char_t & d){
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

void show_system_font(int idx){
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

void show_file_font(const char * path){
	FontFile ff;

	printf("Show font: %s\n", path);

	if( ff.set_file(path) < 0 ){
		printf("Failed to open font: '%s'\n", path);
		perror("Open failed");
		return;
	}

	show_font(ff);
}

void show_font(Font & f){
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

void clean_path(const char * path, const char * suffix){
	Dir d;
	const char * entry;
	String str;

	if( d.open(path) < 0 ){
		printf("Failed to open path: '%s'\n", path);
	}

	while( (entry = d.read()) != 0 ){
		str = File::suffix(entry);
		if( str == "sbf" ){
			str.clear();
			str << path << "/" << entry;
			printf("Removing file: %s\n", str.c_str());
			File::remove(str);
		}
	}
}
