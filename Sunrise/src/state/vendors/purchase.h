#pragma once

#include "charge.h"
#include "rotation.h"

namespace sunrise::state::vendors {

/** Everything one vendor sale row asks of the account beyond the item it grants. */
struct Purchase {
    /** The row's cost, spent with the grant. */
    Charge charge{};
    /** Weekly claim recorded with the grant; a zero vendor hash records nothing. */
    rotation::Claim claim{};
};

} // namespace sunrise::state::vendors
