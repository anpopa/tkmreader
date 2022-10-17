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
static JsonWriter::OutputType outputType = JsonWriter::OutputType::Disabled;

JsonWriter::JsonWriter()
{
  if (App()->getArguments()->hasFor(Arguments::Key::JsonPath)) {
    if (App()->getArguments()->getFor(Arguments::Key::JsonPath) == "stdout") {
      outputType = JsonWriter::OutputType::StandardOut;
    } else {
      outputType = JsonWriter::OutputType::FilePath;
    }
  }

  if (outputType == JsonWriter::OutputType::FilePath) {
    m_outStream =
        std::make_unique<std::ofstream>(App()->getArguments()->getFor(Arguments::Key::JsonPath),
                                        std::ofstream::out | std::ofstream::app);

    if (m_outStream->tellp() > 0) {
      *m_outStream << std::endl;
    }
  }

  builder["commentStyle"] = "None";
  builder["indentation"] = "";
}

void JsonWriter::Payload::print()
{
  switch (outputType) {
  case OutputType::StandardOut:
    std::cout << m_stream.str() << std::endl;
    break;
  case OutputType::FilePath:
    if (m_outStream != nullptr) {
      *m_outStream << m_stream.str() << std::endl;
    }
    break;
  default:
    break;
  }
}

} // namespace tkm::reader
