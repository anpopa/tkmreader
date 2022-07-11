/*-
 * SPDX-License-Identifier: MIT
 *-
 * @date      2021-2022
 * @author    Alin Popa <alin.popa@fxdata.ro>
 * @copyright MIT
 * @brief     Application Class
 * @details   Main Application Class
 *-
 */

#include <filesystem>

#include "Application.h"
#include "Arguments.h"
#include "Defaults.h"
#include "Logger.h"
#include "SQLiteDatabase.h"

using std::string;

namespace tkm::reader
{

Application *Application::appInstance = nullptr;
static bool verboseEnabled = false;

Application::Application(const string &name,
                         const string &description,
                         const std::map<Arguments::Key, std::string> &args)
: bswi::app::IApplication(name, description)
{
  if (Application::appInstance != nullptr) {
    throw bswi::except::SingleInstance();
  }
  appInstance = this;

  m_arguments = std::make_shared<Arguments>(args);

  if (m_arguments->hasFor(Arguments::Key::Init)) {
    if (m_arguments->hasFor(Arguments::Key::DatabasePath)) {
      if (std::filesystem::exists(m_arguments->getFor(Arguments::Key::DatabasePath))) {
        logWarn() << "Removing existing database output file: "
                  << m_arguments->getFor(Arguments::Key::DatabasePath);
        std::filesystem::remove(m_arguments->getFor(Arguments::Key::DatabasePath));
      }
    }
    if (m_arguments->hasFor(Arguments::Key::JsonPath)) {
      if (std::filesystem::exists(m_arguments->getFor(Arguments::Key::JsonPath))) {
        logWarn() << "Removing existing json output file: "
                  << m_arguments->getFor(Arguments::Key::JsonPath);
        std::filesystem::remove(m_arguments->getFor(Arguments::Key::JsonPath));
      }
    }
  }

  if (m_arguments->hasFor(Arguments::Key::DatabasePath)) {
    m_database = std::make_shared<SQLiteDatabase>();
    m_database->enableEvents();
  }
  if (m_arguments->hasFor(Arguments::Key::Verbose)) {
    if (m_arguments->getFor(Arguments::Key::Verbose) == tkmDefaults.valFor(Defaults::Val::True)) {
      verboseEnabled = true;
    }
  }

  m_connection = std::make_shared<Connection>();

  m_dispatcher = std::make_unique<Dispatcher>();
  m_dispatcher->enableEvents();
}

void Application::resetConnection()
{
  m_connection.reset();
  m_connection = std::make_shared<Connection>();
}

void Application::printVerbose(const std::string &msg)
{
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);

  if (verboseEnabled) {
    std::cout << std::put_time(&tm, "%H:%M:%S") << " " << msg << std::endl;
  }
}

void Application::startUpdateLanes(void)
{
  m_fastLaneTimer = std::make_shared<Timer>("FastLaneTimer", [this]() {
    m_dataSources.foreach ([](const std::shared_ptr<DataSource> &entry) {
      if ((entry->getUpdateLane() == DataSource::UpdateLane::Fast) ||
          (entry->getUpdateLane() == DataSource::UpdateLane::Any)) {
        entry->update();
      }
    });
    return true;
  });

  m_paceLaneTimer = std::make_shared<Timer>("PaceLaneTimer", [this]() {
    m_dataSources.foreach ([](const std::shared_ptr<DataSource> &entry) {
      if ((entry->getUpdateLane() == DataSource::UpdateLane::Pace) ||
          (entry->getUpdateLane() == DataSource::UpdateLane::Any)) {
        entry->update();
      }
    });
    return true;
  });

  m_slowLaneTimer = std::make_shared<Timer>("SlowLaneTimer", [this]() {
    m_dataSources.foreach ([](const std::shared_ptr<DataSource> &entry) {
      if ((entry->getUpdateLane() == DataSource::UpdateLane::Slow) ||
          (entry->getUpdateLane() == DataSource::UpdateLane::Any)) {
        entry->update();
      }
    });
    return true;
  });

  configUpdateLanes();

  m_fastLaneTimer->start(getSessionInfo().fast_lane_interval(), true);
  m_paceLaneTimer->start(getSessionInfo().pace_lane_interval(), true);
  m_slowLaneTimer->start(getSessionInfo().slow_lane_interval(), true);

  addEventSource(m_fastLaneTimer);
  addEventSource(m_paceLaneTimer);
  addEventSource(m_slowLaneTimer);
}

void Application::stopUpdateLanes(void)
{
  if (m_fastLaneTimer != nullptr) {
    m_fastLaneTimer->stop();
    remEventSource(m_fastLaneTimer);
  }

  if (m_paceLaneTimer != nullptr) {
    m_paceLaneTimer->stop();
    remEventSource(m_paceLaneTimer);
  }

  if (m_slowLaneTimer != nullptr) {
    m_slowLaneTimer->stop();
    remEventSource(m_slowLaneTimer);
  }
}

