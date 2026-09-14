#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/definition_index_table.h"
#include "../../../state/build_data/runtime.h"
#include "../../../state/build_data/vendors/definition.h"
#include "layout.h"
#include "vendor_build.h"

namespace sunrise::client::content::vendors {
namespace {

namespace reader = middleware::content::packages::reader;
namespace tables = middleware::content::packages::tables;
namespace domain = state::build_data::vendors;

/** Every extracted row, kept off the caller stack. */
struct Storage {
    std::vector<std::byte> blob{};
    std::array<domain::IndexEntry, domain::kIndexCapacity> index{};
    std::array<domain::Definition, domain::kDefinitionCapacity> definitions{};
    std::array<domain::SaleRow, domain::kSaleRowCapacity> saleRows{};
    std::array<domain::InstalledRow, domain::kInstalledRowCapacity> installedRows{};
    std::array<domain::Interaction, domain::kInteractionRowCapacity> interactions{};
    std::size_t indexCount{};
    std::size_t definitionCount{};
    std::size_t saleRowCount{};
    std::size_t installedRowCount{};
    std::size_t interactionCount{};
};

/** One array a definition or a sale row declares, reduced to what the catalog stores. */
struct ArrayView {
    std::uint32_t base{};
    std::uint32_t classId{};
    std::uint16_t count{};
};

/** @param blob Source bytes. @param offset Field offset. @param value Receives the field. */
template <typename Value>
[[nodiscard]] bool
read(std::span<const std::byte> blob, std::size_t offset, Value& value) noexcept {
    if (offset > blob.size() || blob.size() - offset < sizeof value) {
        return false;
    }
    std::memcpy(&value, blob.data() + offset, sizeof value);
    return true;
}

/**
 * Reads one array descriptor and bounds it against the blob holding it.
 * The raw count is read first, because the shared resolver reports absent and corrupt alike.
 * @param blob Whole blob owning the descriptor.
 * @param descriptor Descriptor offset.
 * @param stride One row's size.
 * @param output Receives the array, or an absent array.
 * @return True when the array is absent, or resolves and ends inside the blob.
 */
[[nodiscard]] bool read_array(std::span<const std::byte> blob,
                              std::size_t descriptor,
                              std::size_t stride,
                              ArrayView& output) noexcept {
    /** Row counts are stored as unsigned 16-bit values. */
    constexpr std::uint64_t kMaximumCount = (std::numeric_limits<std::uint16_t>::max)();
    output = {};
    std::uint64_t declared = 0;
    if (!read(blob, descriptor, declared)) {
        return false;
    }
    if (declared == 0) {
        return true;
    }
    tables::Array array{};
    if (!tables::find_array_at(blob, descriptor, array) || array.count > kMaximumCount) {
        return false;
    }
    const std::uint64_t end = array.dataOffset + (array.count * stride);
    if (end > blob.size()) {
        return false;
    }
    output = {static_cast<std::uint32_t>(array.dataOffset),
              array.elementClass,
              static_cast<std::uint16_t>(array.count)};
    return true;
}

/**
 * Reads what one sale row charges: every row of its price-override array, in order.
 * A row charging nothing declares no override, which is data rather than a malformed row. A row
 * declaring more overrides than the catalog can hold is refused, so a charge is never a subset of
 * what the package asks for.
 * @param blob Whole definition blob.
 * @param at Sale row offset inside the blob.
 * @param value Receives the cost rows, or none.
 * @return True when the array is absent, or resolves, fits, and ends inside the blob.
 */
[[nodiscard]] bool
read_sale_cost(std::span<const std::byte> blob, std::size_t at, domain::SaleRow& value) noexcept {
    value.costs = {};
    value.costCount = 0;
    ArrayView cost{};
    if (!read_array(blob, at + kSaleCostArrayDescriptor, domain::kSaleCostRowStride, cost)) {
        return false;
    }
    if (cost.count == 0) {
        return true;
    }
    if (cost.classId != domain::kSaleCostRowClass || cost.count > value.costs.size()) {
        return false;
    }
    for (std::size_t row = 0; row < cost.count; ++row) {
        const std::size_t rowAt = cost.base + (row * domain::kSaleCostRowStride);
        domain::SaleCost& entry = value.costs[row];
        if (!read(blob, rowAt + kSaleCostItemIndexOffset, entry.itemIndex)
            || !read(blob, rowAt + kSaleCostQuantityOffset, entry.quantity)) {
            return false;
        }
    }
    value.costCount = static_cast<std::uint8_t>(cost.count);
    return true;
}

/**
 * Reads the whole installed vendor index.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param storage Pass storage receiving the index rows.
 * @return True when the index blob reads and every row fits.
 */
[[nodiscard]] bool
read_index(const reader::Source& source, reader::Scratch& scratch, Storage& storage) noexcept {
    std::uint32_t classId = 0;
    tables::Array array{};
    if (!reader::read_tag(source, scratch, kIndexRootTag, storage.blob, classId)
        || classId != domain::kIndexWrapperClass) {
        return false;
    }
    const std::span<const std::byte> blob{storage.blob};
    if (!tables::find_array_at(blob, tables::kTableArrayDescriptor, array)
        || array.elementClass != domain::kIndexRowClass || array.count > domain::kIndexCapacity) {
        return false;
    }
    for (std::uint64_t row = 0; row < array.count; ++row) {
        tables::IndexRow entry{};
        if (!tables::index_row(blob, array, row, entry)) {
            return false;
        }
        storage.index[storage.indexCount] = {
            entry.definitionHash, entry.targetTag, static_cast<std::uint16_t>(row)};
        ++storage.indexCount;
    }
    return storage.indexCount != 0;
}

/**
 * Reads every sale row of one definition into the flat bank.
 * @param blob Whole definition blob.
 * @param definition Definition whose sale array was already resolved.
 * @param storage Pass storage receiving the rows.
 * @return True when every row is inside the blob and the bank holds them all.
 */
[[nodiscard]] bool read_sale_rows(std::span<const std::byte> blob,
                                  const domain::Definition& definition,
                                  Storage& storage) noexcept {
    if (definition.saleCount > domain::kSaleRowCapacity - storage.saleRowCount) {
        return false;
    }
    for (std::size_t row = 0; row < definition.saleCount; ++row) {
        const std::size_t at = definition.saleRowBase + (row * domain::kSaleRowStride);
        domain::SaleRow& value = storage.saleRows[storage.saleRowCount + row];
        value = {};
        if (!read(blob, at + kSaleItemIndexOffset, value.itemIndex)
            || !read(blob, at + kSaleSecondaryItemOffset, value.secondaryItemIndex)
            || !read(blob, at + kSaleCategoryIndexOffset, value.categoryIndex)
            || !read_sale_cost(blob, at, value)) {
            return false;
        }
    }
    storage.saleRowCount += definition.saleCount;
    return true;
}

/**
 * Reads the definition hash of every category row of one definition into the flat bank.
 * @param blob Whole definition blob.
 * @param definition Definition whose installed array was already resolved.
 * @param storage Pass storage receiving the rows.
 * @return True when every row is inside the blob and the bank holds them all.
 */
[[nodiscard]] bool read_installed_rows(std::span<const std::byte> blob,
                                       const domain::Definition& definition,
                                       Storage& storage) noexcept {
    if (definition.installedCount > domain::kInstalledRowCapacity - storage.installedRowCount) {
        return false;
    }
    for (std::size_t row = 0; row < definition.installedCount; ++row) {
        const std::size_t at = definition.installedRowBase + (row * domain::kInstalledRowStride);
        domain::InstalledRow& value = storage.installedRows[storage.installedRowCount + row];
        value = {};
        if (!read(blob, at + kInstalledRowHashOffset, value.definitionHash)) {
            return false;
        }
    }
    storage.installedRowCount += definition.installedCount;
    return true;
}

/**
 * Reads one interaction's unlock expression, in evaluation order.
 * An interaction with no expression declares no array, which is data rather than a malformed
 * row. An expression longer than the catalog can hold is refused, so a gate is never a prefix.
 * @param blob Whole definition blob.
 * @param at Interaction row offset inside the blob.
 * @param value Receives the instructions, or none.
 * @return True when the array is absent, or resolves, fits, and ends inside the blob.
 */
[[nodiscard]] bool read_interaction_program(std::span<const std::byte> blob,
                                            std::size_t at,
                                            domain::Interaction& value) noexcept {
    value.program = {};
    value.programCount = 0;
    ArrayView program{};
    if (!read_array(blob,
                    at + kInteractionProgramDescriptor,
                    domain::kInteractionInstructionStride,
                    program)) {
        return false;
    }
    if (program.count == 0) {
        return true;
    }
    if (program.classId != domain::kInteractionProgramClass
        || program.count > value.program.size()) {
        return false;
    }
    for (std::size_t row = 0; row < program.count; ++row) {
        const std::size_t rowAt = program.base + (row * domain::kInteractionInstructionStride);
        domain::GateInstruction& instruction = value.program[row];
        if (!read(blob, rowAt + kInteractionOpcodeOffset, instruction.opcode)
            || !read(blob, rowAt + kInteractionOperandOffset, instruction.operand)) {
            return false;
        }
    }
    value.programCount = static_cast<std::uint8_t>(program.count);
    return true;
}

/**
 * Reads the failure-string indexes one interaction shows when its expression refuses.
 * @param blob Whole definition blob.
 * @param at Interaction row offset inside the blob.
 * @param value Receives the indexes, or none.
 * @return True when the array is absent, or resolves, fits, and ends inside the blob.
 */
[[nodiscard]] bool read_interaction_failures(std::span<const std::byte> blob,
                                             std::size_t at,
                                             domain::Interaction& value) noexcept {
    value.failureIndexes = {};
    value.failureCount = 0;
    ArrayView failures{};
    if (!read_array(blob,
                    at + kInteractionFailureDescriptor,
                    domain::kInteractionFailureStride,
                    failures)) {
        return false;
    }
    if (failures.count == 0) {
        return true;
    }
    if (failures.classId != domain::kInteractionFailureClass
        || failures.count > value.failureIndexes.size()) {
        return false;
    }
    for (std::size_t row = 0; row < failures.count; ++row) {
        const std::size_t rowAt = failures.base + (row * domain::kInteractionFailureStride);
        std::uint32_t index = 0;
        if (!read(blob, rowAt + kInteractionFailureIndexOffset, index)
            || index > (std::numeric_limits<std::uint16_t>::max)()) {
            return false;
        }
        value.failureIndexes[row] = static_cast<std::uint16_t>(index);
    }
    value.failureCount = static_cast<std::uint8_t>(failures.count);
    return true;
}

/**
 * Reads every interaction row of one definition into the flat bank.
 * @param blob Whole definition blob.
 * @param definition Definition whose interaction array was already resolved.
 * @param storage Pass storage receiving the rows.
 * @return True when every row is inside the blob and the bank holds them all.
 */
[[nodiscard]] bool read_interaction_rows(std::span<const std::byte> blob,
                                         const domain::Definition& definition,
                                         Storage& storage) noexcept {
    if (definition.interactionCount > domain::kInteractionRowCapacity - storage.interactionCount) {
        return false;
    }
    for (std::size_t row = 0; row < definition.interactionCount; ++row) {
        const std::size_t at =
            definition.interactionRowBase + (row * domain::kInteractionRowStride);
        domain::Interaction& value = storage.interactions[storage.interactionCount + row];
        value = {};
        if (!read(blob, at + kInteractionHashOffset, value.hash)
            || !read(blob, at + kInteractionCategoryOffset, value.categoryIndex)
            || !read_interaction_program(blob, at, value)
            || !read_interaction_failures(blob, at, value)) {
            return false;
        }
    }
    storage.interactionCount += definition.interactionCount;
    return true;
}

/**
 * Reads one vendor definition and both of its row arrays.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param entry Index row naming the definition.
 * @param storage Pass storage receiving the definition and its rows.
 * @return True when the definition blob reads and every array ends inside it.
 */
[[nodiscard]] bool read_definition(const reader::Source& source,
                                   reader::Scratch& scratch,
                                   const domain::IndexEntry& entry,
                                   Storage& storage) noexcept {
    /** Definition sizes are stored as unsigned 32-bit values. */
    constexpr std::size_t kMaximumSize = (std::numeric_limits<std::uint32_t>::max)();
    std::uint32_t classId = 0;
    if (storage.definitionCount == domain::kDefinitionCapacity
        || !reader::read_tag(source, scratch, entry.definitionTag, storage.blob, classId)
        || classId != domain::kDefinitionClass || storage.blob.size() > kMaximumSize) {
        return false;
    }
    const std::span<const std::byte> blob{storage.blob};
    ArrayView installed{};
    ArrayView sale{};
    ArrayView interactions{};
    if (!read_array(blob, kInstalledArrayDescriptor, domain::kInstalledRowStride, installed)
        || !read_array(blob, kSaleArrayDescriptor, domain::kSaleRowStride, sale)
        || !read_array(
            blob, kInteractionArrayDescriptor, domain::kInteractionRowStride, interactions)) {
        return false;
    }
    domain::Definition definition{};
    definition.definitionHash = entry.definitionHash;
    definition.definitionTag = entry.definitionTag;
    definition.definitionClass = classId;
    definition.definitionSize = static_cast<std::uint32_t>(blob.size());
    definition.index = entry.index;
    definition.installedRowBase = installed.base;
    definition.installedRowClass = installed.classId;
    definition.installedCount = installed.count;
    definition.saleRowBase = sale.base;
    definition.saleRowClass = sale.classId;
    definition.saleCount = sale.count;
    definition.interactionRowBase = interactions.base;
    definition.interactionRowClass = interactions.classId;
    definition.interactionCount = interactions.count;
    definition.saleRowOffset = static_cast<std::uint32_t>(storage.saleRowCount);
    definition.installedRowOffset = static_cast<std::uint32_t>(storage.installedRowCount);
    definition.interactionRowOffset = static_cast<std::uint32_t>(storage.interactionCount);
    // A skipped definition must leave every bank exactly as it found it; an orphan sale row
    // shifts the next definition's offset and `valid()` then rejects the whole set.
    const std::size_t saleRowsBefore = storage.saleRowCount;
    const std::size_t installedRowsBefore = storage.installedRowCount;
    const std::size_t interactionsBefore = storage.interactionCount;
    if (!read(blob, kResetIntervalOffset, definition.resetIntervalRaw)
        || !read(blob, kResetPhaseOffset, definition.resetPhaseRaw)
        || !read_sale_rows(blob, definition, storage)
        || !read_installed_rows(blob, definition, storage)
        || !read_interaction_rows(blob, definition, storage)) {
        storage.saleRowCount = saleRowsBefore;
        storage.installedRowCount = installedRowsBefore;
        storage.interactionCount = interactionsBefore;
        return false;
    }
    storage.definitions[storage.definitionCount] = definition;
    ++storage.definitionCount;
    return true;
}

/**
 * Reports the pass so a boot with no vendor catalog says which step lost the rows.
 * @param storage Pass storage holding every count.
 * @param skipped Requested definitions that could not be read or could not fit.
 * @param result Outcome text for the log line.
 */
void report(const Storage& storage, std::size_t skipped, const char* result) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=build_data stage=vendors index=%zu definitions=%zu "
                                      "sale=%zu installed=%zu skipped=%zu result=%s",
                                      storage.indexCount,
                                      storage.definitionCount,
                                      storage.saleRowCount,
                                      storage.installedRowCount,
                                      skipped,
                                      result);
    if (written > 0) {
        core::log::write(core::log::Channel::state,
                         storage.indexCount != 0 && skipped == 0 ? core::log::Level::info
                                                                 : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

} // namespace

/** Extracts and publishes the vendor catalog from the installed packages. */
bool build(const reader::Source& source, reader::Scratch& scratch) noexcept {
    if (state::build_data::vendor_catalog_ready()) {
        return true;
    }
    static Storage storage{};
    storage = {};
    if (!read_index(source, scratch, storage)) {
        report(storage, 0, "index");
        return false;
    }
    // Walk the index in order: the catalog requires ascending definition order. A definition that
    // will not read or will not fit costs that vendor alone, never the whole pass.
    std::size_t skipped = 0;
    for (std::size_t row = 0; row < storage.indexCount; ++row) {
        const domain::IndexEntry entry = storage.index[row];
        if (read_definition(source, scratch, entry, storage)) {
            continue;
        }
        ++skipped;
        core::log::writef(core::log::Channel::state,
                          core::log::Level::warn,
                          "ev=build_data stage=vendors result=skip hash=0x%08X row=%zu "
                          "definitions=%zu sale=%zu",
                          entry.definitionHash,
                          row,
                          storage.definitionCount,
                          storage.saleRowCount);
    }
    const bool published = state::build_data::publish_vendor_catalog(
        std::span(storage.index).first(storage.indexCount),
        std::span(storage.definitions).first(storage.definitionCount),
        std::span(storage.saleRows).first(storage.saleRowCount),
        std::span(storage.installedRows).first(storage.installedRowCount),
        std::span(storage.interactions).first(storage.interactionCount));
    report(storage, skipped, published ? "ok" : "publish");
    return published;
}

} // namespace sunrise::client::content::vendors
