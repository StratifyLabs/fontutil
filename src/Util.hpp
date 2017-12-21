/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <sapi/sys.hpp>
#include <sapi/sgfx.hpp>
#include <sapi/var.hpp>

class Util {
public:


	static void show_file_font(const char * path);
	static void show_font(Font & f);
	static void show_system_font(int idx);
	static void clean_path(const char * path, const char * suffix);

};

#endif /* UTIL_HPP_ */
