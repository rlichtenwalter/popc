#ifndef POPC_CLUSTER_HPP
#define POPC_CLUSTER_HPP

#include <iostream>
#include <vector>

namespace popc {
	class cluster {
		private:
			using storage_type = std::list<std::size_t>;
		public:
			using const_iterator = storage_type::const_iterator;
			using iterator = storage_type::iterator;
			cluster() = delete;
			cluster( std::size_t num_attributes ) : _members(), _attribute_counts( num_attributes, 0 ) {}
			bool empty() const;
			std::size_t num_instances() const;
			void add_instance( std::size_t instance_num );
			iterator remove_instance( iterator it );
			void increment_attribute_count( std::size_t attribute_num );
			void decrement_attribute_count( std::size_t attribute_num );
			std::size_t attribute_count( std::size_t attribute_num ) const;
			iterator begin();
			iterator end();
			const_iterator cbegin() const;
			const_iterator cend() const;
		private:
			storage_type _members;
			std::vector<std::size_t> _attribute_counts;
	};

	bool cluster::empty() const {
		return _members.empty();
	}

	std::size_t cluster::num_instances() const {
		return _members.size();
	}

	void cluster::add_instance( std::size_t instance_num ) {
		_members.push_back( instance_num );
	}

	cluster::iterator cluster::remove_instance( iterator it ) {
		return _members.erase( it );
	}

	void cluster::increment_attribute_count( std::size_t attribute_num ) {
		++_attribute_counts[ attribute_num ];
	}

	void cluster::decrement_attribute_count( std::size_t attribute_num ) {
		--_attribute_counts[ attribute_num ];
	}
	
	std::size_t cluster::attribute_count( std::size_t attribute_num ) const {
		return _attribute_counts[ attribute_num ];
	}

	cluster::iterator cluster::begin() {
		return _members.begin();
	}
	
	cluster::iterator cluster::end() {
		return _members.end();
	}

	cluster::const_iterator cluster::cbegin() const {
		return _members.cbegin();
	}

	cluster::const_iterator cluster::cend() const {
		return _members.cend();
	}
/*
	std::ostream & operator<<( std::ostream & os, cluster const & c ) {
		os << "Members: ";
		std::copy( _members.begin(), members.end(), std::ostream_iterator<std::size_t>( os, "," ) );
		os << "\n";
		os << "Attribute counts: ";
		std::copy( _attribute_counts.begin(), attribute_counts.end(); std::ostream_inserter<std::size_t>( os, "," ) );
		os << "\n";
	}
*/
}
#endif
