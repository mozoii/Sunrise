#pragma once

#include <cstdint>

#include "../investment/investment.h"

namespace sunrise::state::vendors::rotation {

/** Xûr. Same hash space as the Bungie.net manifest (2190858386). */
inline constexpr std::uint32_t kXurVendorHash = 0x8295D892U;
/**
 * Xûr's sale-row category whose rows sell the Fated Engram. Observed in this build's definition:
 * the category-0 row charges 97 Legendary Shards, the engram's documented price, and every other
 * category charges a weapon, armour or bounty price. One engram is sold per week.
 */
inline constexpr std::int32_t kXurEngramCategory = 0;
/** Friday 2020-07-03 17:00 UTC: one weekly reset inside the build's own season. Server policy. */
inline constexpr std::int64_t kEpochSeconds = 1'593'795'600;
inline constexpr std::int64_t kWeekSeconds = 7 * 24 * 60 * 60;
/** Friday reset to Tuesday reset. */
inline constexpr std::int64_t kVisibleSeconds = 4 * 24 * 60 * 60;

/**
 * A weekly claim a grant records when it commits, so the week is spent only if the item lands.
 * A zero vendor hash records nothing.
 */
struct Claim {
    std::uint32_t vendorHash{};
    std::uint32_t week{};
};

/**
 * @return True when this vendor comes and goes under this policy.
 * Which rows such a vendor shows each week is the client's own choice, made from the definition's
 * weekly reset interval; the server only decides whether the vendor is in town and how often its
 * engram row may be bought.
 */
[[nodiscard]] constexpr bool rotates(std::uint32_t vendorHash) noexcept {
    return vendorHash == kXurVendorHash;
}

/**
 * @param vendorHash Vendor being asked about.
 * @param categoryIndex Sale-row category within that vendor.
 * @return True when rows of that category may be bought once per weekly reset.
 */
[[nodiscard]] constexpr bool weekly_limited(std::uint32_t vendorHash,
                                            std::int32_t categoryIndex) noexcept {
    return rotates(vendorHash) && categoryIndex == kXurEngramCategory;
}

/** @return Weeks since the epoch; zero before it. */
[[nodiscard]] std::uint32_t week(std::int64_t unixSeconds) noexcept;

/** @return True from the Friday reset until the Tuesday reset of the same week. */
[[nodiscard]] bool present(std::uint32_t vendorHash, std::int64_t unixSeconds) noexcept;

/**
 * Publishes what a rotating vendor's own unlock expressions read: the flag its engram interaction
 * tests is set for the week the engram was sold, and clear again after the reset. The client
 * evaluates that gate itself and stops drawing the rows behind it, so a spent week takes the
 * section off the vendor screen rather than leaving a row that only fails once it is clicked.
 * @param family Candidate family-5 state; unchanged unless every override fits.
 * @param unixSeconds Server clock.
 * @return False when the rotation store could not be read or the override list is full.
 */
[[nodiscard]] bool append_investment_overrides(Family5State& family,
                                               std::int64_t unixSeconds) noexcept;

} // namespace sunrise::state::vendors::rotation
