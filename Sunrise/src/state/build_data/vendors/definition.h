#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::state::build_data::vendors {

/** Rows of the installed vendor index. The live table has 511. */
inline constexpr std::size_t kIndexCapacity = 512;
/** Every index row gets a definition, so the two capacities are the same. */
inline constexpr std::size_t kDefinitionCapacity = kIndexCapacity;
/** Sale rows across every definition. The live total is 15,768 and one vendor declares 1,044. */
inline constexpr std::size_t kSaleRowCapacity = 16'384;
/** Category rows across every definition. The live total is 2,479 and one vendor declares 129. */
inline constexpr std::size_t kInstalledRowCapacity = 4'096;

/** One vendor index row is 24 bytes: hash at +0, definition tag at +16. */
inline constexpr std::size_t kIndexRowStride = 24;
/** One sale row is 184 bytes. */
inline constexpr std::size_t kSaleRowStride = 184;
/** One installed row is 24 bytes. */
inline constexpr std::size_t kInstalledRowStride = 24;
/** One interaction row is 80 bytes. */
inline constexpr std::size_t kInteractionRowStride = 80;
/**
 * Interaction rows across every definition. Observed: 41 of the 511 vendors declare any, about
 * 755 in all, and the largest vendor declares 57.
 */
inline constexpr std::size_t kInteractionRowCapacity = 1'024;
/** One unlock-expression instruction is 8 bytes: the opcode, then its operand. */
inline constexpr std::size_t kInteractionInstructionStride = 8;
/** Instructions one interaction's unlock expression may hold. The longest observed is 36. */
inline constexpr std::size_t kInteractionProgramCapacity = 48;
/** One failure-index row is 8 bytes, the index in its low word. */
inline constexpr std::size_t kInteractionFailureStride = 8;
/** Failure indexes one interaction may name. The most observed is 2. */
inline constexpr std::size_t kInteractionFailureCapacity = 4;
/** One price-override row is 48 bytes: the cost item index, then the units it charges. */
inline constexpr std::size_t kSaleCostRowStride = 48;
/** Element class of a sale row's price-override array. */
inline constexpr std::uint32_t kSaleCostRowClass = 0x80807865U;

/** Wrapper class of the vendor index blob. */
inline constexpr std::uint32_t kIndexWrapperClass = 0x8080784AU;
/** Element class of the vendor index array. */
inline constexpr std::uint32_t kIndexRowClass = 0x8080784EU;
/** Class of a vendor definition blob. */
inline constexpr std::uint32_t kDefinitionClass = 0x80807850U;
/** Element class of a definition's installed array. */
inline constexpr std::uint32_t kInstalledRowClass = 0x80807860U;
/** Element class of a definition's sale array. */
inline constexpr std::uint32_t kSaleRowClass = 0x80807861U;
/** Element class of a definition's interaction array. */
inline constexpr std::uint32_t kInteractionRowClass = 0x80807857U;
/** Element class of an interaction's unlock-expression array. */
inline constexpr std::uint32_t kInteractionProgramClass = 0x80807D31U;
/** Element class of an interaction's failure-index array. */
inline constexpr std::uint32_t kInteractionFailureClass = 0x8080785BU;

/** Sale row +176 carries this when the row names no secondary item. */
inline constexpr std::uint16_t kAbsentSecondaryItem = 0xFFFFU;
/** Sale row +100 carries this when the row belongs to no category. The client tests for it. */
inline constexpr std::int32_t kAbsentCategoryIndex = -1;
/** Interaction row +56 carries this when the interaction serves no category. */
inline constexpr std::int32_t kAbsentInteractionCategory = -1;

/** One row of the installed vendor index, which maps a vendor hash to its definition tag. */
struct IndexEntry {
    std::uint32_t definitionHash{};
    std::uint32_t definitionTag{};
    /** Row position, which is the index the opcode-901 request carries. */
    std::uint16_t index{};
};

