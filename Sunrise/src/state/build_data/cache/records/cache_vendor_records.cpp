#include "codec.h"

namespace sunrise::state::build_data::cache::records {

/** Encodes one vendor index row. */
bool encode(const vendors::IndexEntry& value, VendorIndexRecord& record) noexcept {
    record = {};
    record.definitionHash = value.definitionHash;
    record.definitionTag = value.definitionTag;
    record.index = value.index;
    return true;
}

/** Decodes one vendor index row. */
bool decode(const VendorIndexRecord& record, vendors::IndexEntry& value) noexcept {
    value = {};
    if (record.reserved != 0) {
        return false;
    }
    value = {record.definitionHash, record.definitionTag, record.index};
    return true;
}

/** Encodes one vendor definition and its flat-bank ranges. */
bool encode(const vendors::Definition& value, VendorDefinitionRecord& record) noexcept {
    record = {};
    record.definitionHash = value.definitionHash;
    record.definitionTag = value.definitionTag;
    record.definitionClass = value.definitionClass;
    record.definitionSize = value.definitionSize;
    record.installedRowBase = value.installedRowBase;
    record.installedRowClass = value.installedRowClass;
    record.saleRowBase = value.saleRowBase;
    record.saleRowClass = value.saleRowClass;
    record.interactionRowBase = value.interactionRowBase;
    record.interactionRowClass = value.interactionRowClass;
    record.saleRowOffset = value.saleRowOffset;
    record.installedRowOffset = value.installedRowOffset;
    record.interactionRowOffset = value.interactionRowOffset;
    record.resetIntervalRaw = value.resetIntervalRaw;
    record.resetPhaseRaw = value.resetPhaseRaw;
    record.index = value.index;
    record.installedCount = value.installedCount;
    record.saleCount = value.saleCount;
    record.interactionCount = value.interactionCount;
    return true;
}

/** Decodes one vendor definition and its flat-bank ranges. */
bool decode(const VendorDefinitionRecord& record, vendors::Definition& value) noexcept {
    value = {};
    // The catalog checks every range against the whole domain. Only the class is checked here.
    // A row of another class is not a vendor definition, whatever its ranges say.
    if (record.definitionClass != vendors::kDefinitionClass) {
        return false;
    }
    value.definitionHash = record.definitionHash;
    value.definitionTag = record.definitionTag;
    value.definitionClass = record.definitionClass;
    value.definitionSize = record.definitionSize;
    value.installedRowBase = record.installedRowBase;
    value.installedRowClass = record.installedRowClass;
    value.saleRowBase = record.saleRowBase;
    value.saleRowClass = record.saleRowClass;
    value.interactionRowBase = record.interactionRowBase;
    value.interactionRowClass = record.interactionRowClass;
    value.saleRowOffset = record.saleRowOffset;
    value.installedRowOffset = record.installedRowOffset;
    value.interactionRowOffset = record.interactionRowOffset;
    value.resetIntervalRaw = record.resetIntervalRaw;
    value.resetPhaseRaw = record.resetPhaseRaw;
    value.index = record.index;
    value.installedCount = record.installedCount;
    value.saleCount = record.saleCount;
    value.interactionCount = record.interactionCount;
    return true;
}

/** Encodes one vendor sale row. Unused cost rows stay zero so the packed row always matches. */
bool encode(const vendors::SaleRow& value, VendorSaleRowRecord& record) noexcept {
    record = {};
    if (value.costCount > value.costs.size()) {
        return false;
    }
    record.itemIndex = value.itemIndex;
    record.secondaryItemIndex = value.secondaryItemIndex;
    record.categoryIndex = value.categoryIndex;
    record.costCount = value.costCount;
    for (std::size_t cost = 0; cost < value.costCount; ++cost) {
        record.costs[cost].itemIndex = value.costs[cost].itemIndex;
        record.costs[cost].quantity = value.costs[cost].quantity;
    }
    return true;
}

/** Decodes one vendor sale row. */
bool decode(const VendorSaleRowRecord& record, vendors::SaleRow& value) noexcept {
    value = {};
    if (record.reserved != decltype(record.reserved){} || record.costCount > record.costs.size()) {
        return false;
    }
    for (std::size_t cost = 0; cost < record.costs.size(); ++cost) {
        const VendorSaleCostRecord& stored = record.costs[cost];
        if (stored.reserved != 0) {
            return false;
        }
        if (cost >= record.costCount) {
            if (stored.itemIndex != 0 || stored.quantity != 0) {
                return false;
            }
            continue;
        }
        value.costs[cost] = {stored.itemIndex, stored.quantity};
    }
    value.itemIndex = record.itemIndex;
    value.secondaryItemIndex = record.secondaryItemIndex;
    value.categoryIndex = record.categoryIndex;
    value.costCount = record.costCount;
    return true;
}

/** Encodes one vendor category row. */
bool encode(const vendors::InstalledRow& value, VendorInstalledRowRecord& record) noexcept {
    record = {value.definitionHash};
    return true;
}

/** Decodes one vendor category row. */
bool decode(const VendorInstalledRowRecord& record, vendors::InstalledRow& value) noexcept {
    value = {record.definitionHash};
    return true;
}

/** Encodes one vendor interaction row. Unused instructions and indexes stay zero. */
bool encode(const vendors::Interaction& value, VendorInteractionRecord& record) noexcept {
    record = {};
    if (value.programCount > value.program.size()
        || value.failureCount > value.failureIndexes.size()) {
        return false;
    }
    record.hash = value.hash;
    record.categoryIndex = value.categoryIndex;
    record.programCount = value.programCount;
    record.failureCount = value.failureCount;
    for (std::size_t index = 0; index < value.programCount; ++index) {
        record.program[index] = {value.program[index].opcode, value.program[index].operand};
    }
    for (std::size_t index = 0; index < value.failureCount; ++index) {
        record.failureIndexes[index] = value.failureIndexes[index];
    }
    return true;
}

/** Decodes one vendor interaction row. */
bool decode(const VendorInteractionRecord& record, vendors::Interaction& value) noexcept {
    value = {};
    if (record.reserved != decltype(record.reserved){}
        || record.programCount > record.program.size()
        || record.failureCount > record.failureIndexes.size()) {
        return false;
    }
    for (std::size_t index = 0; index < record.program.size(); ++index) {
        const VendorGateInstructionRecord& stored = record.program[index];
        if (index >= record.programCount) {
            if (stored.opcode != 0 || stored.operand != 0) {
                return false;
            }
            continue;
        }
        value.program[index] = {stored.opcode, stored.operand};
    }
    for (std::size_t index = 0; index < record.failureIndexes.size(); ++index) {
        if (index >= record.failureCount) {
            if (record.failureIndexes[index] != 0) {
                return false;
            }
            continue;
        }
        value.failureIndexes[index] = record.failureIndexes[index];
    }
    value.hash = record.hash;
    value.categoryIndex = record.categoryIndex;
    value.programCount = record.programCount;
    value.failureCount = record.failureCount;
    return true;
}

} // namespace sunrise::state::build_data::cache::records
