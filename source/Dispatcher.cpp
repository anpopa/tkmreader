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
#include <json/json.h>

#include "Application.h"
#include "Defaults.h"
#include "Dispatcher.h"
#include "Helpers.h"
#include "JsonWriter.h"

#include "Client.pb.h"
#include "Server.pb.h"

using std::shared_ptr;
using std::string;

namespace tkm::reader
{

static auto splitString(const std::string &s, char delim) -> std::vector<std::string>;
static void printProcAcct(const tkm::msg::server::ProcAcct &acct, uint64_t ts);
static void printProcEvent(const tkm::msg::server::ProcEvent &event, uint64_t ts);
static void printSysProcStat(const tkm::msg::server::SysProcStat &sysProcStat, uint64_t ts);
static void printSysProcPressure(const tkm::msg::server::SysProcPressure &sysProcPressure, uint64_t ts);

static auto doConnect(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
static auto doSendDescriptor(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq)
    -> bool;
static auto doRequestSession(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq)
    -> bool;
static auto doSetSession(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
static auto doStartStream(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
static auto doProcessData(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool;
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
    case Dispatcher::Action::ProcessData:
        return doProcessData(getShared(), request);
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

static auto doProcessData(const shared_ptr<Dispatcher> &mgr, const Dispatcher::Request &rq) -> bool
{
    const auto &data = std::any_cast<tkm::msg::server::Data>(rq.bulkData);

    switch (data.what()) {
        case tkm::msg::server::Data_What_ProcAcct: {
            tkm::msg::server::ProcAcct procAcct;
            data.payload().UnpackTo(&procAcct);
            printProcAcct(procAcct, data.timestamp());
            break;
        }
        case tkm::msg::server::Data_What_ProcEvent: {
            tkm::msg::server::ProcEvent procEvent;
            data.payload().UnpackTo(&procEvent);
            printProcEvent(procEvent, data.timestamp());
            break;
        }
        case tkm::msg::server::Data_What_SysProcStat: {
            tkm::msg::server::SysProcStat sysProcStat;
            data.payload().UnpackTo(&sysProcStat);
            printSysProcStat(sysProcStat, data.timestamp());
            break;
        }
        case tkm::msg::server::Data_What_SysProcPressure: {
            tkm::msg::server::SysProcPressure sysProcPressure;
            data.payload().UnpackTo(&sysProcPressure);
            printSysProcPressure(sysProcPressure, data.timestamp());
            break;
        }
        default:
            break;
    }

    return true;
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

static void printProcAcct(const tkm::msg::server::ProcAcct &acct, uint64_t ts)
{
    Json::Value head;

    head["type"] = "acct";
    head["time"] = ts;

    Json::Value common;
    common["ac_comm"] = acct.ac_comm();
    common["ac_uid"] = acct.ac_uid();
    common["ac_gid"] = acct.ac_gid();
    common["ac_pid"] = acct.ac_pid();
    common["ac_ppid"] = acct.ac_ppid();
    common["ac_utime"] = acct.ac_utime();
    common["ac_stime"] = acct.ac_stime();
    common["user_cpu_percent"] = acct.user_cpu_percent();
    common["sys_cpu_percent"] = acct.sys_cpu_percent();
    head["common"] = common;

    Json::Value cpu;
    cpu["cpu_count"] = acct.cpu().cpu_count();
    cpu["cpu_run_real_total"] = acct.cpu().cpu_run_real_total();
    cpu["cpu_run_virtual_total"] = acct.cpu().cpu_run_virtual_total();
    cpu["cpu_delay_total"] = acct.cpu().cpu_delay_total();
    cpu["cpu_delay_average"] = acct.cpu().cpu_delay_average();
    head["cpu"] = cpu;

    Json::Value mem;
    mem["coremem"] = acct.mem().coremem();
    mem["virtmem"] = acct.mem().virtmem();
    mem["hiwater_rss"] = acct.mem().hiwater_rss();
    mem["hiwater_vm"] = acct.mem().hiwater_vm();
    head["mem"] = mem;

    Json::Value ctx;
    ctx["nvcsw"] = acct.ctx().nvcsw();
    ctx["nivcsw"] = acct.ctx().nivcsw();
    head["ctx"] = ctx;

    Json::Value swp;
    swp["swapin_count"] = acct.swp().swapin_count();
    swp["swapin_delay_total"] = acct.swp().swapin_delay_total();
    swp["swapin_delay_average"] = acct.swp().swapin_delay_average();
    head["swap"] = ctx;

    Json::Value io;
    io["blkio_count"] = acct.io().blkio_count();
    io["blkio_delay_total"] = acct.io().blkio_delay_total();
    io["blkio_delay_average"] = acct.io().blkio_delay_average();
    head["io"] = io;

    Json::Value reclaim;
    reclaim["freepages_count"] = acct.reclaim().freepages_count();
    reclaim["freepages_delay_total"] = acct.reclaim().freepages_delay_total();
    reclaim["freepages_delay_average"] = acct.reclaim().freepages_delay_average();
    head["reclaim"] = reclaim;

    Json::Value thrashing;
    thrashing["thrashing_count"] = acct.thrashing().thrashing_count();
    thrashing["thrashing_delay_total"] = acct.thrashing().thrashing_delay_total();
    thrashing["thrashing_delay_average"] = acct.thrashing().thrashing_delay_average();
    head["thrashing"] = thrashing;

    writeJsonStream() << head;
}

static void printProcEvent(const tkm::msg::server::ProcEvent &event, uint64_t ts)
{
    Json::Value head;
    Json::Value body;

    head["type"] = "proc";
    head["time"] = ts;

    switch (event.what()) {
        case tkm::msg::server::ProcEvent_What_Fork: {
            tkm::msg::server::ProcEventFork forkEvent;
            event.data().UnpackTo(&forkEvent);

            body["parent_pid"] = forkEvent.parent_pid();
            body["parent_tgid"] = forkEvent.parent_tgid();
            body["child_pid"] = forkEvent.child_pid();
            body["child_tgid"] = forkEvent.child_tgid();
            head["fork"] = body;

            break;
        }
        case tkm::msg::server::ProcEvent_What_Exec: {
            tkm::msg::server::ProcEventExec execEvent;
            event.data().UnpackTo(&execEvent);

            body["process_pid"] = execEvent.process_pid();
            body["process_tgid"] = execEvent.process_tgid();
            head["exec"] = body;

            break;
        }
        case tkm::msg::server::ProcEvent_What_Exit: {
            tkm::msg::server::ProcEventExit exitEvent;
            event.data().UnpackTo(&exitEvent);

            body["process_pid"] = exitEvent.process_pid();
            body["process_tgid"] = exitEvent.process_tgid();
            body["exit_code"] = exitEvent.exit_code();
            head["exit"] = body;

            break;
        }
        case tkm::msg::server::ProcEvent_What_UID: {
            tkm::msg::server::ProcEventUID uidEvent;
            event.data().UnpackTo(&uidEvent);

            body["process_pid"] = uidEvent.process_pid();
            body["process_tgid"] = uidEvent.process_tgid();
            body["ruid"] = uidEvent.ruid();
            body["euid"] = uidEvent.euid();
            head["uid"] = body;

            break;
        }
        case tkm::msg::server::ProcEvent_What_GID: {
            tkm::msg::server::ProcEventGID gidEvent;
            event.data().UnpackTo(&gidEvent);

            body["process_pid"] = gidEvent.process_pid();
            body["process_tgid"] = gidEvent.process_tgid();
            body["rgid"] = gidEvent.rgid();
            body["egid"] = gidEvent.egid();
            head["uid"] = body;

            break;
        }
        default:
            break;
    }

    writeJsonStream() << head;
}

static void printSysProcStat(const tkm::msg::server::SysProcStat &sysProcStat, uint64_t ts)
{
    Json::Value head;

    head["type"] = "stat";
    head["time"] = ts;

    Json::Value cpu;
    cpu["all"] = sysProcStat.cpu().all();
    cpu["usr"] = sysProcStat.cpu().usr();
    cpu["sys"] = sysProcStat.cpu().sys();
    head["cpu"] = cpu;

    writeJsonStream() << head;
}

static void printSysProcPressure(const tkm::msg::server::SysProcPressure &sysProcPressure, uint64_t ts)
{
    Json::Value head;

    head["type"] = "psi";
    head["time"] = ts;

    if (sysProcPressure.has_cpu_some() || sysProcPressure.has_cpu_full()) {
        Json::Value cpu;

        if (sysProcPressure.has_cpu_some()) {
            Json::Value some;
            some["avg10"] = sysProcPressure.cpu_some().avg10();
            some["avg60"] = sysProcPressure.cpu_some().avg60();
            some["avg300"] = sysProcPressure.cpu_some().avg300();
            some["total"] = sysProcPressure.cpu_some().total();
            cpu["some"] = some;
        }

        if (sysProcPressure.has_cpu_full()) {
            Json::Value full;
            full["avg10"] = sysProcPressure.cpu_full().avg10();
            full["avg60"] = sysProcPressure.cpu_full().avg60();
            full["avg300"] = sysProcPressure.cpu_full().avg300();
            full["total"] = sysProcPressure.cpu_full().total();
            cpu["full"] = full;
        }

        head["cpu"] = cpu;
    }

    if (sysProcPressure.has_mem_some() || sysProcPressure.has_mem_full()) {
        Json::Value mem;

        if (sysProcPressure.has_mem_some()) {
            Json::Value some;
            some["avg10"] = sysProcPressure.mem_some().avg10();
            some["avg60"] = sysProcPressure.mem_some().avg60();
            some["avg300"] = sysProcPressure.mem_some().avg300();
            some["total"] = sysProcPressure.mem_some().total();
            mem["some"] = some;
        }

        if (sysProcPressure.has_mem_full()) {
            Json::Value full;
            full["avg10"] = sysProcPressure.mem_full().avg10();
            full["avg60"] = sysProcPressure.mem_full().avg60();
            full["avg300"] = sysProcPressure.mem_full().avg300();
            full["total"] = sysProcPressure.mem_full().total();
            mem["full"] = full;
        }

        head["mem"] = mem;
    }

    if (sysProcPressure.has_io_some() || sysProcPressure.has_io_full()) {
        Json::Value io;

        if (sysProcPressure.has_io_some()) {
            Json::Value some;
            some["avg10"] = sysProcPressure.io_some().avg10();
            some["avg60"] = sysProcPressure.io_some().avg60();
            some["avg300"] = sysProcPressure.io_some().avg300();
            some["total"] = sysProcPressure.io_some().total();
            io["some"] = some;
        }

        if (sysProcPressure.has_io_full()) {
            Json::Value full;
            full["avg10"] = sysProcPressure.io_full().avg10();
            full["avg60"] = sysProcPressure.io_full().avg60();
            full["avg300"] = sysProcPressure.io_full().avg300();
            full["total"] = sysProcPressure.io_full().total();
            io["full"] = full;
        }

        head["io"] = io;
    }

    writeJsonStream() << head;
}

} // namespace tkm::reader
