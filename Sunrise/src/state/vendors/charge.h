#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../build_data/vendors/definition.h"

namespace sunrise::state::vendors {

/**
 * What one vendor sale row charges: every price-override row it declares, spent together.
 * Every path that spends a row's cost builds one of these from the row, so the cost fields are
 * read in exactly one place and a free row is recognised the same way everywhere.
 */
struct Charge {
    std::array<build_data::vendors::SaleCost, build_data::vendors::kSaleCostCapacity> costs{};
    std::uint8_t count{};

    /** @return The cost one sale row carries. */
    [[nodiscard]] static constexpr Charge of(const build_data::vendors::SaleRow& row) noexcept {
        return {row.costs, row.costCount};
    }

    /** @return The declared cost rows, in order. */
    [[nodiscard]] std::span<const build_data::vendors::SaleCost> entries() const noexcept {
        return {costs.data(), count > costs.size() ? std::size_t{0} : std::size_t{count}};
    }

    /** @return True when nothing is charged: no cost row names an item with a nonzero quantity. */
    [[nodiscard]] constexpr bool is_free() const noexcept {
        for (std::size_t index = 0; index < count && index < costs.size(); ++index) {
            if (costs[index].itemIndex != build_data::vendors::kAbsentCostItem
                && costs[index].quantity != 0) {
                return false;
            }
        }
        return true;
    }
};

/** Why one sale charge was refused. */
enum class ChargeRefusal : std::uint8_t {
    none,
    /** A cost row names something the account cannot pay with: not a stackable profile item. */
    malformedCost,
    /** Every cost row is payable, but the account holds too little of one. */
    insufficient,
};

} // namespace sunrise::state::vendors
