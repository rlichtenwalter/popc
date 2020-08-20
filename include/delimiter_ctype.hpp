#ifndef POPC_DELIMITER_CTYPE_HPP
#define POPC_DELIMITER_CTYPE_HPP

#include <locale>
#include <string>
#include <vector>

class delimiter_ctype : public std::ctype<char> {
	public: 
		delimiter_ctype( char delimiter, std::size_t refs = 0 );
		delimiter_ctype( std::string delimiters, std::size_t refs = 0 );
	private: 
		static mask const * make_table( std::string delimiters );
};

std::ctype<char>::mask const * delimiter_ctype::make_table( std::string delimiters ) {
	static std::vector<mask> stream_table( classic_table(), classic_table() + table_size );
	for( auto m : stream_table ) {
		m &= ~space;
	}
	for( auto delimiter : delimiters ) {
		stream_table[ delimiter ] |= space;
	}
	return &stream_table[0];
}

delimiter_ctype::delimiter_ctype( char delimiter, std::size_t refs ) : ctype( make_table( std::string( 1, delimiter ) ), false, refs ) {
}

delimiter_ctype::delimiter_ctype( std::string delimiters, std::size_t refs ) : ctype( make_table( delimiters ), false, refs ) {
}

#endif
