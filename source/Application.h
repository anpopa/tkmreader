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
#include "Command.h"
#include "Connection.h"
#include "Defaults.h"
#include "Dispatcher.h"

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
    appInstance = new Application("TMC", "TaskMonitor Reader Application", noArgs);

    return appInstance;
  }

  void stop() final
  {
    if (m_running) {
      m_mainEventLoop->stop();
    }
  }

  auto getSession() -> tkm::msg::monitor::SessionInfo & { return m_session; }
  auto getDispatcher() -> std::shared_ptr<Dispatcher> { return m_dispatcher; }
  auto getCommand() -> std::shared_ptr<Command> { return m_command; }
  auto getConnection() -> std::shared_ptr<Connection> { return m_connection; }
  auto getArguments() -> std::shared_ptr<Arguments> { return m_arguments; }

public:
  Application(Application const &) = delete;
  void operator=(Application const &) = delete;

private:
  std::shared_ptr<Arguments> m_arguments = nullptr;
  std::shared_ptr<Connection> m_connection = nullptr;
  std::shared_ptr<Command> m_command = nullptr;
  std::shared_ptr<Dispatcher> m_dispatcher = nullptr;

private:
  tkm::msg::monitor::SessionInfo m_session{};
  static Application *appInstance;
};

#define App() tkm::reader::Application::getInstance()

} // namespace tkm::reader
