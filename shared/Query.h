/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Query class
 * @details   Provide SQL query strings per database type
 *-
 */

#pragma once

#include <map>
#include <sstream>
#include <string>

#include "Monitor.pb.h"

namespace tkm
{

class Query
{
public:
  enum class Type { SQLite3, PostgreSQL };

  auto createTables(Query::Type type) -> std::string;
  auto dropTables(Query::Type type) -> std::string;

  // Device management
  auto getDevices(Query::Type type) -> std::string;
  auto addDevice(Query::Type type,
                 const std::string &hash,
                 const std::string &name,
                 const std::string &address,
                 int32_t port) -> std::string;
  auto remDevice(Query::Type type, const std::string &hash) -> std::string;
  auto getDevice(Query::Type type, const std::string &hash) -> std::string;
  auto hasDevice(Query::Type type, const std::string &hash) -> std::string;

  // Session management
  auto getSessions(Query::Type type) -> std::string;
  auto getSessions(Query::Type type, const std::string &deviceHash) -> std::string;
  auto addSession(Query::Type type,
                  const std::string &hash,
                  const std::string &name,
                  uint64_t start_timestamp,
                  const std::string &deviceHash) -> std::string;
  auto endSession(Query::Type type, const std::string &hash) -> std::string;
  auto remSession(Query::Type type, const std::string &hash) -> std::string;
  auto getSession(Query::Type type, const std::string &hash) -> std::string;
  auto hasSession(Query::Type type, const std::string &hash) -> std::string;

  // Add device data
  auto addData(Query::Type type,
               const std::string &sessionHash,
               const tkm::msg::monitor::SysProcStat &sysProcStat,
               uint64_t systemTime,
               uint64_t monotonicTime,
               uint64_t receiveTime) -> std::string;
  auto addData(Query::Type type,
               const std::string &sessionHash,
               const tkm::msg::monitor::SysProcMeminfo &sysProcMem,
               uint64_t systemTime,
               uint64_t monotonicTime,
               uint64_t receiveTime) -> std::string;
  auto addData(Query::Type type,
               const std::string &sessionHash,
               const tkm::msg::monitor::SysProcPressure &sysProcPressure,
               uint64_t systemTime,
               uint64_t monotonicTime,
               uint64_t receiveTime) -> std::string;
  auto addData(Query::Type type,
               const std::string &sessionHash,
               const tkm::msg::monitor::ProcAcct &procAcct,
               uint64_t systemTime,
               uint64_t monotonicTime,
               uint64_t receiveTime) -> std::string;
  auto addData(Query::Type type,
               const std::string &sessionHash,
               const tkm::msg::monitor::ProcEvent &procEvent,
               uint64_t systemTime,
               uint64_t monotonicTime,
               uint64_t receiveTime) -> std::string;

public:
  enum class DeviceColumn {
    Id,      // int: Primary key
    Hash,    // str: Unique device hash
    Name,    // str: Device name
    Address, // str: Device address
    Port,    // int: Device port
  };
  const std::map<DeviceColumn, std::string> m_deviceColumn{
      std::make_pair(DeviceColumn::Id, "Id"),
      std::make_pair(DeviceColumn::Hash, "Hash"),
      std::make_pair(DeviceColumn::Name, "Name"),
      std::make_pair(DeviceColumn::Address, "Address"),
      std::make_pair(DeviceColumn::Port, "Port"),
  };

  enum class SessionColumn {
    Id,             // int: Primary key
    Name,           // str: Device name
    Hash,           // str: Unique device hash
    StartTimestamp, // int: Start timestamp
    EndTimestamp,   // int: End timestamp
    Device,         // int: Device id key
  };
  const std::map<SessionColumn, std::string> m_sessionColumn{
      std::make_pair(SessionColumn::Id, "Id"),
      std::make_pair(SessionColumn::Hash, "Hash"),
      std::make_pair(SessionColumn::Name, "Name"),
      std::make_pair(SessionColumn::StartTimestamp, "StartTimestamp"),
      std::make_pair(SessionColumn::EndTimestamp, "EndTimestamp"),
      std::make_pair(SessionColumn::Device, "Device"),
  };

