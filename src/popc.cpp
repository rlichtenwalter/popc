#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <getopt.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <string>
#include <vector>
#include <mlpack/methods/kmeans/kmeans.hpp>
#include "../include/cluster.hpp"
#include "../include/dataset.hpp"
#include "../include/popc.hpp"

std::string VERSION_STRING = "0.2 (beta)";

enum verbosity_level : char {
	QUIET = 0,
	WARNING = 1,
	INFO = 2,
	DEBUG = 3
};

enum message_type : char {
	STANDARD = 0,
	START = 1,
	FINISH = 2
};

char DELIMITER = '\t';
verbosity_level VERBOSITY = WARNING;

void short_usage( char const * program ) {
	std::cerr << "Usage: " << program << " [OPTION]... [FILE]                                     \n";
	std::cerr << "Try '" << program << " --help' for more information.                            \n";
}

void usage( char const * program ) {
	std::cerr << "Usage: " << program << " [OPTION]... [FILE]                                     \n";
	std::cerr << "Generate POPC cluster assignments from input. Input may be taken either from    \n";
	std::cerr << "standard input or from [FILE] if standard input is not provided. Input is read  \n";
	std::cerr << "once as a stream, so named pipes and process substitution may also be used as   \n";
	std::cerr << "[FILE] argument. Input must be tabular data in the form of Boolean (0 or 1)     \n";
	std::cerr << "values separated by tab, or CHAR, if specified. Data must be preceded by a      \n";
	std::cerr << "single-line header naming the columns Output takes the form of a single integer \n";
	std::cerr << "cluster assignment per line, where each line corresponds to the data row of the \n";
	std::cerr << "input.                                                                          \n";
	std::cerr << "                                                                                \n";
	std::cerr << "  -t, --delimiter=CHAR      use CHAR for field separator                        \n";
	std::cerr << "                            defaults to TAB if not provided                     \n";
	std::cerr << "  -c, --clusters=CFILE      use CFILE as the file containing pre-generated      \n";
	std::cerr << "                            k-means cluster assignments; must take the form of  \n";
	std::cerr << "                            a list, one cluster assignment per line, ordered    \n";
	std::cerr << "                            according to the order of the instances             \n";
	std::cerr << "  -m, --multiplier=MULT     multiplying constant C_m from POPC paper            \n";
	std::cerr << "                            defaults to 1000.0 if not provided                  \n";
	std::cerr << "  -p  --power=POW           power constant P from POPC paper                    \n";
	std::cerr << "                            defaults to 10.0 if not provided                    \n";
	std::cerr << "  -v, --verbosity=VALUE     one of {0,1,2,3,quiet,warning,info,debug};          \n";
	std::cerr << "                            defaults to 1=warning if not provided               \n";
	std::cerr << "  -h, --help                display this help and exit                          \n";
	std::cerr << "  -V, --version             output version information and exist                \n";
}

void log_message( char const * message, verbosity_level verbosity, message_type mtype ) {
	using time_type = std::chrono::time_point<std::chrono::high_resolution_clock>;
	static std::stack<time_type,std::list<time_type>> time_stack;
	if( VERBOSITY >= verbosity ) {
		if( mtype == STANDARD && time_stack.size() > 0 ) {
			std::cerr << '\n';
		}
		if( mtype == STANDARD || mtype == START ) {
			std::time_t time = std::time( nullptr );
			std::cerr << std::string( time_stack.size(), '\t' ) << std::put_time( std::localtime( &time ), "%Y-%m-%d %H:%M:%S" ) << " - " << message;
		}
		if( mtype == STANDARD ) {
			std::cerr << '\n';
		} else if( mtype == START ) {
			time_stack.emplace( std::chrono::high_resolution_clock::now() );
		} else if( mtype == FINISH ) {
			if( time_stack.size() > 0 ) {
				auto start_time = time_stack.top();
				std::chrono::duration<double> time_span = std::chrono::duration_cast< std::chrono::duration<double> >( std::chrono::high_resolution_clock::now() - start_time );
				time_stack.pop();
				std::cerr << std::string( time_stack.size(), '\t' ) << "DONE (" << time_span.count() << " seconds)\n";
			} else {
				throw std::logic_error( "attempted to log 'FINISH' message without first logging corresponding 'START' message" );
			}
		}
	}
}

