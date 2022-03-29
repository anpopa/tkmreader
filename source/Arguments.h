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

#pragma once

#include <map>
#include <string>

namespace tkm::reader
{

class Arguments
{
public:
    enum class Key { Address, Port };

public:
    explicit Arguments(const std::map<Key, std::string> &opts)
    {
        m_opts.insert(opts.begin(), opts.end());
    }

    auto getFor(Key key) -> std::string const;
    bool hasFor(Key key)
    {
        return (m_opts.count(key) > 0);
    }
    void setFor(Key key, const std::string &opt)
    {
        m_opts.insert(std::pair<Key, std::string>(key, opt));
    }

private:
    std::map<Key, std::string> m_opts;
};

} // namespace tkm::reader
