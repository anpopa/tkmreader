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
    default:
        break;
    }

    throw std::runtime_error("Unknown value for key");
}

} // namespace tkm::reader
