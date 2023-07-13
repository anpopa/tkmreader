/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     SQLiteDatabase Class
 * @details   SQLite3 database implementation
 *-
 */

#include "SQLiteDatabase.h"
#include "Application.h"
#include "Arguments.h"
#include "Defaults.h"
#include "IDatabase.h"
#include "IEventSource.h"
#include "Query.h"

#include <any>
#include <filesystem>
#include <string>
#include <taskmonitor/taskmonitor.h>
#include <vector>

using std::shared_ptr;
using std::string;
namespace fs = std::filesystem;

namespace tkm::reader
{

static auto sqlite_callback(void *data, int argc, char **argv, char **colname) -> int;
static bool doCheckDatabase(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq);
static bool doInitDatabase(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq);
static bool doAddDevice(const shared_ptr<SQLiteDatabase> db);
static bool doConnect(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq);
static bool doDisconnect(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq);
static bool doAddSession(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq);
static bool doEndSession(const shared_ptr<SQLiteDatabase> db);
static bool doAddData(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq);

SQLiteDatabase::SQLiteDatabase(void)
: IDatabase()
{
  fs::path addr(App()->getArguments()->getFor(Arguments::Key::DatabasePath));
  logDebug() << "Using DB file: " << addr.string();

  if (sqlite3_open(addr.c_str(), &m_db) != SQLITE_OK) {
    logWarn() << "SQLite3 database file invalid. Force reinit..."
              << App()->getArguments()->getFor(Arguments::Key::DatabasePath);
    std::filesystem::remove(App()->getArguments()->getFor(Arguments::Key::DatabasePath));

    if (sqlite3_open(addr.c_str(), &m_db) != SQLITE_OK) {
      sqlite3_close(m_db);
      throw std::runtime_error(sqlite3_errmsg(m_db));
    }
  }
}

SQLiteDatabase::~SQLiteDatabase()
{
  sqlite3_close(m_db);
}

void SQLiteDatabase::enableEvents()
{
  // We might need to handle db request first before
  // a new connection update shared device or session data
  App()->addEventSource(m_queue, IEventSource::Priority::High);

  IDatabase::Request dbrq{.action = IDatabase::Action::CheckDatabase,
                          .bulkData = std::make_any<int>(0),
                          .args = std::map<Defaults::Arg, std::string>()};
  pushRequest(dbrq);
}

bool SQLiteDatabase::runQuery(const std::string &sql, SQLiteDatabase::Query &query)
{
  char *queryError = nullptr;

  if (::sqlite3_exec(m_db, sql.c_str(), sqlite_callback, &query, &queryError) != SQLITE_OK) {
    logError() << "SQLiteDatabase query error: " << queryError;
    sqlite3_free(queryError);
    return false;
  }

  return true;
}

static auto sqlite_callback(void *data, int argc, char **argv, char **colname) -> int
{
  auto *query = static_cast<SQLiteDatabase::Query *>(data);

  switch (query->type) {
  case SQLiteDatabase::QueryType::Check:
  case SQLiteDatabase::QueryType::Create:
  case SQLiteDatabase::QueryType::DropTables:
  case SQLiteDatabase::QueryType::AddDevice:
  case SQLiteDatabase::QueryType::RemDevice:
  case SQLiteDatabase::QueryType::AddSession:
  case SQLiteDatabase::QueryType::RemSession:
  case SQLiteDatabase::QueryType::EndSession:
  case SQLiteDatabase::QueryType::AddData:
  case SQLiteDatabase::QueryType::HasDevice: {
    auto pld = static_cast<int *>(query->raw);
    for (int i = 0; i < argc; i++) {
      if (strncmp(colname[i], tkmQuery.m_deviceColumn.at(Query::DeviceColumn::Id).c_str(), 60) ==
          0) {
        *pld = static_cast<int>(std::stol(argv[i]));
      }
    }
    break;
  }
  case SQLiteDatabase::QueryType::HasSession: {
    auto pld = static_cast<int *>(query->raw);
    for (int i = 0; i < argc; i++) {
      if (strncmp(colname[i], tkmQuery.m_sessionColumn.at(Query::SessionColumn::Id).c_str(), 60) ==
          0) {
        *pld = static_cast<int>(std::stol(argv[i]));
      }
    }
    break;
  }
  default:
    logError() << "Unknown query type";
    break;
  }

  return 0;
}

bool SQLiteDatabase::requestHandler(const Request &rq)
{
  switch (rq.action) {
  case IDatabase::Action::CheckDatabase:
    return doCheckDatabase(getShared(), rq);
  case IDatabase::Action::InitDatabase:
    return doInitDatabase(getShared(), rq);
  case IDatabase::Action::Connect:
    return doConnect(getShared(), rq);
  case IDatabase::Action::Disconnect:
    return doDisconnect(getShared(), rq);
  case IDatabase::Action::AddDevice:
    return doAddDevice(getShared());
  case IDatabase::Action::AddSession:
    return doAddSession(getShared(), rq);
  case IDatabase::Action::EndSession:
    return doEndSession(getShared());
  case IDatabase::Action::AddData:
    return doAddData(getShared(), rq);
  default:
    break;
  }
  logError() << "Unknown action request";
  return false;
}

static bool doCheckDatabase(const shared_ptr<SQLiteDatabase> db, const SQLiteDatabase::Request &rq)
{
  // TODO: Handle database check
  static_cast<void>(db); // UNUSED
  static_cast<void>(rq); // UNUSED
  return true;
}

static bool doInitDatabase(const shared_ptr<SQLiteDatabase> db, const SQLiteDatabase::Request &rq)
{
  if (rq.args.count(Defaults::Arg::Forced)) {
    if (rq.args.at(Defaults::Arg::Forced) == tkmDefaults.valFor(Defaults::Val::True)) {
      SQLiteDatabase::Query query{.type = SQLiteDatabase::QueryType::DropTables, .raw = nullptr};
      db->runQuery(tkmQuery.dropTables(Query::Type::SQLite3), query);
    }
  }

  SQLiteDatabase::Query createQuery{.type = SQLiteDatabase::QueryType::Create, .raw = nullptr};
  auto status = db->runQuery(tkmQuery.createTables(Query::Type::SQLite3), createQuery);

  if (!status) {
    logError() << "Database init failed. Query error";
  } else {
    IDatabase::Request dbReq = {.action = IDatabase::Action::AddDevice,
                                .bulkData = std::make_any<int>(0),
                                .args = std::map<Defaults::Arg, std::string>()};
    status = db->pushRequest(dbReq);
  }

  return status;
}

static bool doAddDevice(const shared_ptr<SQLiteDatabase> db)
{
  tkm::msg::control::DeviceData deviceData;
  auto devId = -1;

  SQLiteDatabase::Query queryCheckExisting{.type = SQLiteDatabase::QueryType::HasDevice,
                                           .raw = nullptr};
  queryCheckExisting.raw = &devId;
  auto status = db->runQuery(
      tkmQuery.hasDevice(Query::Type::SQLite3, App()->getDeviceData().hash()), queryCheckExisting);
  if (status) {
    SQLiteDatabase::Query query{.type = SQLiteDatabase::QueryType::RemDevice, .raw = nullptr};
    db->runQuery(tkmQuery.remDevice(Query::Type::SQLite3, App()->getDeviceData().hash()), query);
  }

  SQLiteDatabase::Query query{.type = SQLiteDatabase::QueryType::AddDevice, .raw = nullptr};
  status = db->runQuery(tkmQuery.addDevice(Query::Type::SQLite3,
                                           App()->getDeviceData().hash(),
                                           App()->getDeviceData().name(),
                                           App()->getDeviceData().address(),
                                           App()->getDeviceData().port()),
                        query);
  if (!status) {
    logError() << "Failed to add device";
  }

  return status;
}

static bool doAddSession(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq)
{
  const auto &sessionInfo = std::any_cast<tkm::msg::monitor::SessionInfo>(rq.bulkData);

  auto sesId = -1;
  SQLiteDatabase::Query queryCheckExisting{.type = SQLiteDatabase::QueryType::HasSession,
                                           .raw = &sesId};

  auto status =
      db->runQuery(tkmQuery.hasSession(Query::Type::SQLite3, App()->getSessionData().hash()),
                   queryCheckExisting);
  if (status) {
    if (sesId != -1) {
      logError() << "Session hash collision detected. Remove old session "
                 << App()->getSessionData().hash();
      SQLiteDatabase::Query query{.type = SQLiteDatabase::QueryType::RemSession, .raw = nullptr};
      status = db->runQuery(
          tkmQuery.remSession(Query::Type::SQLite3, App()->getSessionData().hash()), query);
      if (!status) {
        logError() << "Failed to remove existing session";
      }
    }
  } else {
    logError() << "Failed to check existing session";
  }

  auto currentTime = static_cast<uint64_t>(::time(NULL));

  SQLiteDatabase::Query query{.type = SQLiteDatabase::QueryType::AddSession, .raw = nullptr};
  status = db->runQuery(
      tkmQuery.addSession(
          Query::Type::SQLite3, sessionInfo, App()->getDeviceData().hash(), currentTime),
      query);
  if (!status) {
    logError() << "Query failed to add session";
  } else {
    App()->getSessionData().set_started(currentTime);
    App()->getSessionData().set_ended(0);
  }

  return status;
}

static bool doEndSession(const shared_ptr<SQLiteDatabase> db)
{
  logInfo() << "Mark end for session id: " << App()->getSessionData().hash();
  SQLiteDatabase::Query query{.type = SQLiteDatabase::QueryType::EndSession, .raw = nullptr};
  auto status = db->runQuery(
      tkmQuery.endSession(Query::Type::SQLite3, App()->getSessionData().hash()), query);
  if (!status) {
    logError() << "Query failed to mark end session";
  } else {
    App()->getSessionData().set_ended(static_cast<uint64_t>(::time(NULL)));
  }

  return true;
}

static bool doAddData(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq)
{
  SQLiteDatabase::Query query{.type = SQLiteDatabase::QueryType::AddData, .raw = nullptr};
  const auto &data = std::any_cast<tkm::msg::monitor::Data>(rq.bulkData);
  bool status = true;

  auto writeProcAcct = [&db, &status, &query](const std::string &sessionHash,
                                              const tkm::msg::monitor::ProcAcct &acct,
                                              uint64_t systemTime,
                                              uint64_t monotonicTime,
                                              uint64_t receiveTime) {
    status = db->runQuery(
        tkmQuery.addData(
            Query::Type::SQLite3, sessionHash, acct, systemTime, monotonicTime, receiveTime),
        query);
  };

  auto writeProcInfo = [&db, &status, &query](const std::string &sessionHash,
                                              const tkm::msg::monitor::ProcInfo &info,
                                              uint64_t systemTime,
                                              uint64_t monotonicTime,
                                              uint64_t receiveTime) {
    status = db->runQuery(
        tkmQuery.addData(
            Query::Type::SQLite3, sessionHash, info, systemTime, monotonicTime, receiveTime),
        query);
  };

  auto writeContextInfo = [&db, &status, &query](const std::string &sessionHash,
                                                 const tkm::msg::monitor::ContextInfo &info,
                                                 uint64_t systemTime,
                                                 uint64_t monotonicTime,
                                                 uint64_t receiveTime) {
    status = db->runQuery(
        tkmQuery.addData(
            Query::Type::SQLite3, sessionHash, info, systemTime, monotonicTime, receiveTime),
        query);
  };

  auto writeSysProcStat = [&db, &status, &query](const std::string &sessionHash,
                                                 const tkm::msg::monitor::SysProcStat &sysProcStat,
                                                 uint64_t systemTime,
                                                 uint64_t monotonicTime,
                                                 uint64_t receiveTime) {
    status = db->runQuery(
        tkmQuery.addData(
            Query::Type::SQLite3, sessionHash, sysProcStat, systemTime, monotonicTime, receiveTime),
        query);
  };

  auto writeSysProcBuddyInfo =
      [&db, &status, &query](const std::string &sessionHash,
                             const tkm::msg::monitor::SysProcBuddyInfo &sysProcBuddyInfo,
                             uint64_t systemTime,
                             uint64_t monotonicTime,
                             uint64_t receiveTime) {
        status = db->runQuery(tkmQuery.addData(Query::Type::SQLite3,
                                               sessionHash,
                                               sysProcBuddyInfo,
                                               systemTime,
                                               monotonicTime,
                                               receiveTime),
                              query);
      };

  auto writeSysProcWireless =
      [&db, &status, &query](const std::string &sessionHash,
                             const tkm::msg::monitor::SysProcWireless &sysProcWireless,
                             uint64_t systemTime,
                             uint64_t monotonicTime,
                             uint64_t receiveTime) {
        status = db->runQuery(tkmQuery.addData(Query::Type::SQLite3,
                                               sessionHash,
                                               sysProcWireless,
                                               systemTime,
                                               monotonicTime,
                                               receiveTime),
                              query);
      };

  auto writeSysProcMemInfo = [&db, &status, &query](
                                 const std::string &sessionHash,
                                 const tkm::msg::monitor::SysProcMemInfo &sysProcMem,
                                 uint64_t systemTime,
                                 uint64_t monotonicTime,
                                 uint64_t receiveTime) {
    status = db->runQuery(
        tkmQuery.addData(
            Query::Type::SQLite3, sessionHash, sysProcMem, systemTime, monotonicTime, receiveTime),
        query);
  };

  auto writeSysProcDiskStats =
      [&db, &status, &query](const std::string &sessionHash,
                             const tkm::msg::monitor::SysProcDiskStats &sysDiskStats,
                             uint64_t systemTime,
                             uint64_t monotonicTime,
                             uint64_t receiveTime) {
        status = db->runQuery(tkmQuery.addData(Query::Type::SQLite3,
                                               sessionHash,
                                               sysDiskStats,
                                               systemTime,
                                               monotonicTime,
                                               receiveTime),
                              query);
      };

  auto writeSysProcPressure =
      [&db, &status, &query](const std::string &sessionHash,
                             const tkm::msg::monitor::SysProcPressure &sysProcPressure,
                             uint64_t systemTime,
                             uint64_t monotonicTime,
                             uint64_t receiveTime) {
        status = db->runQuery(tkmQuery.addData(Query::Type::SQLite3,
                                               sessionHash,
                                               sysProcPressure,
                                               systemTime,
                                               monotonicTime,
                                               receiveTime),
                              query);
      };

  auto writeProcEvent = [&db, &status, &query](const std::string &sessionHash,
                                               const tkm::msg::monitor::ProcEvent &procEvent,
                                               uint64_t systemTime,
                                               uint64_t monotonicTime,
                                               uint64_t receiveTime) {
    status = db->runQuery(
        tkmQuery.addData(
            Query::Type::SQLite3, sessionHash, procEvent, systemTime, monotonicTime, receiveTime),
        query);
  };

  auto writeSysProcVMStat = [&db, &status, &query](
                                 const std::string &sessionHash,
                                 const tkm::msg::monitor::SysProcVMStat &sysProcVMStat,
                                 uint64_t systemTime,
                                 uint64_t monotonicTime,
                                 uint64_t receiveTime) {
    status = db->runQuery(
        tkmQuery.addData(
            Query::Type::SQLite3, sessionHash, sysProcVMStat, systemTime, monotonicTime, receiveTime),
        query);
  };

  switch (data.what()) {
  case tkm::msg::monitor::Data_What_ProcEvent: {
    tkm::msg::monitor::ProcEvent procEvent;
    data.payload().UnpackTo(&procEvent);
    writeProcEvent(App()->getSessionData().hash(),
                   procEvent,
                   data.system_time_sec(),
                   data.monotonic_time_sec(),
                   data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_ProcAcct: {
    tkm::msg::monitor::ProcAcct procAcct;
    data.payload().UnpackTo(&procAcct);
    writeProcAcct(App()->getSessionData().hash(),
                  procAcct,
                  data.system_time_sec(),
                  data.monotonic_time_sec(),
                  data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_ProcInfo: {
    tkm::msg::monitor::ProcInfo procInfo;
    data.payload().UnpackTo(&procInfo);
    writeProcInfo(App()->getSessionData().hash(),
                  procInfo,
                  data.system_time_sec(),
                  data.monotonic_time_sec(),
                  data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_ContextInfo: {
    tkm::msg::monitor::ContextInfo ctxInfo;
    data.payload().UnpackTo(&ctxInfo);
    writeContextInfo(App()->getSessionData().hash(),
                     ctxInfo,
                     data.system_time_sec(),
                     data.monotonic_time_sec(),
                     data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcStat: {
    tkm::msg::monitor::SysProcStat sysProcStat;
    data.payload().UnpackTo(&sysProcStat);
    writeSysProcStat(App()->getSessionData().hash(),
                     sysProcStat,
                     data.system_time_sec(),
                     data.monotonic_time_sec(),
                     data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcBuddyInfo: {
    tkm::msg::monitor::SysProcBuddyInfo sysProcBuddyInfo;
    data.payload().UnpackTo(&sysProcBuddyInfo);
    writeSysProcBuddyInfo(App()->getSessionData().hash(),
                          sysProcBuddyInfo,
                          data.system_time_sec(),
                          data.monotonic_time_sec(),
                          data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcWireless: {
    tkm::msg::monitor::SysProcWireless sysProcWireless;
    data.payload().UnpackTo(&sysProcWireless);
    writeSysProcWireless(App()->getSessionData().hash(),
                         sysProcWireless,
                         data.system_time_sec(),
                         data.monotonic_time_sec(),
                         data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcMemInfo: {
    tkm::msg::monitor::SysProcMemInfo sysProcMem;
    data.payload().UnpackTo(&sysProcMem);
    writeSysProcMemInfo(App()->getSessionData().hash(),
                        sysProcMem,
                        data.system_time_sec(),
                        data.monotonic_time_sec(),
                        data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcDiskStats: {
    tkm::msg::monitor::SysProcDiskStats sysProcDisks;
    data.payload().UnpackTo(&sysProcDisks);
    writeSysProcDiskStats(App()->getSessionData().hash(),
                          sysProcDisks,
                          data.system_time_sec(),
                          data.monotonic_time_sec(),
                          data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcPressure: {
    tkm::msg::monitor::SysProcPressure sysProcPressure;
    data.payload().UnpackTo(&sysProcPressure);
    writeSysProcPressure(App()->getSessionData().hash(),
                         sysProcPressure,
                         data.system_time_sec(),
                         data.monotonic_time_sec(),
                         data.receive_time_sec());
    break;
  }
  case tkm::msg::monitor::Data_What_SysProcVMStat: {
    tkm::msg::monitor::SysProcVMStat sysProcVMStat;
    data.payload().UnpackTo(&sysProcVMStat);
    writeSysProcVMStat(App()->getSessionData().hash(),
                        sysProcVMStat,
                        data.system_time_sec(),
                        data.monotonic_time_sec(),
                        data.receive_time_sec());
    break;
  }
  default:
    break;
  }

  return true;
}

static bool doConnect(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq)
{
  // No need for DB connect with SQLite
  static_cast<void>(db); // UNUSED
  static_cast<void>(rq); // UNUSED
  return true;
}

static bool doDisconnect(const shared_ptr<SQLiteDatabase> db, const IDatabase::Request &rq)
{
  // No need for DB disconnect with SQLite#
  static_cast<void>(db); // UNUSED
  static_cast<void>(rq); // UNUSED
  return true;
}

} // namespace tkm::reader
