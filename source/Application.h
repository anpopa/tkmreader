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

#include <string>
#include <taskmonitor/taskmonitor.h>

#include "Arguments.h"
#include "Connection.h"
#include "DataSource.h"
#include "Defaults.h"
#include "Dispatcher.h"
#include "SQLiteDatabase.h"

#include "../bswinfra/source/IApplication.h"
#include "../bswinfra/source/SafeList.h"

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

  void requestStartupData(void);
  void startUpdateLanes(void);
  void stopUpdateLanes(void);

public:
  Application(Application const &) = delete;
  void operator=(Application const &) = delete;

private:
  void configUpdateLanes(void);

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

private:
  bswi::util::SafeList<std::shared_ptr<DataSource>> m_dataSources{"DataSourceList"};
  std::shared_ptr<Timer> m_fastLaneTimer = nullptr;
  std::shared_ptr<Timer> m_paceLaneTimer = nullptr;
  std::shared_ptr<Timer> m_slowLaneTimer = nullptr;
};

#define App() tkm::reader::Application::getInstance()

} // namespace tkm::reader
