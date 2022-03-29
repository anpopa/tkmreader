/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Command Class
 * @details   Handle user command sequence
 *-
 */

#pragma once

#include <list>
#include <string>

#include "Defaults.h"
#include "Dispatcher.h"

#include "../bswinfra/source/Exceptions.h"
#include "../bswinfra/source/IApplication.h"
#include "../bswinfra/source/UserEvent.h"

using namespace bswi::log;
using namespace bswi::event;

namespace tkm::reader
{

class Command final : public UserEvent, public std::enable_shared_from_this<Command>
{
public:
    enum class Action { StartStream, Quit };

    typedef struct Request {
        Command::Action action;
        std::map<tkm::reader::Defaults::Arg, std::string> args;
    } Request;

public:
    Command();

    auto getShared() -> std::shared_ptr<Command> { return shared_from_this(); }
    void enableEvents();
    void addRequest(const Command::Request &request) { m_requests.push_back(request); }

public:
    Command(Command const &) = delete;
    void operator=(Command const &) = delete;

private:
    std::list<Command::Request> m_requests = {};
};

} // namespace tkm::reader