void Application::configUpdateLanes(void)
{
  const auto procAcctUpdateCallback = [this]() {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request ProcAcct");
    logInfo() << "Request ProcAcct data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetProcAcct");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetProcAcct);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto procInfoUpdateCallback = [this]() -> bool {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request ProcInfo");
    logInfo() << "Request ProcInfo data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetProcInfo");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetProcInfo);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto contextInfoUpdateCallback = [this]() {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request ContextInfo");
    logInfo() << "Request ContextInfo data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetContextInfo");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetContextInfo);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto procEventUpdateCallback = [this]() {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request ProcEvent");
    logInfo() << "Request ProcEvent data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetProcEvent");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetProcEventStats);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto sysProcStatUpdateCallback = [this]() {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request SysProcStat");
    logInfo() << "Request SysProcStat data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetSysProcStat");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcStat);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto sysProcBuddyInfoUpdateCallback = [this]() {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request SysProcBuddyInfo");
    logInfo() << "Request SysProcBuddyInfo data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetSysProcBuddyInfo");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcBuddyInfo);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto sysProcMemInfoUpdateCallback = [this]() {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request SysProcMemInfo");
    logInfo() << "Request SysProcMemInfo data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetSysProcMemInfo");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcMemInfo);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto sysProcDiskStatsUpdateCallback = [this]() {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request SysProcDiskStats");
    logInfo() << "Request SysProcDiskStats data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetSysProcDiskStats");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcDiskStats);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  const auto sysProcPressureUpdateCallback = [this]() -> bool {
    tkm::msg::Envelope requestEnvelope;
    tkm::msg::collector::Request requestMessage;

    App()->printVerbose("Request SysProcPressure");
    logInfo() << "Request SysProcPressure data to " << App()->getDeviceData().name();

    requestMessage.set_id("GetSysProcPressure");
    requestMessage.set_type(tkm::msg::collector::Request_Type_GetSysProcPressure);
    requestEnvelope.mutable_mesg()->PackFrom(requestMessage);
    requestEnvelope.set_target(tkm::msg::Envelope_Recipient_Monitor);
    requestEnvelope.set_origin(tkm::msg::Envelope_Recipient_Collector);

    return getConnection()->writeEnvelope(requestEnvelope);
  };

  // Clear the existing list
  m_dataSources.foreach (
      [this](const std::shared_ptr<DataSource> &entry) { m_dataSources.remove(entry); });
  m_dataSources.commit();

  for (const auto &dataSourceType : m_sessionInfo.fast_lane_sources()) {
    switch (dataSourceType) {
    case msg::monitor::SessionInfo_DataSource_ProcInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcInfo", DataSource::UpdateLane::Fast, procInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ProcAcct:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcAcct", DataSource::UpdateLane::Fast, procAcctUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ProcEvent:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcEvent", DataSource::UpdateLane::Fast, procEventUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ContextInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "ContextInfo", DataSource::UpdateLane::Fast, contextInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcStat:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcStat", DataSource::UpdateLane::Fast, sysProcStatUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcBuddyInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcBuddyInfo", DataSource::UpdateLane::Fast, sysProcBuddyInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcMemInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcMemInfo", DataSource::UpdateLane::Fast, sysProcMemInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcPressure:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcPressure", DataSource::UpdateLane::Fast, sysProcPressureUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcDiskStats:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcDiskStats", DataSource::UpdateLane::Fast, sysProcDiskStatsUpdateCallback));
      break;
    default:
      break;
    }
  }

  for (const auto &dataSourceType : m_sessionInfo.pace_lane_sources()) {
    switch (dataSourceType) {
    case msg::monitor::SessionInfo_DataSource_ProcInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcInfo", DataSource::UpdateLane::Pace, procInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ProcAcct:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcAcct", DataSource::UpdateLane::Pace, procAcctUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ProcEvent:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcEvent", DataSource::UpdateLane::Pace, procEventUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ContextInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "ContextInfo", DataSource::UpdateLane::Pace, contextInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcStat:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcStat", DataSource::UpdateLane::Pace, sysProcStatUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcBuddyInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcBuddyInfo", DataSource::UpdateLane::Pace, sysProcBuddyInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcMemInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcMemInfo", DataSource::UpdateLane::Pace, sysProcMemInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcPressure:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcPressure", DataSource::UpdateLane::Pace, sysProcPressureUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcDiskStats:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcDiskStats", DataSource::UpdateLane::Pace, sysProcDiskStatsUpdateCallback));
      break;
    default:
      break;
    }
  }

  for (const auto &dataSourceType : m_sessionInfo.slow_lane_sources()) {
    switch (dataSourceType) {
    case msg::monitor::SessionInfo_DataSource_ProcInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcInfo", DataSource::UpdateLane::Slow, procInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ProcAcct:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcAcct", DataSource::UpdateLane::Slow, procAcctUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ProcEvent:
      m_dataSources.append(std::make_shared<DataSource>(
          "ProcEvent", DataSource::UpdateLane::Slow, procEventUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_ContextInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "ContextInfo", DataSource::UpdateLane::Slow, contextInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcStat:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcStat", DataSource::UpdateLane::Slow, sysProcStatUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcBuddyInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcBuddyInfo", DataSource::UpdateLane::Slow, sysProcBuddyInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcMemInfo:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcMemInfo", DataSource::UpdateLane::Slow, sysProcMemInfoUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcPressure:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcPressure", DataSource::UpdateLane::Slow, sysProcPressureUpdateCallback));
      break;
    case msg::monitor::SessionInfo_DataSource_SysProcDiskStats:
      m_dataSources.append(std::make_shared<DataSource>(
          "SysProcDiskStats", DataSource::UpdateLane::Slow, sysProcDiskStatsUpdateCallback));
      break;
    default:
      break;
    }
  }

  m_dataSources.commit();
}

} // namespace tkm::reader
