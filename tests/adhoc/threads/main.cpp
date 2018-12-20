#include <algorithm>
#include <cstring>
#include <iostream>
#include <list>
#include <string>
#include <thread>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include "threads.h"

namespace po = boost::program_options;

class ValidationError : public std::runtime_error {
 public:
  ValidationError(const std::string& what) : std::runtime_error(what) {}
};

static unsigned int validate(const po::variables_map& vm,
                             const char* key,
                             unsigned int min) {
  try {
    unsigned int value = vm[key].as<unsigned int>();

    if (value < min) {
      throw ValidationError(boost::str(
          boost::format("option '%s' must be at least %u") % key % min));
    }

    return value;
  } catch (boost::bad_any_cast& e) {
    std::cerr << key << ": " << e.what() << std::endl;
    throw;
  }
}

int main(int argc, char* argv[]) {
  po::options_description desc("Allowed options");
  po::variables_map vm;
  unsigned int threads, segments, maxTime, transactions;

  // clang-format off
  desc.add_options()
    ("help", "show this message")
    ("licence,l", po::value<std::string>(), "set the New Relic licence key")
    ("license", po::value<std::string>(), "set the New Relic license key")
    ("appname,a", po::value<std::string>()->default_value("C Threads"), "set the New Relic application name")
    ("host,h", po::value<std::string>()->default_value("collector.newrelic.com"), "set the New Relic host")
    ("transactions,x", po::value<unsigned int>()->default_value(10), "number of transactions to create")
    ("threads,t", po::value<unsigned int>()->default_value(100), "number of threads each transaction should spawn")
    ("segments,s", po::value<unsigned int>()->default_value(20), "number of segments each thread should create")
    ("max-time,m", po::value<unsigned int>()->default_value(100), "maximum time a segment can be, in milliseconds")
  ;
  // clang-format on

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
  } catch (po::invalid_option_value& e) {
    std::cerr << "Error parsing command line options: " << e.what()
              << std::endl;
    return 1;
  }

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  // This would probably make more sense as a custom validator type in
  // program_options, but whatever. This is easy.
  try {
    threads = validate(vm, "threads", 1);
    segments = validate(vm, "segments", 1);
    maxTime = validate(vm, "max-time", 1);
    transactions = validate(vm, "transactions", 1);
  } catch (ValidationError& e) {
    std::cerr << "Error parsing command line options: " << e.what()
              << std::endl;
    return 1;
  }

  if (vm.count("licence") == 0 && vm.count("license") == 0) {
    std::cerr << "Error parsing command line options: a licence key must be "
                 "provided"
              << std::endl;
    return 1;
  }

  Config config(vm["appname"].as<std::string>(),
                vm.count("licence") ? vm["licence"].as<std::string>()
                                    : vm["license"].as<std::string>());

  std::strncpy(config.config->redirect_collector,
               vm["host"].as<std::string>().c_str(), 255);
  config.config->redirect_collector[254] = '\0';

  config.config->log_level = LOG_VERBOSE;
  std::strcpy(config.config->log_filename, "stdout");

  // Wait up to five seconds for the application to connect.
  Application app(config, 5000);

  // Spawn threads.
  std::list<std::thread> txnThreads;

  for (unsigned int i = 0; i < transactions; i++) {
    std::string id(boost::str(boost::format("%u") % i));

    txnThreads.push_back(std::thread(transactionThread, std::ref(app), id,
                                     threads, segments, maxTime));
  }

  // Wait for them to complete.
  std::for_each(txnThreads.begin(), txnThreads.end(),
                [](std::thread& thread) { thread.join(); });

  return 0;
}
