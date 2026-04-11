#pragma once

#include <concepts>

namespace helios {

class App; // forward declaration

/// A Plugin is any type with a `void build(App&)` method.
/// Plugins register systems, resources, events, and other plugins.
///
/// Example:
///   struct MyPlugin {
///       void build(App& app) {
///           app.insert_resource<MyConfig>({});
///           app.add_system(Schedule::Update, my_system);
///       }
///   };
template <typename T>
concept Plugin = requires(T plugin, App& app) {
    { plugin.build(app) } -> std::same_as<void>;
};

} // namespace helios
