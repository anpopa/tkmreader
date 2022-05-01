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
#include <filesystem>
#include <iostream>
#include <json/json.h>
#include <memory>
#include <ostream>
#include <string>
#include <unistd.h>

#include "Application.h"
#include "Arguments.h"
#include "Defaults.h"
#include "Dispatcher.h"
#include "Helpers.h"
#include "IDatabase.h"
#include "JsonWriter.h"

#include "Collector.pb.h"
#include "Monitor.pb.h"

using std::shared_ptr;
using std::string;

namespace tkm::reader
{

static auto splitString(const std::string &s, char delim) -> std::vector<std::string>;
static void
printProcAcct(const tkm::msg::monitor::ProcAcct &acct, uint64_t systemTime, uint64_t monotonicTime);
static void printProcEvent(const tkm::msg::monitor::ProcEvent &event,
                           uint64_t systemTime,
                           uint64_t monotonicTime);
static void printSysProcStat(const tkm::msg::monitor::SysProcStat &sysProcStat,
                             uint64_t systemTime,
                             uint64_t monotonicTime);
static void printSysProcPressure(const tkm::msg::monitor::SysProcPressure &sysProcPressure,
                                 uint64_t systemTime,
                                 uint64_t monotonicTime);
static void printSysProcMeminfo(const tkm::msg::monitor::SysProcMeminfo &sysProcMeminfo,
                                uint64_t systemTime,
                                uint64_t monotonicTime);

static bool doPrepareData(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doConnect(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doReconnect(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doSendDescriptor(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doRequestSession(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doSetSession(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doStartStream(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doProcessData(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doStatus(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doQuit(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);

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
  case Dispatcher::Action::PrepareData:
    return doPrepareData(getShared(), request);
  case Dispatcher::Action::Connect:
    return doConnect(getShared(), request);
  case Dispatcher::Action::Reconnect:
    return doReconnect(getShared(), request);
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

static bool doPrepareData(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  Dispatcher::Request rq;
  bool status = true;

  App()->getDeviceData().set_state(tkm::msg::control::DeviceData_State_Unknown);
  App()->getDeviceData().set_name(App()->getArguments()->getFor(Arguments::Key::Name));
  App()->getDeviceData().set_address(App()->getArguments()->getFor(Arguments::Key::Address));
  App()->getDeviceData().set_port(std::stoi(App()->getArguments()->getFor(Arguments::Key::Port)));
  App()->getDeviceData().set_hash(hashForDevice(App()->getDeviceData()));

  IDatabase::Request dbInit = {.action = IDatabase::Action::InitDatabase};
  status = App()->getDatabase()->pushRequest(dbInit);

  if (!status) {
    std::cout << "Connot initialize output files" << std::endl;
    rq.action = Dispatcher::Action::Quit;
  } else {
    rq.action = Dispatcher::Action::Connect;
  }

  return mgr->pushRequest(rq);
}

static bool doConnect(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  Dispatcher::Request rq;

  if (App()->getConnection()->connect() < 0) {
    std::cout << "INFO: Connection to taskmonitor failed. Retrying..." << std::endl;
    rq.action = Dispatcher::Action::Reconnect;
  } else {
    rq.action = Dispatcher::Action::SendDescriptor;
  }

  return mgr->pushRequest(rq);
}

static bool doReconnect(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  Dispatcher::Request rq;

  // Sleep before retrying
  ::sleep(1);

  // Reset connection object
  App()->resetConnection();

  if (App()->getConnection()->connect() < 0) {
    std::cout << "INFO: Connection to taskmonitor failed. Retrying..." << std::endl;
    rq.action = Dispatcher::Action::Reconnect;
  } else {
    rq.action = Dispatcher::Action::SendDescriptor;
  }

  return mgr->pushRequest(rq);
}

static bool doSendDescriptor(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  tkm::msg::collector::Descriptor descriptor;

  descriptor.set_id("Reader");
  if (!sendCollectorDescriptor(App()->getConnection()->getFD(), descriptor)) {
    logError() << "Failed to send descriptor";
    std::cout << "ERROR: Failed to send descriptor";
    Dispatcher::Request nrq{.action = Dispatcher::Action::Reconnect};
    return mgr->pushRequest(nrq);
  }
  logDebug() << "Sent collector descriptor";

  Dispatcher::Request nrq{.action = Dispatcher::Action::RequestSession};
  return mgr->pushRequest(nrq);
}

static bool doRequestSession(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq)
{
  tkm::msg::Envelope envelope;
  tkm::msg::collector::Request request;

  request.set_id("CreateSession");
  request.set_type(tkm::msg::collector::Request::Type::Request_Type_CreateSession);

  envelope.mutable_mesg()->PackFrom(request);
  envelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
  envelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

  logDebug() << "Request session to monitor";
  return App()->getConnection()->writeEnvelope(envelope);
}

static bool doSetSession(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq)
{
  const auto &sessionInfo = std::any_cast<tkm::msg::monitor::SessionInfo>(rq.bulkData);
  bool status = true;

  std::cout << "INFO: Monitor accepted session with id: " << sessionInfo.hash() << std::endl;
  logDebug() << "Monitor accepted: " << sessionInfo.hash();
  App()->getSessionInfo() = sessionInfo;

  App()->getSessionData().set_hash(sessionInfo.hash());
  App()->getSessionData().set_proc_acct_poll_interval(sessionInfo.proc_acct_poll_interval());
  App()->getSessionData().set_proc_event_poll_interval(sessionInfo.proc_event_poll_interval());
  App()->getSessionData().set_sys_proc_stat_poll_interval(
      sessionInfo.sys_proc_stat_poll_interval());
  App()->getSessionData().set_sys_proc_meminfo_poll_interval(
      sessionInfo.sys_proc_meminfo_poll_interval());
  App()->getSessionData().set_sys_proc_pressure_poll_interval(
      sessionInfo.sys_proc_pressure_poll_interval());

  logDebug() << "SessionInfo procAcctPollInterval=" << sessionInfo.proc_acct_poll_interval()
             << " procEventPollInt=" << sessionInfo.proc_event_poll_interval()
             << " sysProcStatPollInt=" << sessionInfo.sys_proc_stat_poll_interval()
             << " sysProcMemInfoPollInt=" << sessionInfo.sys_proc_meminfo_poll_interval()
             << " sysProcPressurePollInt=" << sessionInfo.sys_proc_pressure_poll_interval();

  Json::Value head;
  head["type"] = "session";
  head["device"] = App()->getArguments()->getFor(Arguments::Key::Name);
  head["session"] = App()->getSessionInfo().hash();
  head["lifecycle"] = App()->getSessionInfo().lifecycle_id();

  Json::Value intervals;
  intervals["proc_acct_poll"] = sessionInfo.proc_acct_poll_interval();
  intervals["proc_event_poll"] = sessionInfo.proc_event_poll_interval();
  intervals["sys_proc_stat_poll"] = sessionInfo.sys_proc_stat_poll_interval();
  intervals["sys_proc_meminfo_poll"] = sessionInfo.sys_proc_meminfo_poll_interval();
  intervals["sys_proc_pressure_poll"] = sessionInfo.sys_proc_pressure_poll_interval();
  head["intervals"] = intervals;

  writeJsonStream() << head;

  IDatabase::Request dbReq = {.action = IDatabase::Action::AddSession, .bulkData = rq.bulkData};
  status = App()->getDatabase()->pushRequest(dbReq);

  if (status) {
    Dispatcher::Request srq{.action = Dispatcher::Action::StartStream};
    status = mgr->pushRequest(srq);
  }

  return status;
}

static bool doStartStream(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  App()->getConnection()->startCollectorTimers();
  std::cout << "INFO: Collection started for session: " << App()->getSessionInfo().hash()
            << std::endl;
  return true;
}

static bool doProcessData(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq)
{
  const auto &data = std::any_cast<tkm::msg::monitor::Data>(rq.bulkData);

  switch (data.what()) {
  case tkm::msg::monitor::Data_What_ProcAcct: {
    tkm::msg::monitor::ProcAcct procAcct;
    data.payload().UnpackTo(&procAcct);
    printProcAcct(procAcct, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_ProcEvent: {
    tkm::msg::monitor::ProcEvent procEvent;
    data.payload().UnpackTo(&procEvent);
    printProcEvent(procEvent, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcStat: {
    tkm::msg::monitor::SysProcStat sysProcStat;
    data.payload().UnpackTo(&sysProcStat);
    printSysProcStat(sysProcStat, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcMeminfo: {
    tkm::msg::monitor::SysProcMeminfo sysProcMeminfo;
    data.payload().UnpackTo(&sysProcMeminfo);
    printSysProcMeminfo(sysProcMeminfo, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcPressure: {
    tkm::msg::monitor::SysProcPressure sysProcPressure;
    data.payload().UnpackTo(&sysProcPressure);
    printSysProcPressure(sysProcPressure, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  default:
    break;
  }

  IDatabase::Request dbReq = {.action = IDatabase::Action::AddData, .bulkData = rq.bulkData};
  return App()->getDatabase()->pushRequest(dbReq);
}

static bool doStatus(const shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq)
{
  const auto &monitorStatus = std::any_cast<tkm::msg::monitor::Status>(rq.bulkData);
  std::string what;

  switch (monitorStatus.what()) {
  case tkm::msg::monitor::Status_What_OK:
    what = tkmDefaults.valFor(tkm::reader::Defaults::Val::StatusOkay);
    break;
  case tkm::msg::monitor::Status_What_Busy:
    what = tkmDefaults.valFor(tkm::reader::Defaults::Val::StatusBusy);
    break;
  case tkm::msg::monitor::Status_What_Error:
  default:
    what = tkmDefaults.valFor(tkm::reader::Defaults::Val::StatusError);
    break;
  }

  logDebug() << "Monitor status (" << monitorStatus.request_id() << "): " << what
             << " Reason: " << monitorStatus.reason();
  if ((monitorStatus.request_id() == "RequestSession") &&
      (monitorStatus.what() == tkm::msg::monitor::Status_What_OK)) {
    return true;
  }

  std::cout << "--------------------------------------------------" << std::endl;
  std::cout << "Status: " << what << " Reason: " << monitorStatus.reason() << std::endl;
  std::cout << "--------------------------------------------------" << std::endl;

  // Trigger the next command
  return doQuit(mgr, rq);
}

static bool doQuit(const shared_ptr<Dispatcher>, const Dispatcher::Request &)
{
  std::cout << std::flush;
  exit(EXIT_SUCCESS);
}

static void
printProcAcct(const tkm::msg::monitor::ProcAcct &acct, uint64_t systemTime, uint64_t monotonicTime)
{
  Json::Value head;

  head["type"] = "acct";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  Json::Value common;
  common["ac_comm"] = acct.ac_comm();
  common["ac_uid"] = acct.ac_uid();
  common["ac_gid"] = acct.ac_gid();
  common["ac_pid"] = acct.ac_pid();
  common["ac_ppid"] = acct.ac_ppid();
  common["ac_utime"] = acct.ac_utime();
  common["ac_stime"] = acct.ac_stime();
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
  io["read_bytes"] = acct.io().read_bytes();
  io["write_bytes"] = acct.io().write_bytes();
  io["read_char"] = acct.io().read_char();
  io["write_char"] = acct.io().write_char();
  io["read_syscalls"] = acct.io().read_syscalls();
  io["write_syscalls"] = acct.io().write_syscalls();
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

static void printProcEvent(const tkm::msg::monitor::ProcEvent &event,
                           uint64_t systemTime,
                           uint64_t monotonicTime)
{
  Json::Value head;
  Json::Value body;

  head["type"] = "procstats";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  body["fork_count"] = event.fork_count();
  body["exec_count"] = event.exec_count();
  body["exit_count"] = event.exit_count();
  body["uid_count"] = event.uid_count();
  body["gid_count"] = event.gid_count();
  head["procstats"] = body;

  writeJsonStream() << head;
}

static void printSysProcStat(const tkm::msg::monitor::SysProcStat &sysProcStat,
                             uint64_t systemTime,
                             uint64_t monotonicTime)
{
  Json::Value head;

  head["type"] = "stat";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  Json::Value cpu;
  cpu["name"] = sysProcStat.cpu().name();
  cpu["all"] = sysProcStat.cpu().all();
  cpu["usr"] = sysProcStat.cpu().usr();
  cpu["sys"] = sysProcStat.cpu().sys();
  head["cpu"] = cpu;

  writeJsonStream() << head;
}

static void printSysProcMeminfo(const tkm::msg::monitor::SysProcMeminfo &sysProcMeminfo,
                                uint64_t systemTime,
                                uint64_t monotonicTime)
{
  Json::Value head;

  head["type"] = "meminfo";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  Json::Value meminfo;
  meminfo["mem_total"] = sysProcMeminfo.mem_total();
  meminfo["mem_free"] = sysProcMeminfo.mem_free();
  meminfo["mem_available"] = sysProcMeminfo.mem_available();
  meminfo["mem_cached"] = sysProcMeminfo.mem_cached();
  meminfo["mem_available_percent"] = sysProcMeminfo.mem_percent();
  meminfo["swap_total"] = sysProcMeminfo.swap_total();
  meminfo["swap_free"] = sysProcMeminfo.swap_free();
  meminfo["swap_cached"] = sysProcMeminfo.swap_cached();
  meminfo["swap_free_percent"] = sysProcMeminfo.swap_percent();
  head["meminfo"] = meminfo;

  writeJsonStream() << head;
}

static void printSysProcPressure(const tkm::msg::monitor::SysProcPressure &sysProcPressure,
                                 uint64_t systemTime,
                                 uint64_t monotonicTime)
{
  Json::Value head;

  head["type"] = "psi";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

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
