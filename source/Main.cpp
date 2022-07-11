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

using namespace std;
using namespace tkm::reader;

static void terminate(int signum)
{
  logInfo() << "Received signal " << signum;
  exit(EXIT_SUCCESS);
}

auto main(int argc, char **argv) -> int
{
  std::map<Arguments::Key, std::string> args;
  int longIndex = 0;
  bool help = false;
  int c;

  struct option longopts[] = {{"name", required_argument, nullptr, 'n'},
                              {"init", no_argument, nullptr, 'i'},
                              {"address", required_argument, nullptr, 'a'},
                              {"port", required_argument, nullptr, 'p'},
                              {"database", required_argument, nullptr, 'd'},
                              {"json", required_argument, nullptr, 'j'},
                              {"verbose", no_argument, nullptr, 'v'},
                              {"help", no_argument, nullptr, 'h'},
                              {nullptr, 0, nullptr, 0}};

  while ((c = getopt_long(argc, argv, "n:a:p:d:j:ihv", longopts, &longIndex)) != -1) {
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
    case 'v':
      args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Verbose,
                                                         tkmDefaults.valFor(Defaults::Val::True)));
      break;
    case 'h':
      help = true;
      break;
    default:
      break;
    }
  }

  if (help) {
    cout << "tkm-reader: read data stream from a taskmonitor device\n"
         << "version: " << tkmDefaults.getFor(tkm::reader::Defaults::Default::Version) << "\n\n";
    cout << "Usage: tkm-reader [OPTIONS] \n\n";
    cout << "  General:\n";
    cout << "     --name, -n      <string>  Device name (default unknown)\n";
    cout << "     --address, -a   <string>  Device IP address (default localhost)\n";
    cout << "     --port, -p      <int>     Device port number (default 3357)\n";
    cout << "     --verbose, -v             Print info messages on STDOUT\n";
    cout << "  Output:\n";
    cout << "     --init, -i                Force output initialization if files exist\n";
    cout << "     --database, -d  <string>  Path to output database file. If not set DB output is "
            "disabled\n";
    cout << "     --json, -j      <string>  Path to output json file. Use STDOUT if not set\n";
    cout << "                               Hint: Can use /dev/null to ignore the output\n";
    cout << "  Help:\n";
    cout << "     --help, -h                Print this help\n\n";

    exit(EXIT_SUCCESS);
  }

  signal(SIGINT, terminate);
  signal(SIGTERM, terminate);

  try {
    Application app{"TKM-Reader", "TaskMonitor Reader", args};

    // Prepare output data
    Dispatcher::Request prepareRequest{
        .action = Dispatcher::Action::PrepareData,
    };
    app.getDispatcher()->pushRequest(prepareRequest);

    app.run();
  } catch (std::exception &e) {
    cout << "Application start failed. " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
