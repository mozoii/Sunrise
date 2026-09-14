#include "availability.h"

#include "../account/pursuit_hold.h"
#include "../build_data/vendors/vendor_catalog.h"
#include "../investment/store.h"
#include "rotation.h"

namespace sunrise::state::vendors {

/** Refuses on the first condition that holds, cheapest first. */
bool evaluate(const build_data::vendors::Definition& definition,
              std::uint32_t vendorHash,
              std::size_t row,
              std::int64_t unixSeconds,
              Availability& output) noexcept {
    output = Availability::purchasable;
    build_data::vendors::SaleRow sale{};
    if (!build_data::vendors::sale_row(definition, row, sale)) {
        return false;
    }
    if (!rotation::present(vendorHash, unixSeconds)) {
        output = Availability::vendorAbsent;
        return true;
    }
    if (rotation::weekly_limited(vendorHash, sale.categoryIndex)) {
        bool claimed = false;
        std::uint32_t lastWeek = 0;
        if (!investment::store::read_vendor_rotation(vendorHash, claimed, lastWeek)) {
            return false;
        }
        if (claimed && lastWeek == rotation::week(unixSeconds)) {
            output = Availability::claimedThisWeek;
            return true;
        }
    }
    // The client applies this one itself for the rows it can see it on, and the grant refuses it
    // either way, so the answer has to agree with both.
    if (account::holds_pursuit(sale.itemIndex)) {
        output = Availability::pursuitHeld;
        return true;
    }
    return true;
}

} // namespace sunrise::state::vendors
