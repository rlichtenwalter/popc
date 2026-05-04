#ifndef POPC_POPC
#define POPC_POPC

#include "cluster.hpp"
#include "dataset.hpp"

namespace popc {

	template <typename fptype = double>
	fptype compute_delta( popc::dataset const & ds, popc::cluster const & cluster, std::size_t instance_num, std::size_t num_clusters, fptype multiplier, fptype power, bool added ) {
		fptype ret = 0;

		std::size_t attribute_num = 0;
		for( auto it = ds.cbegin( instance_num ); it != ds.cend( instance_num ); ++it ) {
			auto val = *it;
			if( val ) {
				auto counts = cluster.attribute_count( attribute_num );
				auto counts_all = ds.positive_count( attribute_num );
				auto denom = static_cast<fptype>( counts_all * multiplier + num_clusters );
				ret -= std::pow( (counts * multiplier + 1) / denom, power );
				ret += std::pow( ((counts + (added ? 1 : -1)) * multiplier + 1) / denom, power );
			}
			++attribute_num;
		}
		return ret;
	}
	
	template <typename fptype = double>
	std::vector<std::size_t> popc( popc::dataset const & ds, std::list<popc::cluster> & clusters, fptype multiplier = static_cast<fptype>( 1000 ), fptype power = static_cast<fptype>( 10 ) ) {
		bool changed = true;
		while( changed ) {
			changed = false;
			for( auto cluster_it = clusters.begin(); cluster_it != clusters.end(); ) {
				auto & cluster = *cluster_it;
				for( auto instance_it = cluster.begin(); instance_it != cluster.end(); ) {
					auto instance_num = *instance_it;
					auto largest_gain = -std::numeric_limits<fptype>::infinity();
					popc::cluster * to_cluster = nullptr;
					auto delta_base = compute_delta( ds, cluster, instance_num, clusters.size(), multiplier, power, false );
					for( auto & second_cluster : clusters ) {
						if( &cluster != &second_cluster ) {
							auto delta = delta_base + compute_delta( ds, second_cluster, instance_num, clusters.size(), multiplier, power, true );
							if( delta > largest_gain ) {
								largest_gain = delta;
								to_cluster = &second_cluster;
							}
						}
					}
					if( largest_gain > 0 ) {
						changed = true;
						instance_it = cluster.remove_instance( instance_it );
						to_cluster->add_instance( instance_num );
						std::size_t attribute_num = 0;
						for( auto val_it = ds.cbegin( instance_num ); val_it != ds.cend( instance_num ); ++val_it ) {
							auto val = *val_it;
							if( val ) {
								cluster.decrement_attribute_count( attribute_num );
								to_cluster->increment_attribute_count( attribute_num );
							}
							++attribute_num;
						}
					} else {
						++instance_it;
					}
				}
				if( cluster.empty() ) {
					cluster_it = clusters.erase( cluster_it );
				} else {
					++cluster_it;
				}
			}
		}
		std::vector<std::size_t> labels( ds.num_instances() );
		std::size_t cluster_index = 0;
		for( auto const & cluster : clusters ) {
			for( auto it = cluster.cbegin(); it != cluster.cend(); ++it ) {
				auto instance_num = *it;
				labels[ instance_num ] = cluster_index;
			}
			++cluster_index;
		}

		return labels;
	}
}
#endif
