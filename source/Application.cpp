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

#include <filesystem>

#include "Application.h"
#include "Arguments.h"
#include "Logger.h"
#include "SQLiteDatabase.h"

using std::string;

namespace tkm::reader
{

Application *Application::appInstance = nullptr;

Application::Application(const string &name,
                         const string &description,
                         const std::map<Arguments::Key, std::string> &args)
: bswi::app::IApplication(name, description)
{
  if (Application::appInstance != nullptr) {
    throw bswi::except::SingleInstance();
  }
  appInstance = this;

  m_arguments = std::make_shared<Arguments>(args);

  if (std::filesystem::exists(App()->getArguments()->getFor(Arguments::Key::DatabasePath))) {
    logWarn() << "Removing existing database output file: "
              << App()->getArguments()->getFor(Arguments::Key::DatabasePath);
    std::filesystem::remove(App()->getArguments()->getFor(Arguments::Key::DatabasePath));
  }
  if (std::filesystem::exists(App()->getArguments()->getFor(Arguments::Key::JsonPath))) {
    logWarn() << "Removing existing json output file: "
              << App()->getArguments()->getFor(Arguments::Key::JsonPath);
    std::filesystem::remove(App()->getArguments()->getFor(Arguments::Key::JsonPath));
  }

  m_database = std::make_shared<SQLiteDatabase>();
  m_database->enableEvents();

  m_connection = std::make_shared<Connection>();

  m_dispatcher = std::make_unique<Dispatcher>();
  m_dispatcher->enableEvents();
}

void Application::resetConnection()
{
  m_connection.reset();
  m_connection = std::make_shared<Connection>();
}

} // namespace tkm::reader
