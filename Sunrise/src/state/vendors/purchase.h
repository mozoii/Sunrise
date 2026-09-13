#pragma once

#include "charge.h"

namespace sunrise::state::vendors {

/** Everything one vendor sale row asks of the account beyond the item it grants. */
struct Purchase {
    /** The row's cost, spent with the grant. */
    Charge charge{};
};

} // namespace sunrise::state::vendors
