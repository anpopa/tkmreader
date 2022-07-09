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
#include <netinet/tcp.h>
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
            tkm::msg::monitor::Status s;

            msg.payload().UnpackTo(&s);
            rq.bulkData = std::make_any<tkm::msg::monitor::Status>(s);

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

  if (monitor == nullptr) {
    throw std::runtime_error("Invalid device address");
  }
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
        App()->printVerbose("Error Connecting");
        return -1;
      }
      if (ret == 0) {
        logError() << "Connection timeout";
        App()->printVerbose("Connection timeout");
        return -1;
      }
      if (!FD_ISSET(m_sockFd, &efds)) {
        int error = 0;
        socklen_t len = sizeof(error);

        if (getsockopt(m_sockFd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
          logError() << "Connection failed";
          App()->printVerbose("Connection failed");
          return -1;
        }

        if (error != 0) {
          logError() << "Connection failed. Reason: " << strerror(error);
          App()->printVerbose("Connection failed");
          return -1;
        }
      }
    } else {
      App()->printVerbose("Connection failed");
      logError() << "Failed to connect to monitor: " << strerror(errno);
      return -1;
    }
  }

  struct timeval timeout;
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  if (setsockopt(m_sockFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    logError() << "Failed to setsockopt SO_RCVTIMEO. Error: " << strerror(errno);
    return -1;
  }
  if (setsockopt(m_sockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
    logError() << "Failed to setsockopt SO_SNDTIMEO. Error: " << strerror(errno);
    return -1;
  }

  int yes = 1;
  if (setsockopt(m_sockFd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) < 0) {
    logError() << "Failed to setsockopt SO_KEEPALIVE. Error: " << strerror(errno);
    return -1;
  }

  int idle = 1;
  if (setsockopt(m_sockFd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int)) < 0) {
    logError() << "Failed to setsockopt TCP_KEEPIDLE. Error: " << strerror(errno);
    return -1;
  }

  int interval = 2;
  if (setsockopt(m_sockFd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int)) < 0) {
    logError() << "Failed to setsockopt TCP_KEEPINTVL. Error: " << strerror(errno);
    return -1;
  }

  int maxpkt = 5;
  if (setsockopt(m_sockFd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int)) < 0) {
    logError() << "Failed to setsockopt TCP_KEEPCNT. Error: " << strerror(errno);
    return -1;
  }

  // We are ready to process events
  logInfo() << "Connected to monitor";
  setPrepare([]() { return true; });

  return 0;
}

} // namespace tkm::reader
