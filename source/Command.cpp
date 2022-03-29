/*-
 * Copyright (c) 2020 Alin Popa
 * All rights reserved.
 */

/*
 * @author Alin Popa <alin.popa@fxdata.ro>
 */

#include "Command.h"
#include "Application.h"

namespace tkm::reader
{

Command::Command()
: UserEvent("Command")
{
    setCallback([this]() {
        if (m_requests.empty()) {
            Dispatcher::Request rq {.action = Dispatcher::Action::Quit};
            App()->getDispatcher()->pushRequest(rq);
            return false;
        }

        auto request = m_requests.front();

        switch (request.action) {
        case Command::Action::StartStream: {
            Dispatcher::Request rq {.action = Dispatcher::Action::StartStream};
            App()->getDispatcher()->pushRequest(rq);
            break;
        }
        case Command::Action::Quit: {
            Dispatcher::Request rq {.action = Dispatcher::Action::Quit};
            App()->getDispatcher()->pushRequest(rq);
            break;
        }
        default:
            logError() << "Unknown command request";
            break;
        }

        m_requests.pop_front();

        return true;
    });
}

void Command::enableEvents()
{
    App()->addEventSource(getShared());
}

} // namespace tkm::reader