int main( int argc, char* argv[] ) {
	// disable I/O sychronization for better I/O performance
	std::ios_base::sync_with_stdio( false );

	const char * cfile = nullptr;
	std::ifstream ifs;

	int c;
	double m = 1000.0;
	double p = 10.0;
	int option_index = 0;
	while( true ) {
		static struct option long_options[] = {
				{ "delimiter", required_argument, 0, 't' },
				{ "clusters", required_argument, 0, 'c' },
				{ "multiplier", required_argument, 0, 'm' },
				{ "power", required_argument, 0, 'p' },
				{ "verbosity", required_argument, 0, 'v' },
				{ "help", no_argument, 0, 'h' },
				{ "version", no_argument, 0, 'V' }
				};
		c = getopt_long( argc, argv, "t:c:m:p:v:whV", long_options, &option_index );
		if( c == -1 ) {
			break;
		}
		switch( c ) {
			case 't':
				if( strcmp( optarg, "\\t" ) == 0 ) {
					DELIMITER = '\t';
				} else if( strlen( optarg ) != 1 ) {
					std::cerr << argv[0] << ":  -t, --delimiter=CHAR  must be a single character\n";
					return 1;
				} else {
					DELIMITER = optarg[0];
				}
				break;
			case 'c':
				cfile = optarg;
				break;
			case 'm': {
				char * pend;
				errno = 0;
				m = strtod( optarg, &pend );
				if( errno != 0 || *pend != '\0' ) {
					std::cerr << argv[0] << ": " << "  -m, --multiplier=MULT    must be a valid floating point value\n";
				}
				break;
			} case 'p': {
				char * pend;
				errno = 0;
				p = strtod( optarg, &pend );
				if( errno != 0 || *pend != '\0' ) {
					std::cerr << argv[0] << ": " << "  -p, --power=POW          must be a valid floating point value\n";
				}
				break;
			} case 'v':
				if( strcmp( optarg, "0" ) == 0 || strcmp( optarg, "quiet" ) == 0 ) {
					VERBOSITY = QUIET;
				} else if( strcmp( optarg, "1" ) == 0 || strcmp( optarg, "warning" ) == 0 ) {
					VERBOSITY = WARNING;
				} else if( strcmp( optarg, "1" ) == 0 || strcmp( optarg, "info" ) == 0 ) {
					VERBOSITY = INFO;
				} else if( strcmp( optarg, "2" ) == 0 || strcmp( optarg, "debug" ) == 0 ) {
					VERBOSITY = DEBUG;
				} else {
					std::cerr << argv[0] << ": " << "  -v, --verbosity=[VALUE]  one of {0,1,2,3,quiet,warning,info,debug}; defaults to 1=warning\n";
					short_usage( argv[0] );
					return 1;
				}
				break;
			case 'h':
				usage( argv[0] );
				return 0;
			case 'V':
				std::cout << "POPC C++ by Ryan N. Lichtenwalter v" << VERSION_STRING << "\n";
				return 0;
			default:
				short_usage( argv[0] );
				return 1;
		}
	}
	if( optind < argc ) {
		if( optind == argc - 1 ) {
			ifs = std::ifstream( argv[optind] );
			log_message( (std::string( "FILE = " ) + std::string( argv[optind] )).c_str(), DEBUG, STANDARD );
		} else {
			std::cerr << argv[0] << ": " << "too many arguments\n";
			short_usage( argv[0] );
			return 1;
		}
	}

	using dataset = popc::dataset;

	// read data
	log_message( "Reading data...", INFO, START );
	dataset data;
	if( ifs.is_open() ) {
		log_message( "Reading from file...", DEBUG, STANDARD );
		while( ifs.good() ) {
			data = popc::dataset( ifs, DELIMITER );
		}
		ifs.close();
	} else {
		log_message( "Reading from standard input...", DEBUG, STANDARD );
		while( std::cin.good() ) {
			data = popc::dataset( std::cin, DELIMITER );
			std::cin >> std::ws;
		}
	}
	log_message( "DONE", INFO, FINISH );

	std::size_t initial_num_clusters = data.num_instances() / 2;
	std::vector<std::size_t> assignments_vec( data.num_instances() );
	if( cfile != nullptr ) {
		// read k-means cluster assignments, if provided
		log_message( "Reading clustering assignments...", INFO, START );
		auto cluster_ifs = std::ifstream( cfile );
		std::size_t instance_num = 0;
		std::size_t cluster_num;
		while( cluster_ifs >> cluster_num ) {
			if( instance_num >= data.num_instances() ) {
				log_message( "error: too many lines in cluster file for the number of instances read in data file", QUIET, STANDARD );
				return 2;
			}
			if( cluster_num >= initial_num_clusters ) {
				log_message( "error: cluster identifier exceeds permitted number of clusters given number of instances in data file", QUIET, STANDARD );
				return 2;
			}
			if( cluster_ifs.get() != '\n' ) {
				log_message( "error: unexpected character in cluster file", QUIET, STANDARD );
				return 2;
			}
			assignments_vec[ instance_num++ ] = cluster_num;
		}
		cluster_ifs.close();
		log_message( "DONE", INFO, FINISH );
	} else {
		// otherwise generate initial k-means clusters
		log_message( "Performing k-means...", INFO, START );
		using Matrix = arma::Mat<double>;
		using Row = arma::Row<std::size_t>;
		using KMeans = mlpack::kmeans::KMeans<
				mlpack::metric::EuclideanDistance,
				mlpack::kmeans::SampleInitialization,
				mlpack::kmeans::MaxVarianceNewCluster,
				mlpack::kmeans::NaiveKMeans,
				Matrix>;
	
		KMeans clusterer;
		Matrix armadata( data.num_attributes(), data.num_instances() );
		Row assignments( data.num_instances() );
		for( dataset::size_type instance_num = 0; instance_num < data.num_instances(); ++instance_num ) {
			for( dataset::size_type attribute_num = 0; attribute_num < data.num_attributes(); ++attribute_num ) {
				// transpose is being performed here for Armadillo's exposed column-major storage
				armadata.at( attribute_num, instance_num ) = data( instance_num, attribute_num );
			}
		}

		clusterer.Cluster( armadata, initial_num_clusters, assignments, false );

		// moving cluster assignments into vector for unified processing later
		// negligible expense in time and space compared to other operations
		for( std::size_t instance_num = 0; instance_num < assignments.n_elem; ++instance_num ) {
			assignments_vec[ instance_num ] = assignments[ instance_num ];
		}
		log_message( "DONE", INFO, FINISH );
	}
	
	// process cluster assignments for easier usage
	log_message( "Processing cluster assignments...", INFO, START );
	std::vector<popc::cluster> clusters( initial_num_clusters, popc::cluster( data.num_attributes() ) );
	for( std::size_t instance_num = 0; instance_num < assignments_vec.size(); ++instance_num ) {
		auto cluster_num = assignments_vec[ instance_num ];
		auto & cluster = clusters[ cluster_num ];
		cluster.add_instance( instance_num );
		for( std::size_t attribute_num = 0; attribute_num < data.num_attributes(); ++attribute_num ) {
			if( data( instance_num, attribute_num ) ) {
				cluster.increment_attribute_count( attribute_num );
			}
		}
	}
	log_message( "DONE", INFO, FINISH );

	// refine cluster
	log_message( "Executing POPC algorithm...", INFO, START );
	std::list<popc::cluster> clusters_list;
	std::move( clusters.begin(), clusters.end(), std::back_inserter( clusters_list ) );
	auto result = popc::popc( data, clusters_list, m, p );
	log_message( "DONE", INFO, FINISH );
	
	// output results
	log_message( "Outputting results...", INFO, START );
	for( auto val : result ) {
		std::cout << val << "\n";
	}
	log_message( "DONE", INFO, FINISH );

	return 0;
}
