/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     IDatabase Class
 * @details   Interfaces for databases
 *-
 */

#pragma once

#include "Defaults.h"
#include <map>
#include <memory>
#include <string>

#include "../bswinfra/source/AsyncQueue.h"

using namespace bswi::event;

namespace tkm::reader
{

class IDatabase
{
public:
  enum class Action {
    CheckDatabase,
    InitDatabase,
    Connect,
    Disconnect,
    GetDevices,
    AddDevice,
    RemoveDevice,
    LoadDevices,
    GetSessions,
    AddSession,
    RemSession,
    EndSession,
    CleanSessions,
    AddData
  };

  typedef struct Request {
    Action action;
    std::any bulkData;
    std::map<Defaults::Arg, std::string> args;
  } Request;

public:
  IDatabase(void)
  {
    m_queue = std::make_shared<AsyncQueue<IDatabase::Request>>(
        "DBQueue", [this](const IDatabase::Request &rq) { return requestHandler(rq); });
  }
  virtual ~IDatabase() = default;

  bool pushRequest(Request &rq) { return m_queue->push(rq); }
  virtual void enableEvents() = 0;
  virtual bool requestHandler(const IDatabase::Request &request) = 0;

public:
  IDatabase(IDatabase const &) = delete;
  void operator=(IDatabase const &) = delete;

protected:
  std::shared_ptr<AsyncQueue<IDatabase::Request>> m_queue = nullptr;
};

} // namespace tkm::reader
