#pragma once

#include <helios/ecs/world.h>
#include <helios/ecs/entity.h>

#include <functional>
#include <string>
#include <vector>

namespace helios::editor {

class EditorCommands {
public:
    struct UndoEntry {
        std::string description;
        Entity entity;
        std::function<void(World&)> apply;
        std::function<void(World&)> revert;
    };

    template<typename T>
    void set(Entity entity, World& world, const T& new_value,
             const std::string& description = "Set component") {
        T old_value = world.get<T>(entity);
        m_entries.resize(m_cursor);
        m_entries.push_back(UndoEntry{
            .description = description,
            .entity = entity,
            .apply = [entity, new_value](World& w) { w.get<T>(entity) = new_value; },
            .revert = [entity, old_value](World& w) { w.get<T>(entity) = old_value; },
        });
        world.get<T>(entity) = new_value;
        m_cursor = m_entries.size();
    }

    void execute(UndoEntry entry, World& world);
    bool undo(World& world);
    bool redo(World& world);

    bool can_undo() const { return m_cursor > 0; }
    bool can_redo() const { return m_cursor < m_entries.size(); }

    const std::string& undo_description() const;
    const std::string& redo_description() const;

    void clear();

private:
    std::vector<UndoEntry> m_entries;
    size_t m_cursor = 0;
};

} // namespace helios::editor