  enum class ProcEventColumn {
    Id,            // int: Primary key
    SystemTime,    // int: SystemTime
    MonotonicTime, // int: MonotonicTime
    ReceiveTime,   // int: Receive timestamp
    ForkCount,     // int: fork_count
    ExecCount,     // int: exec_count
    ExitCount,     // int: exit_count
    UIdCount,      // int: uid_count
    GIdCount,      // int: gid_count
    SessionId,     // int: Session id key
  };
  const std::map<ProcEventColumn, std::string> m_procEventColumn{
      std::make_pair(ProcEventColumn::Id, "Id"),
      std::make_pair(ProcEventColumn::SystemTime, "SystemTime"),
      std::make_pair(ProcEventColumn::MonotonicTime, "MonotonicTime"),
      std::make_pair(ProcEventColumn::ReceiveTime, "ReceiveTime"),
      std::make_pair(ProcEventColumn::ForkCount, "ForkCount"),
      std::make_pair(ProcEventColumn::ExecCount, "ExecCount"),
      std::make_pair(ProcEventColumn::ExitCount, "ExitCount"),
      std::make_pair(ProcEventColumn::UIdCount, "UIdCount"),
      std::make_pair(ProcEventColumn::GIdCount, "GIdCount"),
      std::make_pair(ProcEventColumn::SessionId, "SessionId"),
  };

  enum class SysProcStatColumn {
    Id,            // int: Primary key
    SystemTime,    // int: SystemTime
    MonotonicTime, // int: MonotonicTime
    ReceiveTime,   // int: Receive timestamp
    CPUStatName,   // str: CPUStat.name
    CPUStatAll,    // int: CPUStat.all
    CPUStatUsr,    // int: CPUStat.usr
    CPUStatSys,    // int: CPUStat.sys
    SessionId,     // int: Session id key
  };
  const std::map<SysProcStatColumn, std::string> m_sysProcStatColumn{
      std::make_pair(SysProcStatColumn::Id, "Id"),
      std::make_pair(SysProcStatColumn::SystemTime, "SystemTime"),
      std::make_pair(SysProcStatColumn::MonotonicTime, "MonotonicTime"),
      std::make_pair(SysProcStatColumn::ReceiveTime, "ReceiveTime"),
      std::make_pair(SysProcStatColumn::CPUStatName, "CPUStatName"),
      std::make_pair(SysProcStatColumn::CPUStatAll, "CPUStatAll"),
      std::make_pair(SysProcStatColumn::CPUStatUsr, "CPUStatUsr"),
      std::make_pair(SysProcStatColumn::CPUStatSys, "CPUStatSys"),
      std::make_pair(SysProcStatColumn::SessionId, "SessionId"),
  };

  enum class SysProcMemColumn {
    Id,              // int: Primary key
    SystemTime,      // int: SystemTime
    MonotonicTime,   // int: MonotonicTime
    ReceiveTime,     // int: Receive timestamp
    MemTotal,        // int: MemTotal
    MemFree,         // int: MemFree
    MemAvail,        // int: MemAvail
    MemCached,       // int: MemCached
    MemAvailPercent, // int: MemAvailPercent
    SwapTotal,       // int: SwapTotal
    SwapFree,        // int: SwapFree
    SwapCached,      // int: SwapCached
    SwapFreePercent, // int: SwapFreePercent
    SessionId,       // int: Session id key
  };
  const std::map<SysProcMemColumn, std::string> m_sysProcMemColumn{
      std::make_pair(SysProcMemColumn::Id, "Id"),
      std::make_pair(SysProcMemColumn::SystemTime, "SystemTime"),
      std::make_pair(SysProcMemColumn::MonotonicTime, "MonotonicTime"),
      std::make_pair(SysProcMemColumn::ReceiveTime, "ReceiveTime"),
      std::make_pair(SysProcMemColumn::MemTotal, "MemTotal"),
      std::make_pair(SysProcMemColumn::MemFree, "MemFree"),
      std::make_pair(SysProcMemColumn::MemAvail, "MemAvail"),
      std::make_pair(SysProcMemColumn::MemCached, "MemCached"),
      std::make_pair(SysProcMemColumn::MemAvailPercent, "MemAvailPercent"),
      std::make_pair(SysProcMemColumn::SwapTotal, "SwapTotal"),
      std::make_pair(SysProcMemColumn::SwapFree, "SwapFree"),
      std::make_pair(SysProcMemColumn::SwapCached, "SwapCached"),
      std::make_pair(SysProcMemColumn::SwapFreePercent, "SwapFreePercent"),
      std::make_pair(SysProcMemColumn::SessionId, "SessionId"),
  };

