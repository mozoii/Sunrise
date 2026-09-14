#pragma once

#include <cstddef>
#include <cstdint>

#include "../build_data/vendors/definition.h"

namespace sunrise::state::vendors {

/**
 * Why one vendor sale row cannot be bought right now, decided before anything is prepared.
 * Every reason is a property of the row and the account, never of one vendor, so a rule added
 * here applies to every vendor that meets it.
 */
enum class Availability : std::uint8_t {
    /** Nothing this policy knows about refuses the row. The charge may still refuse it later. */
    purchasable,
    /** The vendor's rotation policy keeps it out of town right now. */
    vendorAbsent,
    /** A row its vendor limits to one purchase per weekly reset was already bought this week. */
    claimedThisWeek,
    /** The row sells a pursuit the selected character already holds, and pursuits never stack. */
    pursuitHeld,
};

/**
 * Names one refusal for a log line.
 * The keys match the reasons the purchase path already reports, so existing log filters and the
 * failure text a row would show keep naming the same condition.
 * @param value Refusal to name.
 * @return A short stable key, or "ok" when the row is purchasable.
 */
[[nodiscard]] constexpr const char* reason(Availability value) noexcept {
    switch (value) {
    case Availability::vendorAbsent:
        return "vendor_absent";
    case Availability::claimedThisWeek:
        return "engram_this_week";
    case Availability::pursuitHeld:
        return "already_held";
    case Availability::purchasable:
        break;
    }
    return "ok";
}

/**
 * Decides whether one sale row of one vendor can be bought right now.
 * This is the one place the answer is derived. The purchase path refuses on it, and anything that
 * needs to show a row's state ahead of a purchase reads the same answer rather than its own.
 * @param definition Vendor definition owning the row.
 * @param vendorHash Definition hash of that vendor, which is what the rotation policy is keyed by.
 * @param row Sale row ordinal within the definition.
 * @param unixSeconds Server clock.
 * @param output Receives the verdict, and is left purchasable when the row could not be read.
 * @return False when the row or the rotation store could not be read, which is not a verdict.
 */
[[nodiscard]] bool evaluate(const build_data::vendors::Definition& definition,
                            std::uint32_t vendorHash,
                            std::size_t row,
                            std::int64_t unixSeconds,
                            Availability& output) noexcept;

} // namespace sunrise::state::vendors
