/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Command Class
 * @details   Handle user command sequence
 *-
 */

#include <csignal>
#include <errno.h>
#include <filesystem>
#include <netdb.h>
#include <time.h>
#include <unistd.h>

#include "Application.h"
#include "Connection.h"
#include "Defaults.h"
#include "Helpers.h"

#include "Monitor.pb.h"

namespace tkm::reader
{

Connection::Connection()
: Pollable("Connection")
{
  if ((m_sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    throw std::runtime_error("Fail to create Connection socket");
  }

  m_reader = std::make_unique<EnvelopeReader>(m_sockFd);
  m_writer = std::make_unique<EnvelopeWriter>(m_sockFd);

  lateSetup(
      [this]() {
        auto status = true;

        do {
          tkm::msg::Envelope envelope;

          // Read next message
          auto readStatus = readEnvelope(envelope);
          if (readStatus == IAsyncEnvelope::Status::Again) {
            return true;
          } else if (readStatus == IAsyncEnvelope::Status::Error) {
            logDebug() << "Read error";
            return false;
          } else if (readStatus == IAsyncEnvelope::Status::EndOfFile) {
            logDebug() << "Read end of file";
            return false;
          }

          // Check for valid origin
          if (envelope.origin() != tkm::msg::Envelope_Recipient_Monitor) {
            continue;
          }

          tkm::msg::monitor::Message msg;
          envelope.mesg().UnpackTo(&msg);

          switch (msg.type()) {
          case tkm::msg::monitor::Message_Type_SetSession: {
            Dispatcher::Request rq{.action = Dispatcher::Action::SetSession};
            tkm::msg::monitor::SessionInfo sessionInfo;

            msg.payload().UnpackTo(&sessionInfo);

            const std::string sessionName =
                "Collector." + std::to_string(getpid()) + "." + std::to_string(time(NULL));
            sessionInfo.set_name(sessionName);

            rq.bulkData = std::make_any<tkm::msg::monitor::SessionInfo>(sessionInfo);

            App()->getDispatcher()->pushRequest(rq);
            break;
          }
          case tkm::msg::monitor::Message_Type_Data: {
            Dispatcher::Request rq{.action = Dispatcher::Action::ProcessData};
            tkm::msg::monitor::Data data;

            msg.payload().UnpackTo(&data);
            data.set_receive_time_sec(time(NULL));
            rq.bulkData = std::make_any<tkm::msg::monitor::Data>(data);

            App()->getDispatcher()->pushRequest(rq);
            break;
          }
          case tkm::msg::monitor::Message_Type_Status: {
            Dispatcher::Request rq{.action = Dispatcher::Action::Status};
            tkm::msg::monitor::Status status;

            msg.payload().UnpackTo(&status);
            rq.bulkData = std::make_any<tkm::msg::monitor::Status>(status);

            App()->getDispatcher()->pushRequest(rq);
            break;
          }
          default:
            logError() << "Unknown response type";
            status = false;
            break;
          }
        } while (status);

        return status;
      },
      m_sockFd,
      bswi::event::IPollable::Events::Level,
      bswi::event::IEventSource::Priority::Normal);

  // We are ready for events only after connect
  setPrepare([]() { return false; });
  // If the event is removed we stop the main application
  setFinalize([]() {
    logInfo() << "Device connection terminated";
    Dispatcher::Request nrq{.action = Dispatcher::Action::Reconnect};
    App()->getDispatcher()->pushRequest(nrq);
  });
}

void Connection::enableEvents()
{
  initCollectorTimers();
  App()->addEventSource(getShared());
}

Connection::~Connection()
{
  if (m_sockFd > 0) {
    ::close(m_sockFd);
  }
}

auto Connection::connect() -> int
{
  int port = 0;

  std::string monitorAddress = App()->getArguments()->getFor(Arguments::Key::Address);
  struct hostent *monitor = gethostbyname(monitorAddress.c_str());

  m_addr.sin_family = AF_INET;
  memcpy(&m_addr.sin_addr.s_addr, monitor->h_addr, (size_t) monitor->h_length);
  try {
    port = std::stoi(App()->getArguments()->getFor(Arguments::Key::Port));
  } catch (const std::exception &e) {
    port = std::stoi(tkmDefaults.getFor(Defaults::Default::Port));
    logWarn() << "Cannot convert port number from config (using 3357): " << e.what();
  }
  m_addr.sin_port = htons(port);

  if (::connect(m_sockFd, (struct sockaddr *) &m_addr, sizeof(struct sockaddr_in)) == -1) {
    if (errno == EINPROGRESS) {
      fd_set wfds, efds;

      FD_ZERO(&wfds);
      FD_SET(m_sockFd, &wfds);

      FD_ZERO(&efds);
      FD_SET(m_sockFd, &efds);

      // We are going to use select to wait for the socket to connect
      struct timeval tv;
      tv.tv_sec = 3;
      tv.tv_usec = 0;

      auto ret = select(m_sockFd + 1, NULL, &wfds, &efds, &tv);
      if (ret == -1) {
        logError() << "Error Connecting";
        return -1;
      }
      if (ret == 0) {
        logError() << "Connection timeout";
        return -1;
      }
      if (!FD_ISSET(m_sockFd, &efds)) {
        int error = 0;
        socklen_t len = sizeof(error);

        if (getsockopt(m_sockFd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
          logError() << "Connection failed";
          return -1;
        }

        if (error != 0) {
          logError() << "Connection failed. Reason: " << strerror(error);
          return -1;
        }
      }
    } else {
      logError() << "Failed to connect to monitor: " << strerror(errno);
      return -1;
    }
  }

  struct timeval timeout;
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  if (setsockopt(m_sockFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    logError() << "Failed to setsockopt";
    return -1;
  }
  if (setsockopt(m_sockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
    logError() << "Failed to setsockopt";
    return -1;
  }

  // We are ready to process events
  logInfo() << "Connected to monitor";
  setPrepare([]() { return true; });

  return 0;
}

void Connection::initCollectorTimers(void)
{
  std::weak_ptr<Connection> weakConnection(getShared());

  // ProcAcct timer
  m_procAcctTimer = std::make_shared<Timer>("ProcAcctTimer", [weakConnection]() {
    auto lock = weakConnection.lock();
    if (lock) {
      tkm::msg::Envelope requestEnvelope;
      tkm::msg::collector::Request requestMessage;

      logInfo() << "Request ProcAcct data to " << App()->getDeviceData().name();

      requestMessage.set_id("GetProcAcct");
      requestMessage.set_type(tkm::msg::collector::Request_Type_GetProcAcct);
      requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
      requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
      requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

      return lock->writeEnvelope(requestEnvelope);
    }
    return false;
  });

  // ProcInfo timer
  m_procInfoTimer = std::make_shared<Timer>("ProcInfoTimer", [weakConnection]() {
    auto lock = weakConnection.lock();
    if (lock) {
      tkm::msg::Envelope requestEnvelope;
      tkm::msg::collector::Request requestMessage;

      logInfo() << "Request ProcInfo data to " << App()->getDeviceData().name();

      requestMessage.set_id("GetProcInfo");
      requestMessage.set_type(tkm::msg::collector::Request_Type_GetProcInfo);
      requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
      requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
      requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

      return lock->writeEnvelope(requestEnvelope);
    }
    return false;
  });

  // ContextInfo timer
  m_contextInfoTimer = std::make_shared<Timer>("ContextInfoTimer", [weakConnection]() {
    auto lock = weakConnection.lock();
    if (lock) {
      tkm::msg::Envelope requestEnvelope;
      tkm::msg::collector::Request requestMessage;

      logInfo() << "Request ContextInfo data to " << App()->getDeviceData().name();

      requestMessage.set_id("GetContextInfo");
      requestMessage.set_type(tkm::msg::collector::Request_Type_GetContextInfo);
      requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
      requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
      requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

      return lock->writeEnvelope(requestEnvelope);
    }
    return false;
  });

  // ProcEvent timer
  m_procEventTimer = std::make_shared<Timer>("ProcEventTimer", [weakConnection]() {
    auto lock = weakConnection.lock();
    if (lock) {
      tkm::msg::Envelope requestEnvelope;
      tkm::msg::collector::Request requestMessage;

      logInfo() << "Request ProcEvent data to " << App()->getDeviceData().name();

      requestMessage.set_id("GetProcEvent");
      requestMessage.set_type(tkm::msg::collector::Request_Type_GetProcEventStats);
      requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
      requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
      requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

      return lock->writeEnvelope(requestEnvelope);
    }
    return false;
  });

  // SysProcStat timer
  m_sysProcStatTimer = std::make_shared<Timer>("SysProcStatTimer", [weakConnection]() {
    auto lock = weakConnection.lock();
    if (lock) {
      tkm::msg::Envelope requestEnvelope;
      tkm::msg::collector::Request requestMessage;

      logInfo() << "Request SysProcStat data to " << App()->getDeviceData().name();

      requestMessage.set_id("GetSysProcStat");
      requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcStat);
      requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
      requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
      requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

      return lock->writeEnvelope(requestEnvelope);
    }
    return false;
  });

  // SysProcMemInfo timer
  m_sysProcMemInfoTimer = std::make_shared<Timer>("SysProcMemInfoTimer", [weakConnection]() {
    auto lock = weakConnection.lock();
    if (lock) {
      tkm::msg::Envelope requestEnvelope;
      tkm::msg::collector::Request requestMessage;

      logInfo() << "Request SysProcMeminfo data to " << App()->getDeviceData().name();

      requestMessage.set_id("GetSysProcMemInfo");
      requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcMeminfo);
      requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
      requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
      requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

      return lock->writeEnvelope(requestEnvelope);
    }
    return false;
  });

  // SysProcPressure timer
  m_sysProcPressureTimer = std::make_shared<Timer>("SysProcPressureTimer", [weakConnection]() {
    auto lock = weakConnection.lock();
    if (lock) {
      tkm::msg::Envelope requestEnvelope;
      tkm::msg::collector::Request requestMessage;

      logInfo() << "Request SysProcPressure data to " << App()->getDeviceData().name();

      requestMessage.set_id("GetSysProcPressure");
      requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcPressure);
      requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
      requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
      requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

      return lock->writeEnvelope(requestEnvelope);
    }
    return false;
  });
}

void Connection::startCollectorTimers(void)
{
  m_procAcctTimer->start(App()->getSessionInfo().proc_acct_poll_interval(), true);
  App()->addEventSource(m_procAcctTimer);
  m_procInfoTimer->start(App()->getSessionInfo().proc_info_poll_interval(), true);
  App()->addEventSource(m_procInfoTimer);
  m_contextInfoTimer->start(App()->getSessionInfo().context_information_poll_interval(), true);
  App()->addEventSource(m_contextInfoTimer);
  m_procEventTimer->start(App()->getSessionInfo().proc_event_poll_interval(), true);
  App()->addEventSource(m_procEventTimer);
  m_sysProcStatTimer->start(App()->getSessionInfo().sys_proc_stat_poll_interval(), true);
  App()->addEventSource(m_sysProcStatTimer);
  m_sysProcMemInfoTimer->start(App()->getSessionInfo().sys_proc_meminfo_poll_interval(), true);
  App()->addEventSource(m_sysProcMemInfoTimer);
  m_sysProcPressureTimer->start(App()->getSessionInfo().sys_proc_pressure_poll_interval(), true);
  App()->addEventSource(m_sysProcPressureTimer);
}

void Connection::stopCollectorTimers(void)
{
  m_procAcctTimer->stop();
  App()->remEventSource(m_procAcctTimer);
  m_procInfoTimer->stop();
  App()->remEventSource(m_procInfoTimer);
  m_contextInfoTimer->stop();
  App()->remEventSource(m_contextInfoTimer);
  m_procEventTimer->stop();
  App()->remEventSource(m_procEventTimer);
  m_sysProcStatTimer->stop();
  App()->remEventSource(m_sysProcStatTimer);
  m_sysProcMemInfoTimer->stop();
  App()->remEventSource(m_sysProcMemInfoTimer);
  m_sysProcPressureTimer->stop();
  App()->remEventSource(m_sysProcPressureTimer);
}

} // namespace tkm::reader
