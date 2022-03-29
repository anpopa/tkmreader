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

#include <csignal>
#include <cstdlib>
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

    struct option longopts[] = {{"address", required_argument, nullptr, 'a'},
                                {"port", required_argument, nullptr, 'p'},
                                {"format", required_argument, nullptr, 'f'},
                                {"help", no_argument, nullptr, 'h'},
                                {nullptr, 0, nullptr, 0}};

    while ((c = getopt_long(argc, argv, "s:e:a:t:k:dh", longopts, &longIndex)) != -1) {
        switch (c) {
        case 'a':
            args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Address, optarg));
            break;
        case 'p':
            args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Port, optarg));
            break;
        case 'f':
            args.insert(std::pair<Arguments::Key, std::string>(Arguments::Key::Format, optarg));
            break;
        case 'h':
            help = true;
            break;
        default:
            break;
        }
    }

    if (help) {
        cout << "tmreader: read data stream from a taskmonitor device"
             << tkmDefaults.getFor(tkm::reader::Defaults::Default::Version) << "\n\n";
        cout << "Usage: tmreader [OPTIONS] \n\n";
        cout << "  General:\n";
        cout << "     --address, -a   <string>  Device IP address (default localhost)\n";
        cout << "     --port, -p      <int>     Device port number (default 3357)\n";
        cout << "     --format, -f    <string>  Output stream format (default json)\n";
        cout << "  Help:\n";
        cout << "     --help, -h                Print this help\n\n";

        exit(EXIT_SUCCESS);
    }

    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    try {
        Application app {"TMC", "TaskMonitor Reader", args};

        Command::Request startStreamRequest {.action = Command::Action::StartStream};
        app.getCommand()->addRequest(startStreamRequest);

        // The last command is to quit
        Command::Request quitRequest {.action = Command::Action::Quit};
        app.getCommand()->addRequest(quitRequest);

        // Request connection
        Dispatcher::Request connectRequest {
            .action = Dispatcher::Action::Connect,
        };
        app.getDispatcher()->pushRequest(connectRequest);

        app.run();
    } catch (std::exception &e) {
        cout << "Application start failed. " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
