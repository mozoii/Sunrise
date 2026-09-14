#include "rotation.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "../../middleware/content/packages/tables/definition_index_table.h"
#include "../build_data/vendors/vendor_catalog.h"
#include "../investment/family5_overrides.h"
#include "../investment/store.h"
#include "../unlocks/definition.h"

namespace sunrise::state::vendors::rotation {

std::uint32_t week(std::int64_t unixSeconds) noexcept {
    return unixSeconds < kEpochSeconds
               ? 0U
               : static_cast<std::uint32_t>((unixSeconds - kEpochSeconds) / kWeekSeconds);
}

bool present(std::uint32_t vendorHash, std::int64_t unixSeconds) noexcept {
    if (!rotates(vendorHash)) {
        return true;
    }
    if (unixSeconds < kEpochSeconds) {
        return false;
    }
    return ((unixSeconds - kEpochSeconds) % kWeekSeconds) < kVisibleSeconds;
}

/**
 * Finds the flag one interaction's expression tests when the expression is exactly `flag X;
 * not`, which is the shape of a "sold this week" gate: purchasable until the flag is raised.
 * @param interaction Interaction whose expression is read.
 * @param slot Receives the flag slot.
 * @return True when the expression has that shape.
 */
[[nodiscard]] bool negated_flag_gate(const build_data::vendors::Interaction& interaction,
                                     std::uint16_t& slot) noexcept {
    namespace tables = middleware::content::packages::tables;
    slot = 0;
    if (interaction.programCount != 2
        || interaction.program[0].opcode != tables::kUnlockReadFlagOpcode
        || interaction.program[1].opcode != tables::kUnlockNotOpcode
        || interaction.program[0].operand > (std::numeric_limits<std::uint16_t>::max)()) {
        return false;
    }
    slot = static_cast<std::uint16_t>(interaction.program[0].operand);
    return true;
}

bool append_investment_overrides(Family5State& family, std::int64_t unixSeconds) noexcept {
    build_data::vendors::Definition definition{};
    // A build without the vendor has nothing to publish.
    if (!build_data::vendors::find(kXurVendorHash, definition)) {
        return true;
    }
    bool claimed = false;
    std::uint32_t lastEngramWeek = 0;
    if (!investment::store::read_vendor_rotation(kXurVendorHash, claimed, lastEngramWeek)) {
        return false;
    }
    const bool soldThisWeek = claimed && lastEngramWeek == week(unixSeconds);
    for (std::size_t row = 0; row < definition.interactionCount; ++row) {
        build_data::vendors::Interaction interaction{};
        std::uint16_t slot = 0;
        if (!build_data::vendors::interaction(definition, row, interaction)
            || interaction.categoryIndex != kXurEngramCategory
            || !negated_flag_gate(interaction, slot)) {
            continue;
        }
        // The flag is raised only for the week of the sale. Off-week it is left at its default,
        // or cleared when an authored row would otherwise hold it up.
        const bool stored =
            soldThisWeek
                ? investment::upsert_flag_override(family, slot, unlocks::kFlagSet, true)
                : investment::upsert_flag_override(family, slot, unlocks::kFlagClear, false);
        if (!stored) {
            return false;
        }
    }
    return true;
}

} // namespace sunrise::state::vendors::rotation
