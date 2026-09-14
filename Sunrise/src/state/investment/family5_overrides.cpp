#include "family5_overrides.h"

#include <cstddef>

namespace sunrise::state::investment {

/** Rewrites the list in place: rows for other slots keep their order, this slot gets one row. */
bool upsert_flag_override(Family5State& family,
                          std::uint16_t slot,
                          std::uint8_t value,
                          bool appendMissing) noexcept {
    const std::size_t oldCount = family.flagCount;
    if (oldCount > family.flags.size()) {
        return false;
    }
    std::size_t write = 0;
    bool found = false;
    for (std::size_t read = 0; read < oldCount; ++read) {
        const UnlockFlagOverride row = family.flags[read];
        if (row.slot == slot) {
            if (!found) {
                family.flags[write++] = {slot, value};
                found = true;
            }
            continue;
        }
        family.flags[write++] = row;
    }
    if (!found && appendMissing) {
        if (write >= family.flags.size()) {
            return false;
        }
        family.flags[write++] = {slot, value};
    }
    for (std::size_t index = write; index < oldCount; ++index) {
        family.flags[index] = {};
    }
    family.flagCount = write;
    return true;
}

} // namespace sunrise::state::investment
