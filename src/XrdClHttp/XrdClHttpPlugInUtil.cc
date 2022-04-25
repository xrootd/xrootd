/**
 * This file is part of XrdClHttp
 */

#include "XrdClHttp/XrdClHttpPlugInUtil.hh"

#include <mutex>

#include "XrdCl/XrdClLog.hh"

static std::once_flag logging_topic_init;

namespace XrdCl {

void SetUpLogging(Log* logger) {
  // Assert that there is no existing topic
  std::call_once(logging_topic_init, [logger] {
      if (logger) {
        logger->SetTopicName(kLogXrdClHttp, "XrdClHttp");
      }
    });
}

}
