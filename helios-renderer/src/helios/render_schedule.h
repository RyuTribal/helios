#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

/// Interned identifier for a camera render schedule.
/// Cheap to copy and compare. value=0 means unset/default.
struct RenderScheduleLabel {
    uint32_t value = 0;
    bool operator==(const RenderScheduleLabel&) const = default;
    bool operator!=(const RenderScheduleLabel&) const = default;
};

/// Registry mapping string names → interned RenderScheduleLabel values.
/// Stored as a World resource. Populated during plugin build phase.
class RenderScheduleRegistry {
public:
    /// Get-or-create a label. Called during plugin build().
    RenderScheduleLabel label(const std::string& name) {
        auto it = m_name_to_label.find(name);
        if (it != m_name_to_label.end()) return it->second;

        RenderScheduleLabel lbl{m_next++};
        m_name_to_label[name] = lbl;
        m_label_to_name.push_back(name);
        return lbl;
    }

    /// Lookup only (returns label with value=0 if not found).
    RenderScheduleLabel find(const std::string& name) const {
        auto it = m_name_to_label.find(name);
        if (it != m_name_to_label.end()) return it->second;
        return {};
    }

    /// Get name for debugging. Returns empty string for invalid labels.
    const std::string& name_of(RenderScheduleLabel lbl) const {
        static const std::string empty;
        if (lbl.value == 0 || lbl.value > m_label_to_name.size()) return empty;
        return m_label_to_name[lbl.value - 1];
    }

private:
    std::unordered_map<std::string, RenderScheduleLabel> m_name_to_label;
    std::vector<std::string> m_label_to_name; // indexed by label.value - 1
    uint32_t m_next = 1;
};

} // namespace helios
