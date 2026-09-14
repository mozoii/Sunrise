#pragma once

#include <cstdint>

#include "investment.h"

namespace sunrise::state::investment {

/**
 * Sets one flag-override slot of a candidate family-5 state and drops duplicate rows for it.
 * @param family Candidate state. Unchanged when the list cannot hold a wanted new row.
 * @param slot Flag slot, which is what an unlock expression names.
 * @param value Logical flag byte to publish.
 * @param appendMissing False leaves a slot with no row absent, so the client reads its default.
 * @return False when the slot is new, wanted, and the list is full.
 */
[[nodiscard]] bool upsert_flag_override(Family5State& family,
                                        std::uint16_t slot,
                                        std::uint8_t value,
                                        bool appendMissing) noexcept;

} // namespace sunrise::state::investment
