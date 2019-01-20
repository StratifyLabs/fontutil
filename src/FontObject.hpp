#ifndef FONTOBJECT_HPP
#define FONTOBJECT_HPP

#include <sapi/var.hpp>
#include <sapi/sys.hpp>
#include <sapi/fmt.hpp>
#include <sapi/sgfx.hpp>

#include "ApplicationPrinter.hpp"

class FontObject : public ApplicationPrinter {
public:
	FontObject();

	void set_generate_map(bool value = true){
		m_is_generate_map = value;
	}
	bool is_generate_map() const { return m_is_generate_map; }

	void set_bits_per_pixel(u8 bits_per_pixel){
		m_bits_per_pixel = bits_per_pixel;
	}

	void set_ascii(bool value = true){
		m_is_ascii = value;
	}

	bool is_ascii() const { return m_is_ascii; }

	void set_character_set(const ConstString & character_set){
		m_character_set = character_set;
		set_ascii(false);
	}

	u8 bits_per_pixel() const { return m_bits_per_pixel; }

	const String & character_set() const { return m_character_set; }

private:
	u8 m_bits_per_pixel;
	bool m_is_generate_map;
	bool m_is_ascii;

	String m_character_set;

};

#endif // FONTOBJECT_HPP
