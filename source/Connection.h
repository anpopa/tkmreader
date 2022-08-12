/*-
 * Copyright (c) 2020 Alin Popa
 * All rights reserved.
 */

/*
 * @author Alin Popa <alin.popa@fxdata.ro>
 */

#pragma once

#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <taskmonitor/TaskMonitor.h>

#include "Arguments.h"

#include "../bswinfra/source/Exceptions.h"
#include "../bswinfra/source/IApplication.h"
#include "../bswinfra/source/Pollable.h"
#include "../bswinfra/source/Timer.h"

using namespace bswi::log;
using namespace bswi::event;

namespace tkm::reader
{

class Connection final : public Pollable, public std::enable_shared_from_this<Connection>
{
public:
  Connection();
  ~Connection();

public:
  Connection(Connection const &) = delete;
  void operator=(Connection const &) = delete;

  void enableEvents();
  auto connect() -> int;
  [[nodiscard]] int getFD() const { return m_sockFd; }
  auto getShared() -> std::shared_ptr<Connection> { return shared_from_this(); }

  auto readEnvelope(tkm::msg::Envelope &envelope) -> tkm::IAsyncEnvelope::Status
  {
    return m_reader->next(envelope);
  }

  bool writeEnvelope(const tkm::msg::Envelope &envelope)
  {
    if (m_writer->send(envelope) == tkm::IAsyncEnvelope::Status::Ok) {
      return m_writer->flush();
    }
    return true;
  }

private:
  std::unique_ptr<tkm::EnvelopeReader> m_reader = nullptr;
  std::unique_ptr<tkm::EnvelopeWriter> m_writer = nullptr;
  struct sockaddr_in m_addr = {};
  int m_sockFd = -1;
};

} // namespace tkm::reader
