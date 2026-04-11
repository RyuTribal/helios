#include "editor_commands.h"

namespace helios::editor {

void EditorCommands::execute(UndoEntry entry, World& world) {
    m_entries.resize(m_cursor);
    entry.apply(world);
    m_entries.push_back(std::move(entry));
    m_cursor = m_entries.size();
}

bool EditorCommands::undo(World& world) {
    if (!can_undo()) return false;
    --m_cursor;
    m_entries[m_cursor].revert(world);
    return true;
}

bool EditorCommands::redo(World& world) {
    if (!can_redo()) return false;
    m_entries[m_cursor].apply(world);
    ++m_cursor;
    return true;
}

const std::string& EditorCommands::undo_description() const {
    static const std::string empty;
    if (!can_undo()) return empty;
    return m_entries[m_cursor - 1].description;
}

const std::string& EditorCommands::redo_description() const {
    static const std::string empty;
    if (!can_redo()) return empty;
    return m_entries[m_cursor].description;
}

void EditorCommands::clear() {
    m_entries.clear();
    m_cursor = 0;
}

} // namespace helios::editor
