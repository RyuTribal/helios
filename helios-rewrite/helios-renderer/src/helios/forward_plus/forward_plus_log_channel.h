// helios-renderer/src/helios/forward_plus/forward_plus_log_channel.h
#pragma once

#include "helios/core/log_macros.h"

// ForwardPlus subsystem log channel.
// Defined inline so that any TU under forward_plus/ can use
// HELIOS_LOG_INFO(ForwardPlus, ...) etc.
HELIOS_DEFINE_LOG_CHANNEL(ForwardPlus);
