#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#include <getopt.h>

#include <popc/cluster.hpp>
#include <popc/dataset.hpp>
#include <popc/detail/bitpacked_kmeans.hpp>
#include <popc/popc.hpp>

#ifndef POPC_VERSION
#define POPC_VERSION "unknown"
#endif

namespace {

enum verbosity_level : char {
  QUIET = 0,
  WARNING = 1,
  INFO = 2,
  DEBUG = 3,
};

enum message_type : char {
  STANDARD = 0,
  START = 1,
  FINISH = 2,
};

char DELIMITER = '\t';
verbosity_level VERBOSITY = WARNING;

void short_usage(char const *program) {
  std::cerr << "Usage: " << program << " [OPTION]... [FILE]\n"
            << "Try '" << program << " --help' for more information.\n";
}

void usage(char const *program) {
  std::cerr
      << "Usage: " << program << " [OPTION]... [FILE]\n"
      << "Generate POPC cluster assignments from input. Input may be taken either from\n"
      << "standard input or from [FILE] if standard input is not provided. Input is read\n"
      << "once as a stream, so named pipes and process substitution may also be used as\n"
      << "the [FILE] argument. Input must be tabular data in the form of Boolean (0 or 1)\n"
      << "values separated by tab, or CHAR if specified. Data must be preceded by a\n"
      << "single-line header naming the columns. Output takes the form of a single integer\n"
      << "cluster assignment per line, where each line corresponds to the data row of the\n"
      << "input.\n"
      << "\n"
      << "  -t, --delimiter=CHAR      use CHAR for field separator (default: TAB)\n"
      << "  -c, --clusters=CFILE      read pre-computed cluster assignments from CFILE\n"
      << "                            (one cluster identifier per line, ordered by instance)\n"
      << "  -m, --multiplier=MULT     multiplying constant C_m (default: 1000.0)\n"
      << "  -p, --power=POW           power constant P (default: 10.0)\n"
      << "  -v, --verbosity=VALUE     one of {0,1,2,3,quiet,warning,info,debug} (default: 1)\n"
      << "  -h, --help                display this help and exit\n"
      << "  -V, --version             output version information and exit\n";
}

void log_message(char const *message, verbosity_level verbosity, message_type mtype) {
  using time_type = std::chrono::time_point<std::chrono::high_resolution_clock>;
  static std::stack<time_type, std::list<time_type>> time_stack;
  if (VERBOSITY < verbosity) {
    return;
  }
  if (mtype == STANDARD && !time_stack.empty()) {
    std::cerr << '\n';
  }
  if (mtype == STANDARD || mtype == START) {
    std::time_t const now = std::time(nullptr);
    std::cerr << std::string(time_stack.size(), '\t')
              << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << " - " << message;
  }
  if (mtype == STANDARD) {
    std::cerr << '\n';
  } else if (mtype == START) {
    time_stack.emplace(std::chrono::high_resolution_clock::now());
  } else if (mtype == FINISH) {
    if (time_stack.empty()) {
      throw std::logic_error{"FINISH logged without corresponding START"};
    }
    auto const start_time = time_stack.top();
    std::chrono::duration<double> const elapsed =
        std::chrono::high_resolution_clock::now() - start_time;
    time_stack.pop();
    std::cerr << std::string(time_stack.size(), '\t') << "DONE (" << elapsed.count()
              << " seconds)\n";
  }
}

bool parse_double(char const *arg, double &out) {
  char *end = nullptr;
  errno = 0;
  double const v = std::strtod(arg, &end);
  if (errno != 0 || end == arg || *end != '\0') {
    return false;
  }
  out = v;
  return true;
}

bool parse_verbosity(char const *arg, verbosity_level &out) {
  if (std::strcmp(arg, "0") == 0 || std::strcmp(arg, "quiet") == 0) {
    out = QUIET;
  } else if (std::strcmp(arg, "1") == 0 || std::strcmp(arg, "warning") == 0) {
    out = WARNING;
  } else if (std::strcmp(arg, "2") == 0 || std::strcmp(arg, "info") == 0) {
    out = INFO;
  } else if (std::strcmp(arg, "3") == 0 || std::strcmp(arg, "debug") == 0) {
    out = DEBUG;
  } else {
    return false;
  }
  return true;
}

} // namespace

namespace {

int run(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);

  char const *cfile = nullptr;
  double multiplier = 1000.0;
  double power = 10.0;

  static option const long_options[] = {
      {.name = "delimiter", .has_arg = required_argument, .flag = nullptr, .val = 't'},
      {.name = "clusters", .has_arg = required_argument, .flag = nullptr, .val = 'c'},
      {.name = "multiplier", .has_arg = required_argument, .flag = nullptr, .val = 'm'},
      {.name = "power", .has_arg = required_argument, .flag = nullptr, .val = 'p'},
      {.name = "verbosity", .has_arg = required_argument, .flag = nullptr, .val = 'v'},
      {.name = "help", .has_arg = no_argument, .flag = nullptr, .val = 'h'},
      {.name = "version", .has_arg = no_argument, .flag = nullptr, .val = 'V'},
      {.name = nullptr, .has_arg = 0, .flag = nullptr, .val = 0},
  };

