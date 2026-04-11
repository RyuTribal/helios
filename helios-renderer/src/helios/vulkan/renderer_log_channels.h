#pragma once
#include "helios/core/log_macros.h"

// Renderer subsystem log channel.
// Defined inline so that any TU in helios-renderer can use HELIOS_LOG(Renderer, ...).
HELIOS_DEFINE_LOG_CHANNEL(Renderer);
