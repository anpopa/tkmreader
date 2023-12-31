/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Arguments Class
 * @details   Store user command line arguments
 *-
 */

#include "Arguments.h"
#include "Defaults.h"
#include "Logger.h"
#include <stdexcept>

using namespace std;

namespace tkm::reader
{

auto Arguments::getFor(Key key) -> string const
{
  if (m_opts.count(key) > 0) {
    return m_opts.at(key);
  }

  switch (key) {
  case Key::Address:
    return tkmDefaults.getFor(Defaults::Default::Address);
  case Key::Port:
    return tkmDefaults.getFor(Defaults::Default::Port);
  case Key::Name:
    return tkmDefaults.getFor(Defaults::Default::Name);
  case Key::DatabasePath:
    return tkmDefaults.getFor(Defaults::Default::DatabasePath);
  case Key::JsonPath:
    return tkmDefaults.getFor(Defaults::Default::JsonPath);
  case Key::Timeout:
    return tkmDefaults.getFor(Defaults::Default::Timeout);
  case Key::Strict:
    return tkmDefaults.getFor(Defaults::Default::Strict);
  case Key::Verbose:
    return tkmDefaults.getFor(Defaults::Default::Verbose);
  default:
    break;
  }

  throw std::runtime_error("Unknown value for key");
}

} // namespace tkm::reader
