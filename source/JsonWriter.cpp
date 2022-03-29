/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     JsonWriter Class
 * @details   Write Json Values to different streams
 *-
 */

#include "JsonWriter.h"
#include "../bswinfra/source/Logger.h"
#include "Application.h"
#include <iostream>
#include <json/writer.h>
#include <memory>

namespace tkm::reader
{

JsonWriter *JsonWriter::instance = nullptr;

JsonWriter::JsonWriter()
{
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
}

void JsonWriter::Payload::print()
{
    std::cout << m_stream.str() << std::endl;
}

} // namespace tkm::reader
