/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Dispatcher Class
 * @details   Global application dispatcher
 *-
 */

#include <chrono>
#include <ctime>
#include <iostream>
#include <ostream>
#include <string>
#include <unistd.h>

#include "Application.h"
#include "Defaults.h"
#include "Dispatcher.h"
#include "Helpers.h"

#include "Client.pb.h"
#include "Server.pb.h"

using std::shared_ptr;
using std::string;

namespace tkm::reader
{

static auto splitString(const std::string &s, char delim) -> std::vector<std::string>;

static auto doConnect(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
static auto doSendDescriptor(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq)
    -> bool;
static auto doRequestSession(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq)
    -> bool;
static auto doSetSession(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
static auto doStartStream(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
static auto doStatus(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
static auto doQuit(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;

void Dispatcher::enableEvents()
{
    App()->addEventSource(m_queue);
}

auto Dispatcher::pushRequest(Request &request) -> bool
{
    return m_queue->push(request);
}

auto Dispatcher::requestHandler(const Request &request) -> bool
{
    switch (request.action) {
    case Dispatcher::Action::Connect:
        return doConnect(getShared(), request);
    case Dispatcher::Action::SendDescriptor:
        return doSendDescriptor(getShared(), request);
    case Dispatcher::Action::RequestSession:
        return doRequestSession(getShared(), request);
    case Dispatcher::Action::SetSession:
        return doSetSession(getShared(), request);
    case Dispatcher::Action::StartStream:
        return doStartStream(getShared(), request);
    case Dispatcher::Action::Status:
        return doStatus(getShared(), request);
    case Dispatcher::Action::Quit:
        return doQuit(getShared(), request);
    default:
        break;
    }

    logError() << "Unknown action request";
    return false;
}

static auto doConnect(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &) -> bool
{
    Dispatcher::Request rq;

    if (App()->getConnection()->connect() < 0) {
        std::cout << "Connection to tkmaunchd failed" << std::endl;
        rq.action = Dispatcher::Action::Quit;
    } else {
        rq.action = Dispatcher::Action::SendDescriptor;
    }

    return mgr->pushRequest(rq);
}

static auto doSendDescriptor(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &) -> bool
{
    tkm::msg::client::Descriptor descriptor;
    char hostName[64] = {0};

    gethostname(hostName, sizeof(hostName));
    std::string instId = hostName;
    
    instId += "." + std::to_string(getpid());
    descriptor.set_id(instId);

    if (!sendClientDescriptor(App()->getConnection()->getFD(), descriptor)) {
        logError() << "Failed to send descriptor";
        Dispatcher::Request nrq {.action = Dispatcher::Action::Quit};
        return mgr->pushRequest(nrq);
    }
    logDebug() << "Sent client descriptor";

    Dispatcher::Request nrq {.action = Dispatcher::Action::RequestSession};
    return mgr->pushRequest(nrq);
}

static auto doRequestSession(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq)
    -> bool
{
    tkm::msg::Envelope envelope;
    tkm::msg::client::Request request;

    request.set_id("CreateSession");
    request.set_type(tkm::msg::client::Request::Type::Request_Type_CreateSession);

    envelope.mutable_mesg()->PackFrom(request);
    envelope.set_target(tkm::msg::Envelope_Recipient_Server);
    envelope.set_origin(tkm::msg::Envelope_Recipient_Client);

    logDebug() << "Request session to server";
    return App()->getConnection()->writeEnvelope(envelope);
}

static auto doSetSession(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool
{
    const auto &sessionInfo = std::any_cast<tkm::msg::server::SessionInfo>(rq.bulkData);

    logDebug() << "Server accepted: " << sessionInfo.id();
    App()->getSession() = sessionInfo;

    return App()->getCommand()->trigger();
}

static auto doStartStream(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &) -> bool
{
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::client::Request requestMessage;
    tkm::msg::client::StreamState streamState;

    streamState.set_state(true);
    requestMessage.set_id("StartStream");
    requestMessage.set_type(tkm::msg::client::Request_Type_StreamState);
    requestMessage.mutable_data()->PackFrom(streamState);

    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Server);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Client);

    logDebug() << "Request start stream";
    return App()->getConnection()->writeEnvelope(requestEnvelope);
}

static auto doStatus(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool
{
    const auto &serverStatus = std::any_cast<tkm::msg::server::Status>(rq.bulkData);
    std::string what;

    switch (serverStatus.what()) {
    case tkm::msg::server::Status_What_OK:
        what = tkmDefaults.valFor(tkm::reader::Defaults::Val::StatusOkay);
        break;
    case tkm::msg::server::Status_What_Busy:
        what = tkmDefaults.valFor(tkm::reader::Defaults::Val::StatusBusy);
        break;
    case tkm::msg::server::Status_What_Error:
    default:
        what = tkmDefaults.valFor(tkm::reader::Defaults::Val::StatusError);
        break;
    }

    logDebug() << "Server status (" << serverStatus.requestid() << "): " << what
               << " Reason: " << serverStatus.reason();
    if ((serverStatus.requestid() == "RequestSession")
        && (serverStatus.what() == tkm::msg::server::Status_What_OK)) {
        return true;
    }

    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "Status: " << what << " Reason: " << serverStatus.reason() << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    // Trigger the next command
    return App()->getCommand()->trigger();
}

static auto doQuit(const shared_ptr<Dispatcher> &, const Dispatcher::Request &) -> bool
{
    std::cout << std::flush;
    exit(EXIT_SUCCESS);
}

} // namespace tkm::reader
