// -*- indent-tabs-mode: nil -*-

#include <arc/client/ACCLoader.h>

#include "FastestQueueBroker.h"
#include "RandomBroker.h"
#include "BenchmarkBroker.h"
#include "DataBroker.h"

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
  { "FastestQueueBroker", "HED:ACC", 0, &Arc::FastestQueueBroker::Instance },
  { "RandomBroker", "HED:ACC", 0, &Arc::RandomBroker::Instance },
  { "BenchmarkBroker", "HED:ACC", 0, &Arc::BenchmarkBroker::Instance },
  { "DataBroker", "HED:ACC", 0, &Arc::DataBroker::Instance },
  { NULL, NULL, 0, NULL }
};
