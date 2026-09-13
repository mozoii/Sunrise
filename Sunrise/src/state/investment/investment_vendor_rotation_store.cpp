#include "store_internal.h"

namespace sunrise::state::investment::store {

/** A vendor without a row has never sold its weekly engram, which is not a read failure. */
bool read_vendor_rotation(std::uint32_t vendorHash,
                          bool& found,
                          std::uint32_t& lastEngramWeek) noexcept {
    const std::lock_guard lock(g_mutex);
    found = false;
    lastEngramWeek = 0;
    Statement row("SELECT last_engram_week FROM vendor_rotation WHERE vendor_hash=?");
    if (!row.parameters(vendorHash)) {
        return false;
    }
    const int result = row.step();
    if (result == SQLITE_DONE) {
        return true;
    }
    found = result == SQLITE_ROW && row.column(0, lastEngramWeek);
    return found;
}

/** The caller commits this mark together with the grant it records. */
bool write_vendor_rotation(std::uint32_t vendorHash, std::uint32_t lastEngramWeek) noexcept {
    const std::lock_guard lock(g_mutex);
    Statement row("INSERT OR REPLACE INTO vendor_rotation VALUES (?,?)");
    return row.write(vendorHash, lastEngramWeek);
}

} // namespace sunrise::state::investment::store
