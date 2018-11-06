#ifndef APPLICATIONPRINTER_HPP
#define APPLICATIONPRINTER_HPP

#include <sapi/sys.hpp>

class ApplicationPrinter {
public:
	static Printer & printer(){ return m_printer; }

private:
	static Printer m_printer;
};

typedef ApplicationPrinter Ap;

#endif // APPLICATIONPRINTER_HPP
