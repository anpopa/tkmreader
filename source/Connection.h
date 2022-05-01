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

#include "Arguments.h"
#include "EnvelopeReader.h"
#include "EnvelopeWriter.h"
#include "Helpers.h"

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

  auto readEnvelope(tkm::msg::Envelope &envelope) -> IAsyncEnvelope::Status
  {
    return m_reader->next(envelope);
  }

  bool writeEnvelope(const tkm::msg::Envelope &envelope)
  {
    if (m_writer->send(envelope) == IAsyncEnvelope::Status::Ok) {
      return m_writer->flush();
    }
    return true;
  }

public:
  void initCollectorTimers(void);
  void startCollectorTimers(void);
  void stopCollectorTimers(void);

private:
  std::shared_ptr<Timer> m_procAcctTimer = nullptr;
  std::shared_ptr<Timer> m_procEventTimer = nullptr;
  std::shared_ptr<Timer> m_sysProcStatTimer = nullptr;
  std::shared_ptr<Timer> m_sysProcMemInfoTimer = nullptr;
  std::shared_ptr<Timer> m_sysProcPressureTimer = nullptr;

private:
  std::unique_ptr<EnvelopeReader> m_reader = nullptr;
  std::unique_ptr<EnvelopeWriter> m_writer = nullptr;
  struct sockaddr_in m_addr = {};
  int m_sockFd = -1;
};

} // namespace tkm::reader
