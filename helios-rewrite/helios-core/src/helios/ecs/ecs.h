#pragma once

// Convenience header — includes the full ECS surface area.

// --- Plan 1 types ---
#include "helios/ecs/entity.h"
#include "helios/ecs/entity_allocator.h"
#include "helios/ecs/component_id.h"
#include "helios/ecs/column.h"
#include "helios/ecs/archetype.h"
#include "helios/ecs/archetype_storage.h"
#include "helios/ecs/resource_storage.h"
#include "helios/ecs/event_storage.h"
#include "helios/ecs/query_filters.h"
#include "helios/ecs/query.h"
#include "helios/ecs/world.h"
#include "helios/ecs/commands.h"
#include "helios/ecs/asset_handle.h"
#include "helios/ecs/system_params.h"

// --- Plan 2 types ---
#include "helios/ecs/access_descriptor.h"
#include "helios/ecs/app.h"
#include "helios/ecs/dag_builder.h"
#include "helios/ecs/plugin.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/scheduler.h"
#include "helios/ecs/system_descriptor.h"
#include "helios/ecs/system_param_traits.h"
#include "helios/ecs/system_set.h"
#include "helios/ecs/thread_pool.h"
#include "helios/ecs/time.h"
