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
#include "IDatabase.h"
#include "JsonWriter.h"
#include "Logger.h"

namespace tkm::reader
{

static void
printProcAcct(const tkm::msg::monitor::ProcAcct &acct, uint64_t systemTime, uint64_t monotonicTime);
static void
printProcInfo(const tkm::msg::monitor::ProcInfo &info, uint64_t systemTime, uint64_t monotonicTime);
static void printContextInfo(const tkm::msg::monitor::ContextInfo &info,
                             uint64_t systemTime,
                             uint64_t monotonicTime);
static void printProcEvent(const tkm::msg::monitor::ProcEvent &event,
                           uint64_t systemTime,
                           uint64_t monotonicTime);
static void printSysProcStat(const tkm::msg::monitor::SysProcStat &sysProcStat,
                             uint64_t systemTime,
                             uint64_t monotonicTime);
static void printSysProcPressure(const tkm::msg::monitor::SysProcPressure &sysProcPressure,
                                 uint64_t systemTime,
                                 uint64_t monotonicTime);
static void printSysProcMemInfo(const tkm::msg::monitor::SysProcMemInfo &sysProcMemInfo,
                                uint64_t systemTime,
                                uint64_t monotonicTime);
static void printSysProcDiskStats(const tkm::msg::monitor::SysProcDiskStats &sysProcDiskStats,
                                  uint64_t systemTime,
                                  uint64_t monotonicTime);
static void printSysProcBuddyInfo(const tkm::msg::monitor::SysProcBuddyInfo &sysProcBuddyInfo,
                                  uint64_t systemTime,
                                  uint64_t monotonicTime);
static void printSysProcWireless(const tkm::msg::monitor::SysProcWireless &sysProcWireless,
                                 uint64_t systemTime,
                                 uint64_t monotonicTime);

static bool doPrepareData(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doConnect(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doReconnect(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doSendDescriptor(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doRequestSession();
static bool doSetSession(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doStartStream();
static bool doProcessData(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doStatus(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);
static bool doQuit(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq);

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
    return doRequestSession();
  case Dispatcher::Action::SetSession:
    return doSetSession(getShared(), request);
  case Dispatcher::Action::StartStream:
    return doStartStream();
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

auto Dispatcher::hashForDevice(const tkm::msg::control::DeviceData &data) -> std::string
{
  std::string tmp = data.address();
  tmp += std::to_string(data.port());
  return std::to_string(tkm::jnkHsh(tmp.c_str()));
}

void Dispatcher::resetRequestSessionTimer(void)
{
  if (m_reqSessionTimer != nullptr) {
    m_reqSessionTimer->stop();
    App()->remEventSource(m_reqSessionTimer);
    m_reqSessionTimer.reset();
  }

  App()->getSessionInfo().clear_name();
  m_reqSessionTimer = std::make_shared<Timer>("SessionCreationTimer", []() {
    if (App()->getSessionInfo().name().empty()) {
      logError() << "Create session timout. Taskmonitor not responding";
      if (App()->getConnection() != nullptr) {
        App()->remEventSource(App()->getConnection());
        App()->resetConnection();
      }
    }
    return false;
  });
  m_reqSessionTimer->start(1500000, false);
  App()->addEventSource(m_reqSessionTimer);
}

static bool doPrepareData(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  Dispatcher::Request rq;
  bool status = true;

  App()->getDeviceData().set_state(tkm::msg::control::DeviceData_State_Unknown);
  App()->getDeviceData().set_name(App()->getArguments()->getFor(Arguments::Key::Name));
  App()->getDeviceData().set_address(App()->getArguments()->getFor(Arguments::Key::Address));
  App()->getDeviceData().set_port(std::stoi(App()->getArguments()->getFor(Arguments::Key::Port)));
  App()->getDeviceData().set_hash(mgr->hashForDevice(App()->getDeviceData()));

  if (App()->getArguments()->hasFor(Arguments::Key::DatabasePath)) {
    IDatabase::Request dbInit = {.action = IDatabase::Action::InitDatabase,
                                 .bulkData = std::make_any<int>(0),
                                 .args = std::map<Defaults::Arg, std::string>()};

    if (App()->getArguments()->hasFor(Arguments::Key::Init)) {
      const std::map<Defaults::Arg, std::string> forcedArgument{
          std::make_pair<Defaults::Arg, std::string>(
              Defaults::Arg::Forced, std::string(tkmDefaults.valFor(Defaults::Val::True)))};

      dbInit.args = forcedArgument;
    }

    status = App()->getDatabase()->pushRequest(dbInit);
  }

  if (!status) {
    logError() << "Connot initialize output files";
    rq.action = Dispatcher::Action::Quit;
  } else {
    rq.action = Dispatcher::Action::Connect;
  }

  return mgr->pushRequest(rq);
}

static bool doConnect(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  Dispatcher::Request rq;

  if (App()->getConnection()->connect() < 0) {
    rq.action = Dispatcher::Action::Reconnect;
  } else {
    rq.action = Dispatcher::Action::SendDescriptor;
    App()->getConnection()->enableEvents();
  }

  return mgr->pushRequest(rq);
}

static bool doReconnect(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  Dispatcher::Request rq;

  if ((App()->getSessionInfo().hash().length() > 0) && (App()->getSessionData().ended() == 0)) {
    if (App()->getArguments()->hasFor(Arguments::Key::DatabasePath)) {
      IDatabase::Request dbrq = {.action = IDatabase::Action::EndSession,
                                 .bulkData = std::make_any<int>(0),
                                 .args = std::map<Defaults::Arg, std::string>()};
      App()->getDatabase()->pushRequest(dbrq);
    }
  }

  // Sleep before retrying
  ::sleep(3);

  App()->printVerbose("Reconnecting...");
  logInfo() << "Reconnecting to " << App()->getDeviceData().name() << " ...";

  // Stop update lanes
  App()->stopUpdateLanes();
  // Reset connection object
  App()->resetConnection();

  if (App()->getConnection()->connect() < 0) {
    rq.action = Dispatcher::Action::Reconnect;
  } else {
    rq.action = Dispatcher::Action::SendDescriptor;
    App()->getConnection()->enableEvents();
  }

  return mgr->pushRequest(rq);
}

static bool doSendDescriptor(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &)
{
  tkm::msg::collector::Descriptor descriptor;

  descriptor.set_id("Reader");
  if (!sendCollectorDescriptor(App()->getConnection()->getFD(), descriptor)) {
    logError() << "Failed to send descriptor";
    Dispatcher::Request nrq{.action = Dispatcher::Action::Reconnect,
                            .bulkData = std::make_any<int>(0),
                            .args = std::map<Defaults::Arg, std::string>()};
    return mgr->pushRequest(nrq);
  }
  logDebug() << "Sent collector descriptor";

  Dispatcher::Request nrq{.action = Dispatcher::Action::RequestSession,
                          .bulkData = std::make_any<int>(0),
                          .args = std::map<Defaults::Arg, std::string>()};
  return mgr->pushRequest(nrq);
}

static bool doRequestSession()
{
  tkm::msg::Envelope envelope;
  tkm::msg::collector::Request request;

  request.set_id("CreateSession");
  request.set_type(tkm::msg::collector::Request::Type::Request_Type_CreateSession);

  envelope.mutable_mesg()->PackFrom(request);
  envelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
  envelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

  App()->printVerbose("Request session");
  logDebug() << "Request session to monitor";

  App()->getDispatcher()->resetRequestSessionTimer();
  return App()->getConnection()->writeEnvelope(envelope);
}

static bool doSetSession(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq)
{
  const auto &sessionInfo = std::any_cast<tkm::msg::monitor::SessionInfo>(rq.bulkData);
  bool status = true;

  App()->printVerbose("Monitor accepted session with id: " + sessionInfo.hash());
  logInfo() << "Monitor accepted session with id: " << sessionInfo.hash();
  App()->getSessionInfo().CopyFrom(sessionInfo);
  App()->getSessionData().set_hash(App()->getSessionInfo().hash());

  logDebug() << "SessionInfo FastLaneInterval=" << sessionInfo.fast_lane_interval()
             << " PaceLaneInterval=" << sessionInfo.pace_lane_interval()
             << " SlowLaneInterval=" << sessionInfo.slow_lane_interval();

  Json::Value head;
  head["type"] = "session";
  head["device"] = App()->getArguments()->getFor(Arguments::Key::Name);
  head["session"] = App()->getSessionInfo().hash();
  writeJsonStream() << head;

  if (App()->getArguments()->hasFor(Arguments::Key::DatabasePath)) {
    IDatabase::Request dbReq = {.action = IDatabase::Action::AddSession,
                                .bulkData = rq.bulkData,
                                .args = std::map<Defaults::Arg, std::string>()};
    status = App()->getDatabase()->pushRequest(dbReq);
  }

  if (status) {
    Dispatcher::Request srq{.action = Dispatcher::Action::StartStream,
                            .bulkData = std::make_any<int>(0),
                            .args = std::map<Defaults::Arg, std::string>()};
    status = mgr->pushRequest(srq);
  }

  return status;
}

static bool doStartStream()
{
  App()->printVerbose("Reading data started for session: " + App()->getSessionInfo().hash());
  logInfo() << "Reading data started for session: " << App()->getSessionInfo().hash();

  App()->requestStartupData();
  App()->startUpdateLanes();

  // We use the fastLaneInterval for connection inactivity monitor timer
  const auto inactivityInterval = App()->getSessionInfo().fast_lane_interval();
  App()->resetInactivityTimer(inactivityInterval);

  return true;
}

static bool doProcessData(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq)
{
  const auto &data = std::any_cast<tkm::msg::monitor::Data>(rq.bulkData);

  static_cast<void>(mgr); // UNUSED

  switch (data.what()) {
  case tkm::msg::monitor::Data_What_ProcAcct: {
    tkm::msg::monitor::ProcAcct procAcct;
    data.payload().UnpackTo(&procAcct);
    printProcAcct(procAcct, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_ProcInfo: {
    tkm::msg::monitor::ProcInfo procInfo;
    data.payload().UnpackTo(&procInfo);
    printProcInfo(procInfo, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_ProcEvent: {
    tkm::msg::monitor::ProcEvent procEvent;
    data.payload().UnpackTo(&procEvent);
    printProcEvent(procEvent, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_ContextInfo: {
    tkm::msg::monitor::ContextInfo ctxInfo;
    data.payload().UnpackTo(&ctxInfo);
    printContextInfo(ctxInfo, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcStat: {
    tkm::msg::monitor::SysProcStat sysProcStat;
    data.payload().UnpackTo(&sysProcStat);
    printSysProcStat(sysProcStat, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcMemInfo: {
    tkm::msg::monitor::SysProcMemInfo sysProcMemInfo;
    data.payload().UnpackTo(&sysProcMemInfo);
    printSysProcMemInfo(sysProcMemInfo, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcDiskStats: {
    tkm::msg::monitor::SysProcDiskStats sysProcDiskStats;
    data.payload().UnpackTo(&sysProcDiskStats);
    printSysProcDiskStats(sysProcDiskStats, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcPressure: {
    tkm::msg::monitor::SysProcPressure sysProcPressure;
    data.payload().UnpackTo(&sysProcPressure);
    printSysProcPressure(sysProcPressure, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcBuddyInfo: {
    tkm::msg::monitor::SysProcBuddyInfo sysProcBuddyInfo;
    data.payload().UnpackTo(&sysProcBuddyInfo);
    printSysProcBuddyInfo(sysProcBuddyInfo, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcWireless: {
    tkm::msg::monitor::SysProcWireless sysProcWireless;
    data.payload().UnpackTo(&sysProcWireless);
    printSysProcWireless(sysProcWireless, data.system_time_sec(), data.monotonic_time_sec());
    break;
  }
  default:
    break;
  }

  if (App()->getArguments()->hasFor(Arguments::Key::DatabasePath)) {
    IDatabase::Request dbReq = {.action = IDatabase::Action::AddData,
                                .bulkData = rq.bulkData,
                                .args = std::map<Defaults::Arg, std::string>()};
    return App()->getDatabase()->pushRequest(dbReq);
  }

  return true;
}

static bool doStatus(const std::shared_ptr<Dispatcher> mgr, const Dispatcher::Request &rq)
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

static bool doQuit(const std::shared_ptr<Dispatcher>, const Dispatcher::Request &)
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

static void
printProcInfo(const tkm::msg::monitor::ProcInfo &info, uint64_t systemTime, uint64_t monotonicTime)
{
  Json::Value head;
  Json::Value body;

  head["type"] = "procinfo";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  for (const auto &procEntry : info.entry()) {
    Json::Value entry;
    entry["comm"] = procEntry.comm();
    entry["pid"] = procEntry.pid();
    entry["ppid"] = procEntry.ppid();
    entry["ctx_id"] = procEntry.ctx_id();
    entry["ctx_name"] = procEntry.ctx_name();
    entry["cpu_time"] = procEntry.cpu_time();
    entry["cpu_percent"] = procEntry.cpu_percent();
    entry["mem_rss"] = procEntry.mem_rss();
    entry["mem_pss"] = procEntry.mem_pss();
    head[std::to_string(procEntry.pid())] = entry;
  }

  writeJsonStream() << head;
}

static void printContextInfo(const tkm::msg::monitor::ContextInfo &info,
                             uint64_t systemTime,
                             uint64_t monotonicTime)
{
  Json::Value head;
  Json::Value body;

  head["type"] = "ctxinfo";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  for (const auto &ctxEntry : info.entry()) {
    Json::Value entry;
    entry["ctx_id"] = ctxEntry.ctx_id();
    entry["ctx_name"] = ctxEntry.ctx_name();
    entry["total_cpu_time"] = ctxEntry.total_cpu_time();
    entry["total_cpu_percent"] = ctxEntry.total_cpu_percent();
    entry["total_mem_rss"] = ctxEntry.total_mem_rss();
    entry["total_mem_pss"] = ctxEntry.total_mem_pss();
    head[ctxEntry.ctx_name()] = entry;
  }

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
  cpu["all"] = sysProcStat.cpu().all();
  cpu["usr"] = sysProcStat.cpu().usr();
  cpu["sys"] = sysProcStat.cpu().sys();
  cpu["iow"] = sysProcStat.cpu().iow();
  head["cpu"] = cpu;

  for (const auto &cpuCore : sysProcStat.core()) {
    Json::Value core;
    core["all"] = cpuCore.all();
    core["usr"] = cpuCore.usr();
    core["sys"] = cpuCore.sys();
    core["iow"] = cpuCore.iow();
    head[cpuCore.name()] = core;
  }

  writeJsonStream() << head;
}

static void printSysProcBuddyInfo(const tkm::msg::monitor::SysProcBuddyInfo &sysProcBuddyInfo,
                                  uint64_t systemTime,
                                  uint64_t monotonicTime)
{
  Json::Value head;

  head["type"] = "buddyinfo";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  auto index = 0;
  for (const auto &nodeEntry : sysProcBuddyInfo.node()) {
    Json::Value node;
    node["name"] = nodeEntry.name();
    node["zone"] = nodeEntry.zone();
    node["data"] = nodeEntry.data();
    head[std::to_string(index++)] = node;
  }

  writeJsonStream() << head;
}

static void printSysProcWireless(const tkm::msg::monitor::SysProcWireless &sysProcWireless,
                                 uint64_t systemTime,
                                 uint64_t monotonicTime)
{
  Json::Value head;

  head["type"] = "wireless";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  auto index = 0;
  for (const auto &ifw : sysProcWireless.ifw()) {
    Json::Value node;
    node["name"] = ifw.name();
    node["status"] = ifw.status();
    node["quality_link"] = ifw.quality_link();
    node["quality_level"] = ifw.quality_level();
    node["quality_noise"] = ifw.quality_noise();
    node["discarded_nwid"] = ifw.discarded_nwid();
    node["discarded_crypt"] = ifw.discarded_crypt();
    node["discarded_frag"] = ifw.discarded_frag();
    node["discarded_retry"] = ifw.discarded_retry();
    node["discarded_misc"] = ifw.discarded_misc();
    node["missed_beacon"] = ifw.missed_beacon();
    head[std::to_string(index++)] = node;
  }

  writeJsonStream() << head;
}

static void printSysProcMemInfo(const tkm::msg::monitor::SysProcMemInfo &sysProcMemInfo,
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
  meminfo["mem_total"] = sysProcMemInfo.mem_total();
  meminfo["mem_free"] = sysProcMemInfo.mem_free();
  meminfo["mem_available"] = sysProcMemInfo.mem_available();
  meminfo["mem_cached"] = sysProcMemInfo.mem_cached();
  meminfo["mem_available_percent"] = sysProcMemInfo.mem_percent();
  meminfo["active"] = sysProcMemInfo.active();
  meminfo["inactive"] = sysProcMemInfo.inactive();
  meminfo["slab"] = sysProcMemInfo.slab();
  meminfo["kreclaimable"] = sysProcMemInfo.kreclaimable();
  meminfo["sreclaimable"] = sysProcMemInfo.sreclaimable();
  meminfo["sunreclaim"] = sysProcMemInfo.sunreclaim();
  meminfo["kernel_stack"] = sysProcMemInfo.kernel_stack();
  meminfo["swap_total"] = sysProcMemInfo.swap_total();
  meminfo["swap_free"] = sysProcMemInfo.swap_free();
  meminfo["swap_cached"] = sysProcMemInfo.swap_cached();
  meminfo["swap_free_percent"] = sysProcMemInfo.swap_percent();
  head["meminfo"] = meminfo;

  writeJsonStream() << head;
}

static void printSysProcDiskStats(const tkm::msg::monitor::SysProcDiskStats &sysProcDiskStats,
                                  uint64_t systemTime,
                                  uint64_t monotonicTime)
{
  Json::Value head;

  head["type"] = "diskstats";
  head["system_time"] = systemTime;
  head["monotonic_time"] = monotonicTime;
  head["receive_time"] = ::time(NULL);
  head["session"] = App()->getSessionInfo().hash();

  for (const auto &diskEntry : sysProcDiskStats.disk()) {
    Json::Value entry;
    entry["major"] = diskEntry.node_major();
    entry["minor"] = diskEntry.node_minor();
    entry["name"] = diskEntry.name();
    entry["reads_completed"] = diskEntry.reads_completed();
    entry["reads_merged"] = diskEntry.reads_merged();
    entry["reads_spent_ms"] = diskEntry.reads_spent_ms();
    entry["writes_completed"] = diskEntry.writes_completed();
    entry["writes_merged"] = diskEntry.writes_merged();
    entry["writes_spent_ms"] = diskEntry.writes_spent_ms();
    entry["io_in_progress"] = diskEntry.io_in_progress();
    entry["io_spent_ms"] = diskEntry.io_spent_ms();
    entry["io_weighted_ms"] = diskEntry.io_weighted_ms();
    head[diskEntry.name()] = entry;
  }
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
