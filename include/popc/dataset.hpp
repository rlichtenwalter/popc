#ifndef POPC_DATASET_HPP
#define POPC_DATASET_HPP

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
			dataset( storage_type data, size_type num_instances, size_type num_attributes, std::vector<std::string> names = std::vector<std::string>() );
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
		// read header line with attribute names
		std::string name;
		while( !is.eof() ) {
			char c = is.get();
			if( c == delimiter ) {
				_names.emplace_back( name );
				name.clear();
			} else if( c == '\n' ) {
				_names.emplace_back( name );
				break;
			} else {
				name.push_back( c );
			}
			is.peek();
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


	dataset::dataset( storage_type data, size_type num_instances, size_type num_attributes, std::vector<std::string> names ) : _names( std::move( names ) ), _data( std::move( data ) ), _num_instances( num_instances ), _positive_counts( num_attributes, 0 ) {
		if( num_instances * num_attributes != _data.size() ) {
			throw std::logic_error( "data size must equal the product of num_instances and num_attributes" );
		}
		if( _names.empty() ) {
			for( size_type i = 0; i < num_attributes; ++i ) {
				_names.emplace_back( "attr" + std::to_string( i + 1 ) );
			}
		} else if( num_attributes != _names.size() ) {
			throw std::logic_error( "names size must either equal num_attributes or be zero" );
		}
		for( size_type instance_num = 0; instance_num < num_instances; ++instance_num ) {
			for( size_type attribute_num = 0; attribute_num < num_attributes; ++attribute_num ) {
				if( (*this)( instance_num, attribute_num ) ) {
					++_positive_counts[ attribute_num ];
				}
			}
		}
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
