/*! \file */ //Copyright 2011-2018 Tyler Gilbert; All Rights Reserved


#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <sapi/sys.hpp>
#include <sapi/sgfx.hpp>
#include <sapi/var.hpp>
#include "ApplicationPrinter.hpp"

class Util : public ApplicationPrinter {
public:

	static void show_icon_file(const ConstString & path, sg_size_t canvas_size);
	static void show_file_font(const ConstString & path, bool is_details = false);
	static void show_font(Font & f);
	static void show_system_font(int idx);
	static void clean_path(const ConstString & path, const ConstString & suffix);

};

#endif /* UTIL_HPP_ */
