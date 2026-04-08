// helios-rewrite/helios-renderer/src/helios/graph/graph_log_channel.h
#pragma once
#include "helios/core/log_macros.h"

// Graph subsystem log channel.
// Defined inline so that any TU in helios-renderer/graph/ can use HELIOS_LOG(Graph, ...).
HELIOS_DEFINE_LOG_CHANNEL(Graph);
