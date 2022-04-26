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
#include "Arguments.h"
#include <iostream>
#include <json/writer.h>
#include <memory>

namespace tkm::reader
{

static std::unique_ptr<std::ofstream> m_outStream = nullptr;
JsonWriter *JsonWriter::instance = nullptr;

JsonWriter::JsonWriter()
{
  m_outStream = std::make_unique<std::ofstream>(
      App()->getArguments()->getFor(Arguments::Key::JsonPath), std::ofstream::out);
  builder["commentStyle"] = "None";
  builder["indentation"] = "";
}

void JsonWriter::Payload::print()
{
  *m_outStream << m_stream.str() << std::endl;
}

} // namespace tkm::reader
