/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Main
 * @details   Main function and signal handling
 *-
 */

#include "Application.h"
#include "Arguments.h"
#include "Defaults.h"

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <map>
#include <taskmonitor/taskmonitor.h>

using namespace tkm::reader;

static void terminate(int signum)
{
  logInfo() << "Received signal " << signum;
  ::exit(EXIT_SUCCESS);
}

auto main(int argc, char **argv) -> int
{
  std::map<Arguments::Key, std::string> args;
  int longIndex = 0;
  bool version = false;
  bool help = false;
  int c;

  struct option longopts[] = {{"name", required_argument, nullptr, 'n'},
                              {"init", no_argument, nullptr, 'i'},
                              {"address", required_argument, nullptr, 'a'},
                              {"port", required_argument, nullptr, 'p'},
                              {"database", required_argument, nullptr, 'd'},
                              {"json", required_argument, nullptr, 'j'},
                              {"verbose", no_argument, nullptr, 'x'},
                              {"timeout", required_argument, nullptr, 't'},
                              {"strict", no_argument, nullptr, 's'},
                              {"version", no_argument, nullptr, 'v'},
                              {"help", no_argument, nullptr, 'h'},
                              {nullptr, 0, nullptr, 0}};

  while ((c = getopt_long(argc, argv, "n:a:p:d:j:t:ixsvh", longopts, &longIndex)) != -1) {
    switch (c) {
    case 'n':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Name, optarg));
      break;
    case 'i':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Init,
                                                         tkmDefaults.valFor(Defaults::Val::True)));
      break;
    case 'a':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Address, optarg));
      break;
    case 'p':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Port, optarg));
      break;
    case 'd':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::DatabasePath, optarg));
      break;
    case 'j':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::JsonPath, optarg));
      break;
    case 't':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Timeout, optarg));
      break;
    case 'x':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Verbose,
                                                         tkmDefaults.valFor(Defaults::Val::True)));
      break;
    case 's':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Strict,
                                                         tkmDefaults.valFor(Defaults::Val::True)));
      break;
    case 'v':
      version = true;
      break;
    case 'h':
    default:
      help = true;
      break;
    }
  }

  if (version) {
    std::cout << "tkmreader: " << tkmDefaults.getFor(tkm::reader::Defaults::Default::Version)
              << " libtkm: " << TKMLIB_VERSION << "\n";
    ::exit(EXIT_SUCCESS);
  }

  if (help) {
    std::cout << "TaskMonitorReader: read and store data from taskmonitor service\n"
              << "Version: " << tkmDefaults.getFor(tkm::reader::Defaults::Default::Version)
              << " libtkm: " << TKMLIB_VERSION << "\n\n";
    std::cout << "Usage: tkmreader [OPTIONS] \n\n";
    std::cout << "  General:\n";
    std::cout << "     --name, -n      <string>  Device name (default unknown)\n";
    std::cout << "     --address, -a   <string>  Device IP address (default localhost)\n";
    std::cout << "     --port, -p      <int>     Device port number (default 3357)\n";
    std::cout
        << "     --timeout, -t   <int>     Number of seconds (>3) for session inactivity timeout\n";
    std::cout << "                               Default and minimum value is 3 seconds.\n";
    std::cout << "     --strict, -s              Stop if target libtkm version missmatch\n";
    std::cout << "     --verbose, -v             Print info messages\n";
    std::cout << "  Output:\n";
    std::cout << "     --init, -i                Force output initialization if files exist\n";
    std::cout
        << "     --database, -d  <string>  Path to output database file. If not set DB output is "
           "disabled\n";
    std::cout << "     --json, -j      <string>  Path to output json file. If not set json output "
                 "is disabled\n";
    std::cout << "                               Hint: Use 'stdout' for standard output\n";
    std::cout << "  Help:\n";
    std::cout << "     --help, -h                Print this help\n\n";

    ::exit(EXIT_SUCCESS);
  }

  ::signal(SIGINT, terminate);
  ::signal(SIGTERM, terminate);

  try {
    Application app{"TKMReader", "TaskMonitor Reader", args};

    Dispatcher::Request prepareRequest{.action = Dispatcher::Action::PrepareData,
                                       .bulkData = std::make_any<int>(0),
                                       .args = std::map<tkm::reader::Defaults::Arg, std::string>()};
    app.getDispatcher()->pushRequest(prepareRequest);

    app.run();
  } catch (std::exception &e) {
    std::cout << "Application start failed. " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
