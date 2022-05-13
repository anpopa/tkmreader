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

JsonWriter *JsonWriter::instance = nullptr;
static std::unique_ptr<std::ofstream> m_outStream = nullptr;
static bool useStdOut = true;

JsonWriter::JsonWriter()
{
  if (App()->getArguments()->hasFor(Arguments::Key::JsonPath)) {
    m_outStream =
        std::make_unique<std::ofstream>(App()->getArguments()->getFor(Arguments::Key::JsonPath),
                                        std::ofstream::out | std::ofstream::app);

    if (m_outStream->tellp() > 0) {
      *m_outStream << std::endl;
    }
    useStdOut = false;
  }
  builder["commentStyle"] = "None";
  builder["indentation"] = "";
}

void JsonWriter::Payload::print()
{
  if (useStdOut) {
    std::cout << m_stream.str() << std::endl;
  } else {
    *m_outStream << m_stream.str() << std::endl;
  }
}

} // namespace tkm::reader
