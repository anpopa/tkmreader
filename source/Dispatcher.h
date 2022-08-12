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

#pragma once

#include <map>
#include <string>
#include <taskmonitor/taskmonitor.h>

#include "Arguments.h"
#include "Connection.h"
#include "Defaults.h"

#include "../bswinfra/source/AsyncQueue.h"
#include "../bswinfra/source/Exceptions.h"

using namespace bswi::event;

namespace tkm::reader
{

class Dispatcher : public std::enable_shared_from_this<Dispatcher>
{
public:
  enum class Action {
    PrepareData,
    Connect,
    Reconnect,
    SendDescriptor,
    RequestSession,
    SetSession,
    StartStream,
    ProcessData,
    Status,
    Quit
  };

  typedef struct Request {
    Action action;
    std::any bulkData;
    std::map<tkm::reader::Defaults::Arg, std::string> args;
  } Request;

public:
  Dispatcher()
  {
    m_queue = std::make_shared<AsyncQueue<Request>>(
        "DispatcherQueue", [this](const Request &request) { return requestHandler(request); });
  }

  auto getShared() -> std::shared_ptr<Dispatcher> { return shared_from_this(); }
  void enableEvents();
  bool pushRequest(Request &request);
  auto hashForDevice(const tkm::msg::control::DeviceData &data) -> std::string;

private:
  bool requestHandler(const Request &request);

private:
  std::shared_ptr<AsyncQueue<Request>> m_queue = nullptr;
};

} // namespace tkm::reader