/** One extracted vendor definition and the flat-bank ranges its rows occupy. */
struct Definition {
    std::uint32_t definitionHash{};
    std::uint32_t definitionTag{};
    /** Class the package entry records, which must be `kDefinitionClass`. */
    std::uint32_t definitionClass{};
    /** Definition blob size. Every array must end inside it. */
    std::uint32_t definitionSize{};
    /** First installed row, as an offset into the definition blob. */
    std::uint32_t installedRowBase{};
    std::uint32_t installedRowClass{};
    /** First sale row, as an offset into the definition blob. */
    std::uint32_t saleRowBase{};
    std::uint32_t saleRowClass{};
    /** First interaction row, as an offset into the definition blob. */
    std::uint32_t interactionRowBase{};
    std::uint32_t interactionRowClass{};
    /** First row of this definition's range in the flat sale bank. */
    std::uint32_t saleRowOffset{};
    /** First row of this definition's range in the flat installed bank. */
    std::uint32_t installedRowOffset{};
    /** First row of this definition's range in the flat interaction bank. */
    std::uint32_t interactionRowOffset{};
    /** Raw definition +20. Its unit, epoch and scope are open, so it is not converted. */
    std::uint32_t resetIntervalRaw{};
    /** Raw definition +24, paired with the interval and equally open. */
    std::uint32_t resetPhaseRaw{};
    /** Row of the vendor index this definition is named by. */
    std::uint16_t index{};
    std::uint16_t installedCount{};
    std::uint16_t saleCount{};
    std::uint16_t interactionCount{};
};

/** A price-override row naming no item carries this. */
inline constexpr std::uint16_t kAbsentCostItem = 0xFFFFU;
/** Price-override rows one sale row may declare. A row declaring more is refused, not truncated. */
inline constexpr std::size_t kSaleCostCapacity = 4;

/**
 * One price-override row (sale row +32 array, `kSaleCostRowClass`): what the sale charges.
 * Observed on Xûr's definition: item 128 with 29, 23, 97 and 9 units, which are exactly the
 * Legendary Shard prices of his weapon, armour, Fated Engram and Invitation of the Nine rows.
 */
struct SaleCost {
    /** Override row +0. Cost item-definition index. */
    std::uint16_t itemIndex{kAbsentCostItem};
    /** Override row +4. Units charged. */
    std::uint32_t quantity{};
};

/** One sale row of one vendor definition. */
struct SaleRow {
    /** Row +100. The row's vendor category. The catalog bounds it by the category count. */
    std::int32_t categoryIndex{};
    /** Row +70. Main sale item-definition index. */
    std::uint16_t itemIndex{};
    /** Row +176. `kAbsentSecondaryItem` when the row names none. */
    std::uint16_t secondaryItemIndex{};
    /** Every price-override row, in declared order. The row charges all of them together. */
    std::array<SaleCost, kSaleCostCapacity> costs{};
    /** Rows of `costs` in use. Zero when the row charges nothing. */
    std::uint8_t costCount{};
};

/** One category row, reduced to the definition hash a rowless request resolves through. */
struct InstalledRow {
    std::uint32_t definitionHash{};
};

/** One unlock-expression instruction, as authored. An opcode that takes no operand leaves it. */
struct GateInstruction {
    std::uint32_t opcode{};
    std::uint32_t operand{};
};

/**
 * One interaction row of one vendor definition: a dialog the vendor offers, with the unlock
 * expression the client evaluates before offering it and the failure strings it shows when the
 * expression refuses.
 * The expression is a visibility gate rather than a purchasability one: rows behind a failed gate
 * are not drawn, and a category that loses every row goes with them. Observed on Xûr's definition,
 * where the interaction serving his engram category tests one flag and negates it, and names
 * failure index 3, the same index both of his engram sale rows carry.
 */
struct Interaction {
    /** Row +4. Hash naming the interaction. */
    std::uint32_t hash{};
    /** Row +56. Vendor category the interaction serves, or `kAbsentInteractionCategory`. */
    std::int32_t categoryIndex{kAbsentInteractionCategory};
    /** Row +8 array, `kInteractionProgramClass`: the unlock expression, in evaluation order. */
    std::array<GateInstruction, kInteractionProgramCapacity> program{};
    /** Row +40 array, `kInteractionFailureClass`: failure-string indexes the refusal shows. */
    std::array<std::uint16_t, kInteractionFailureCapacity> failureIndexes{};
    /** Instructions of `program` in use. Zero when the interaction declares no expression. */
    std::uint8_t programCount{};
    /** Entries of `failureIndexes` in use. */
    std::uint8_t failureCount{};
};

} // namespace sunrise::state::build_data::vendors