  enum class SysProcPressureColumn {
    Id,            // int: Primary key
    SystemTime,    // int: SystemTime
    MonotonicTime, // int: MonotonicTime
    ReceiveTime,   // int: Receive timestamp
    CPUSomeAvg10,  // float: cpu_some_avg10
    CPUSomeAvg60,  // float: cpu_some_avg60
    CPUSomeAvg300, // float: cpu_some_avg300
    CPUSomeTotal,  // float: cpu_some_total
    CPUFullAvg10,  // float: cpu_full_avg10
    CPUFullAvg60,  // float: cpu_full_avg60
    CPUFullAvg300, // float: cpu_full_avg300
    CPUFullTotal,  // float: cpu_full_total
    MEMSomeAvg10,  // float: mem_some_avg10
    MEMSomeAvg60,  // float: mem_some_avg60
    MEMSomeAvg300, // float: mem_some_avg300
    MEMSomeTotal,  // float: mem_some_total
    MEMFullAvg10,  // float: mem_full_avg10
    MEMFullAvg60,  // float: mem_full_avg60
    MEMFullAvg300, // float: mem_full_avg300
    MEMFullTotal,  // float: mem_full_total
    IOSomeAvg10,   // float: io_some_avg10
    IOSomeAvg60,   // float: io_some_avg60
    IOSomeAvg300,  // float: io_some_avg300
    IOSomeTotal,   // float: io_some_total
    IOFullAvg10,   // float: io_full_avg10
    IOFullAvg60,   // float: io_full_avg60
    IOFullAvg300,  // float: io_full_avg300
    IOFullTotal,   // float: io_full_total
    SessionId,     // int: Session id key
  };
  const std::map<SysProcPressureColumn, std::string> m_sysProcPressureColumn{
      std::make_pair(SysProcPressureColumn::Id, "Id"),
      std::make_pair(SysProcPressureColumn::SystemTime, "SystemTime"),
      std::make_pair(SysProcPressureColumn::MonotonicTime, "MonotonicTime"),
      std::make_pair(SysProcPressureColumn::ReceiveTime, "ReceiveTime"),
      std::make_pair(SysProcPressureColumn::CPUSomeAvg10, "CPUSomeAvg10"),
      std::make_pair(SysProcPressureColumn::CPUSomeAvg60, "CPUSomeAvg60"),
      std::make_pair(SysProcPressureColumn::CPUSomeAvg300, "CPUSomeAvg300"),
      std::make_pair(SysProcPressureColumn::CPUSomeTotal, "CPUSomeTotal"),
      std::make_pair(SysProcPressureColumn::CPUFullAvg10, "CPUFullAvg10"),
      std::make_pair(SysProcPressureColumn::CPUFullAvg60, "CPUFullAvg60"),
      std::make_pair(SysProcPressureColumn::CPUFullAvg300, "CPUFullAvg300"),
      std::make_pair(SysProcPressureColumn::CPUFullTotal, "CPUFullTotal"),
      std::make_pair(SysProcPressureColumn::MEMSomeAvg10, "MEMSomeAvg10"),
      std::make_pair(SysProcPressureColumn::MEMSomeAvg60, "MEMSomeAvg60"),
      std::make_pair(SysProcPressureColumn::MEMSomeAvg300, "MEMSomeAvg300"),
      std::make_pair(SysProcPressureColumn::MEMSomeTotal, "MEMSomeTotal"),
      std::make_pair(SysProcPressureColumn::MEMFullAvg10, "MEMFullAvg10"),
      std::make_pair(SysProcPressureColumn::MEMFullAvg60, "MEMFullAvg60"),
      std::make_pair(SysProcPressureColumn::MEMFullAvg300, "MEMFullAvg300"),
      std::make_pair(SysProcPressureColumn::MEMFullTotal, "MEMFullTotal"),
      std::make_pair(SysProcPressureColumn::IOSomeAvg10, "IOSomeAvg10"),
      std::make_pair(SysProcPressureColumn::IOSomeAvg60, "IOSomeAvg60"),
      std::make_pair(SysProcPressureColumn::IOSomeAvg300, "IOSomeAvg300"),
      std::make_pair(SysProcPressureColumn::IOSomeTotal, "IOSomeTotal"),
      std::make_pair(SysProcPressureColumn::IOFullAvg10, "IOFullAvg10"),
      std::make_pair(SysProcPressureColumn::IOFullAvg60, "IOFullAvg60"),
      std::make_pair(SysProcPressureColumn::IOFullAvg300, "IOFullAvg300"),
      std::make_pair(SysProcPressureColumn::IOFullTotal, "IOFullTotal"),
      std::make_pair(SysProcPressureColumn::SessionId, "SessionId"),
  };

