#ifndef POPC_DATASET_HPP
#define POPC_DATASET_HPP

#include <iostream>
#include <locale>
#include <string>
#include <vector>
#include "delimiter_ctype.hpp"

namespace popc {
	class dataset {
		friend std::ostream & operator<<( std::ostream & os, dataset const & ds );
		public:
			using value_type = bool;
		private:
			using storage_type = std::vector<value_type>;
		public:
			using size_type = std::size_t;
			using const_iterator = storage_type::const_iterator;
			dataset() : _names(), _data(), _num_instances(), _positive_counts() {}
			dataset( std::istream & is, char delimiter = '\t' );
			size_type num_instances() const;
			size_type num_attributes() const;
			value_type operator()( size_type instance_num, size_type attribute_num ) const;
			std::string attribute_name( size_type attribute_num ) const;
			size_type positive_count( size_type attribute_num ) const;
			const_iterator cbegin( size_type instance_num ) const;
			const_iterator cend( size_type instance_num ) const;
		private:
			std::vector<std::string> _names;
			storage_type _data;
			size_type _num_instances;
			std::vector<size_type> _positive_counts;
	};

	dataset::dataset( std::istream & is, char delimiter ) : _num_instances() {
		// the pointer below is managed via the library interface
		is.imbue( std::locale( is.getloc(), new delimiter_ctype( delimiter ) ) );

		// read header line with attribute names
		std::string name;
		while( is.good() && is.peek() != '\n' ) {
			is >> name;
			_names.push_back( name );
		}
		if( is.get() != '\n' ) {
			std::cerr << "error: missing required newline after header\n";
			exit( 2 );
		}

		// read data matrix
		_data.reserve( 256 * num_attributes() );
		_positive_counts.resize( num_attributes() );
		size_type instance_num = 0;
		size_type attribute_num = 0;
		while( !is.eof() ) {
			if( attribute_num == 0 ) {
				++_num_instances;
			}
			char c = is.get();
			if( c == '0' ) {
				_data.push_back( 0 );
			} else if( c == '1' ) {
				_data.push_back( 1 );
				++_positive_counts[ attribute_num ];
			} else if( c == delimiter ) {
				std::cerr << "error: unexpected delimiter detected at line " << instance_num + 2 << "\n";
				exit( 2 );
			} else if( c == '\n' ) {
				std::cerr << "error: newline detected after delimiter at line " << instance_num + 2 << "\n";
				exit( 2 );
			} else {
				std::cerr << "error: invalid character for attribute value at line " << instance_num + 2 << " for column " << attribute_num + 1 << " - must be 0 or 1\n";
				exit( 2 );
			}
			c = is.get();
			if( c == delimiter ) {
				++attribute_num;
			} else if( c == '\n' ) {
				if( attribute_num + 1 == num_attributes() ) {
					attribute_num = 0;
				} else {
					std::cerr << "error: inconsistent number of columns on line " << instance_num + 2 << "\n";
					exit( 2 );
				}
			} else {
				std::cerr << "error: invalid character '" << c << "' at line " << instance_num << "\n";
				exit( 2 );
			}
			is.peek();
		}
		_data.shrink_to_fit();
	}

	dataset::size_type dataset::num_instances() const {
		return _num_instances;
	}

	dataset::size_type dataset::num_attributes() const {
		return _names.size();
	}

	dataset::value_type dataset::operator()( size_type instance_num, size_type attribute_num ) const {
		return _data[ instance_num * num_attributes() + attribute_num ];
	}

	std::string dataset::attribute_name( size_type attribute_num ) const {
		return _names[ attribute_num ];
	}

	dataset::size_type dataset::positive_count( size_type attribute_num ) const {
		return _positive_counts[ attribute_num ];
	}

	std::ostream & operator<<( std::ostream & os, dataset const & ds ) {
		dataset::size_type attribute_num = 0;
		for( auto const & name : ds._names ) {
			os << name;
			++attribute_num;
			if( attribute_num == ds.num_attributes() ) {
				os << '\n';
				attribute_num = 0;
			} else {
				os << '\t';
			}
		}
		for( auto val : ds._data ) {
			os << val;
			++attribute_num;
			if( attribute_num == ds.num_attributes() ) {
				os << '\n';
				attribute_num = 0;
			} else {
				os << '\t';
			}
		}
		return os;
	}

	dataset::const_iterator dataset::cbegin( size_type instance_num ) const {
		return _data.begin() + instance_num * num_attributes();
	}

	dataset::const_iterator dataset::cend( size_type instance_num ) const {
		return _data.begin() + (instance_num + 1) * num_attributes();
	}

}

#endif
