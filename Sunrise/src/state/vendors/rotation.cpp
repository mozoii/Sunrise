#include "rotation.h"

#include <cstdint>

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

} // namespace sunrise::state::vendors::rotation
