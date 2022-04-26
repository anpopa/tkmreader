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

#include "Application.h"
#include "Arguments.h"
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

  m_database = std::make_shared<SQLiteDatabase>();
  m_database->enableEvents();

  m_connection = std::make_shared<Connection>();
  m_connection->enableEvents();

  m_dispatcher = std::make_unique<Dispatcher>();
  m_dispatcher->enableEvents();

  m_command = std::make_unique<Command>();
  m_command->enableEvents();
}

} // namespace tkm::reader
