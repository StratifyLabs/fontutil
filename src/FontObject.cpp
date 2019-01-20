#include "FontObject.hpp"

FontObject::FontObject(){
	m_bits_per_pixel = 1;
	m_is_generate_map = false;
	m_character_set = Font::ascii_character_set();
	m_is_ascii = true;
}