  while (true) {
    int option_index = 0;
    int const c = getopt_long(argc, argv, "t:c:m:p:v:hV", long_options, &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 't':
      if (std::strcmp(optarg, "\\t") == 0) {
        DELIMITER = '\t';
      } else if (std::strlen(optarg) != 1) {
        std::cerr << argv[0] << ": -t, --delimiter=CHAR must be a single character\n";
        return 1;
      } else {
        DELIMITER = optarg[0];
      }
      break;
    case 'c':
      cfile = optarg;
      break;
    case 'm':
      if (!parse_double(optarg, multiplier)) {
        std::cerr << argv[0] << ": -m, --multiplier=MULT must be a valid floating-point value\n";
        return 1;
      }
      break;
    case 'p':
      if (!parse_double(optarg, power)) {
        std::cerr << argv[0] << ": -p, --power=POW must be a valid floating-point value\n";
        return 1;
      }
      break;
    case 'v':
      if (!parse_verbosity(optarg, VERBOSITY)) {
        std::cerr << argv[0]
                  << ": -v, --verbosity=VALUE must be one of "
                     "{0,1,2,3,quiet,warning,info,debug}\n";
        short_usage(argv[0]);
        return 1;
      }
      break;
    case 'h':
      usage(argv[0]);
      return 0;
    case 'V':
      std::cout << "POPC C++ by Ryan N. Lichtenwalter v" << POPC_VERSION << "\n";
      return 0;
    default:
      short_usage(argv[0]);
      return 1;
    }
  }

  std::ifstream ifs;
  if (optind < argc) {
    if (optind != argc - 1) {
      std::cerr << argv[0] << ": too many arguments\n";
      short_usage(argv[0]);
      return 1;
    }
    ifs.open(argv[optind]);
    if (!ifs.is_open()) {
      std::cerr << argv[0] << ": cannot open file: " << argv[optind] << "\n";
      return 2;
    }
    log_message((std::string{"FILE = "} + argv[optind]).c_str(), DEBUG, STANDARD);
  }

  log_message("Reading data...", INFO, START);
  popc::dataset data;
  try {
    if (ifs.is_open()) {
      log_message("Reading from file...", DEBUG, STANDARD);
      data = popc::dataset{ifs, DELIMITER};
      ifs.close();
    } else {
      log_message("Reading from standard input...", DEBUG, STANDARD);
      data = popc::dataset{std::cin, DELIMITER};
    }
  } catch (std::exception const &e) {
    std::cerr << argv[0] << ": parse error: " << e.what() << "\n";
    return 2;
  }
  log_message("DONE", INFO, FINISH);

  std::size_t const initial_num_clusters = data.num_instances() / 2;
  std::vector<std::size_t> assignments(data.num_instances());

  if (cfile != nullptr) {
    log_message("Reading clustering assignments...", INFO, START);
    std::ifstream cluster_ifs{cfile};
    if (!cluster_ifs.is_open()) {
      std::cerr << argv[0] << ": cannot open clusters file: " << cfile << "\n";
      return 2;
    }
    std::size_t instance_num = 0;
    std::size_t cluster_num = 0;
    while (cluster_ifs >> cluster_num) {
      if (instance_num >= data.num_instances()) {
        std::cerr << argv[0]
                  << ": too many lines in cluster file for the number "
                     "of instances\n";
        return 2;
      }
      assignments[instance_num++] = cluster_num;
      // Consume optional trailing whitespace; tolerate either Unix or
      // Windows line endings without breaking the read.
      cluster_ifs >> std::ws;
    }
    if (instance_num != data.num_instances()) {
      std::cerr << argv[0] << ": cluster file has fewer lines than instances\n";
      return 2;
    }
    log_message("DONE", INFO, FINISH);
  } else {
    log_message("Performing bitpacked k-modes seed...", INFO, START);
    assignments = popc::detail::bitpacked_kmodes_seed(data, initial_num_clusters);
    log_message("DONE", INFO, FINISH);
  }

  // The seed assignment may use cluster identifiers larger than the number
  // of distinct clusters used (e.g. user-supplied cluster file with sparse
  // numbering). Compact to dense [0, k) so the cluster vector size below is
  // tight and the popc loop's clusters.size() reflects reality.
  if (assignments.empty()) {
    return 0;
  }
  std::size_t actual_num_clusters = 0;
  {
    std::size_t const max_label = *std::ranges::max_element(assignments);
    std::vector<std::size_t> remap(max_label + 1, std::numeric_limits<std::size_t>::max());
    for (auto const a : assignments) {
      if (remap[a] == std::numeric_limits<std::size_t>::max()) {
        remap[a] = actual_num_clusters++;
      }
    }
    for (auto &a : assignments) {
      a = remap[a];
    }
  }

  log_message("Processing cluster assignments...", INFO, START);
  std::vector<popc::cluster> clusters_vec(actual_num_clusters,
                                          popc::cluster{data.num_attributes()});
  for (std::size_t i = 0; i < assignments.size(); ++i) {
    auto &cluster = clusters_vec[assignments[i]];
    cluster.add_instance(i);
    for (std::size_t j = 0; j < data.num_attributes(); ++j) {
      if (data(i, j)) {
        cluster.increment_attribute_count(j);
      }
    }
  }
  log_message("DONE", INFO, FINISH);

  log_message("Executing POPC algorithm...", INFO, START);
  std::list<popc::cluster> clusters_list;
  std::ranges::move(clusters_vec, std::back_inserter(clusters_list));
  auto const result = popc::popc(data, clusters_list, multiplier, power);
  log_message("DONE", INFO, FINISH);

  log_message("Outputting results...", INFO, START);
  for (auto const val : result) {
    std::cout << val << '\n';
  }
  log_message("DONE", INFO, FINISH);

  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  try {
    return run(argc, argv);
  } catch (std::exception const &e) {
    std::cerr << argv[0] << ": " << e.what() << "\n";
    return 2;
  } catch (...) {
    std::cerr << argv[0] << ": unknown error\n";
    return 2;
  }
}