  enum class ProcAcctColumn {
    Id,                    // int: Primary key
    SystemTime,            // int: SystemTime
    MonotonicTime,         // int: MonotonicTime
    ReceiveTime,           // int: Receive timestamp
    AcComm,                // str: ac_comm
    AcUid,                 // int: ac_uid
    AcGid,                 // int: ac_gid
    AcPid,                 // int: ac_pid
    AcPPid,                // int: ac_ppid
    AcUTime,               // int: ac_utime
    AcSTime,               // int: ac_stime
    CpuCount,              // int: cpu_count
    CpuRunRealTotal,       // int: cpu_run_real_total
    CpuRunVirtualTotal,    // int: cpu_run_virtual_total
    CpuDelayTotal,         // int: cpu_delay_total
    CpuDelayAverage,       // int: cpu_delay_average
    CoreMem,               // int: coremem
    VirtMem,               // int: virtmem
    HiwaterRss,            // int: hiwater_rss
    HiwaterVm,             // int: hiwater_vm
    Nvcsw,                 // int: nvcsw
    Nivcsw,                // int: nivcsw
    SwapinCount,           // int: swapin_count
    SwapinDelayTotal,      // int: swapin_delay_total
    SwapinDelayAverage,    // int: swapin_delay_average
    BlkIOCount,            // int: blkio_count
    BlkIODelayTotal,       // int: blkio_delay_total
    BlkIODelayAverage,     // int: blkio_delay_average
    IOStorageReadBytes,    // int: Storage read bytes
    IOStorageWriteBytes,   // int: Storage write bytes
    IOReadChar,            // int: IO read bytes
    IOWriteChar,           // int: IO write bytes
    IOReadSyscalls,        // int: IO read syscalls
    IOWriteSyscalls,       // int: IO write syscalls
    FreePagesCount,        // int: freepages_count
    FreePagesDelayTotal,   // int: freepages_delay_total
    FreePagesDelayAverage, // int: freepages_delay_average
    ThrashingCount,        // int: thrashing_count
    ThrashingDelayTotal,   // int: thrashing_delay_total
    ThrashingDelayAverage, // int: thrashing_delay_average
    SessionId,             // int: Session id key
  };
  const std::map<ProcAcctColumn, std::string> m_procAcctColumn{
      std::make_pair(ProcAcctColumn::Id, "Id"),
      std::make_pair(ProcAcctColumn::SystemTime, "SystemTime"),
      std::make_pair(ProcAcctColumn::MonotonicTime, "MonotonicTime"),
      std::make_pair(ProcAcctColumn::ReceiveTime, "ReceiveTime"),
      std::make_pair(ProcAcctColumn::AcComm, "AcComm"),
      std::make_pair(ProcAcctColumn::AcUid, "AcUid"),
      std::make_pair(ProcAcctColumn::AcGid, "AcGid"),
      std::make_pair(ProcAcctColumn::AcPid, "AcPid"),
      std::make_pair(ProcAcctColumn::AcPPid, "AcPPid"),
      std::make_pair(ProcAcctColumn::AcUTime, "AcUTime"),
      std::make_pair(ProcAcctColumn::AcSTime, "AcSTime"),
      std::make_pair(ProcAcctColumn::CpuCount, "CpuCount"),
      std::make_pair(ProcAcctColumn::CpuRunRealTotal, "CpuRunRealTotal"),
      std::make_pair(ProcAcctColumn::CpuRunVirtualTotal, "CpuRunVirtualTotal"),
      std::make_pair(ProcAcctColumn::CpuDelayTotal, "CpuDelayTotal"),
      std::make_pair(ProcAcctColumn::CpuDelayAverage, "CpuDelayAverage"),
      std::make_pair(ProcAcctColumn::CoreMem, "CoreMem"),
      std::make_pair(ProcAcctColumn::VirtMem, "VirtMem"),
      std::make_pair(ProcAcctColumn::HiwaterRss, "HiwaterRss"),
      std::make_pair(ProcAcctColumn::HiwaterVm, "HiwaterVm"),
      std::make_pair(ProcAcctColumn::Nvcsw, "Nvcsw"),
      std::make_pair(ProcAcctColumn::Nivcsw, "Nivcsw"),
      std::make_pair(ProcAcctColumn::SwapinCount, "SwapinCount"),
      std::make_pair(ProcAcctColumn::SwapinDelayTotal, "SwapinDelayTotal"),
      std::make_pair(ProcAcctColumn::SwapinDelayAverage, "SwapinDelayAverage"),
      std::make_pair(ProcAcctColumn::BlkIOCount, "BlkIOCount"),
      std::make_pair(ProcAcctColumn::BlkIODelayTotal, "BlkIODelayTotal"),
      std::make_pair(ProcAcctColumn::BlkIODelayAverage, "BlkIODelayAverage"),
      std::make_pair(ProcAcctColumn::IOStorageReadBytes, "IOStorageReadBytes"),
      std::make_pair(ProcAcctColumn::IOStorageWriteBytes, "IOStorageWriteBytes"),
      std::make_pair(ProcAcctColumn::IOReadChar, "IOReadChar"),
      std::make_pair(ProcAcctColumn::IOWriteChar, "IOWriteChar"),
      std::make_pair(ProcAcctColumn::IOReadSyscalls, "IOReadSyscalls"),
      std::make_pair(ProcAcctColumn::IOWriteSyscalls, "IOWriteSyscalls"),
      std::make_pair(ProcAcctColumn::FreePagesCount, "FreePagesCount"),
      std::make_pair(ProcAcctColumn::FreePagesDelayTotal, "FreePagesDelayTotal"),
      std::make_pair(ProcAcctColumn::FreePagesDelayAverage, "FreePagesDelayAverage"),
      std::make_pair(ProcAcctColumn::ThrashingCount, "ThrashingCount"),
      std::make_pair(ProcAcctColumn::ThrashingDelayTotal, "ThrashingDelayTotal"),
      std::make_pair(ProcAcctColumn::ThrashingDelayAverage, "ThrashingDelayAverage"),
      std::make_pair(ProcAcctColumn::SessionId, "SessionId"),
  };

  const std::string m_devicesTableName = "tkmDevices";
  const std::string m_sessionsTableName = "tkmSessions";
  const std::string m_sysProcStatTableName = "tkmSysProcStat";
  const std::string m_sysProcMeminfoTableName = "tkmSysProcMeminfo";
  const std::string m_sysProcPressureTableName = "tkmSysProcPressure";
  const std::string m_procAcctTableName = "tkmProcAcct";
  const std::string m_procEventTableName = "tkmProcEvent";
};

static Query tkmQuery{};

} // namespace tkm