/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Application Class
 * @details   Main Application Class
 *-
 */

#pragma once

#include <atomic>
#include <cstdlib>
#include <string>
#include <utility>

#include "Arguments.h"
#include "Connection.h"
#include "Defaults.h"
#include "Dispatcher.h"
#include "SQLiteDatabase.h"

#include "../bswinfra/source/IApplication.h"

#include "../bswinfra/source/AsyncQueue.h"
#include "../bswinfra/source/EventLoop.h"
#include "../bswinfra/source/Exceptions.h"
#include "../bswinfra/source/KeyFile.h"
#include "../bswinfra/source/Logger.h"
#include "../bswinfra/source/PathEvent.h"
#include "../bswinfra/source/Pollable.h"
#include "../bswinfra/source/Timer.h"
#include "../bswinfra/source/UserEvent.h"

#include "Collector.pb.h"
#include "Monitor.pb.h"

namespace tkm::reader
{

class Application final : public bswi::app::IApplication
{
public:
  explicit Application(const std::string &name,
                       const std::string &description,
                       const std::map<Arguments::Key, std::string> &args);

  ~Application() final = default;

  static Application *getInstance()
  {
    if (appInstance != nullptr) {
      return appInstance;
    }

    std::map<Arguments::Key, std::string> noArgs;
    appInstance = new Application("TkmReader", "TaskMonitor Reader Application", noArgs);

    return appInstance;
  }

  void stop() final
  {
    if (m_running) {
      m_mainEventLoop->stop();
    }
  }

  auto getDispatcher() -> const std::shared_ptr<Dispatcher> { return m_dispatcher; }
  auto getConnection() -> const std::shared_ptr<Connection> { return m_connection; }
  auto getDatabase() -> const std::shared_ptr<SQLiteDatabase> { return m_database; }
  auto getArguments() -> const std::shared_ptr<Arguments> { return m_arguments; }
  auto getSessionInfo() -> tkm::msg::monitor::SessionInfo & { return m_sessionInfo; }
  auto getDeviceData() -> tkm::msg::control::DeviceData & { return m_deviceData; }
  auto getSessionData() -> tkm::msg::control::SessionData & { return m_sessionData; }

  void printVerbose(const std::string &msg);
  void resetConnection(void);

public:
  Application(Application const &) = delete;
  void operator=(Application const &) = delete;

private:
  std::shared_ptr<Arguments> m_arguments = nullptr;
  std::shared_ptr<Connection> m_connection = nullptr;
  std::shared_ptr<Dispatcher> m_dispatcher = nullptr;
  std::shared_ptr<SQLiteDatabase> m_database = nullptr;

private:
  tkm::msg::monitor::SessionInfo m_sessionInfo{};
  tkm::msg::control::DeviceData m_deviceData{};
  tkm::msg::control::SessionData m_sessionData{};
  static Application *appInstance;
};

#define App() tkm::reader::Application::getInstance()

} // namespace tkm::reader
