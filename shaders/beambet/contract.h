// BeamBet Contract Header
#pragma once

namespace BeamBet {

// Shader ID (hash of compiled contract.wasm) - update after each recompile
static const ShaderID s_SID = {0x4a,0x06,0xcf,0x27,0x2f,0x09,0xaa,0xab,0x68,0xb5,0x13,0x2d,0xd4,0xd0,0x62,0x0b,0x08,0x4a,0x47,0x9a,0x1f,0x93,0xbd,0x2b,0x62,0x45,0xbc,0xc5,0x20,0xd0,0x6b,0x9b};

// Constants
static const uint64_t s_DefaultMinBet = 1000000ULL;      // 0.01 BEAM
static const uint64_t s_DefaultMaxBet = 10000000000ULL;  // 100 BEAM
static const uint64_t s_DefaultUpDownMult = 190ULL;      // 1.9x (5% house edge)
static const uint64_t s_DefaultExactMult = 9500ULL;      // 95x (5% house edge on 1/100 chance)
static const uint64_t s_RevealEpoch = 10ULL;             // 10 blocks minimum wait

// Bet status
struct BetStatus {
    enum _ {
        Pending = 0,
        Won = 1,
        Lost = 2,
        Claimed = 3  // Winnings have been claimed
    };
};

// Contract state
struct State {
    PubKey m_OwnerPk;           // Owner's public key
    uint64_t m_MinBet;          // Minimum bet in groth
    uint64_t m_MaxBet;          // Maximum bet in groth
    uint64_t m_UpDownMult;      // Up/Down multiplier (x100 scale, 190 = 1.9x)
    uint64_t m_ExactMult;       // Exact number multiplier (x100 scale, 280 = 2.8x)
    AssetID m_AssetId;          // Asset ID (0 = BEAM)
    uint64_t m_TotalDeposited;  // Total owner deposits (cumulative)
    uint64_t m_TotalBets;       // Total user bets received (cumulative)
    uint64_t m_TotalPayouts;    // Total payouts made to users (cumulative)
    uint64_t m_PendingBets;     // Sum of locked amounts for pending bets
    uint64_t m_PendingMaxPayout;// Sum of max possible payouts for pending bets (solvency reserve)
    uint64_t m_PendingPayouts;  // Sum of payouts for Won-but-not-yet-Claimed bets (owner can't withdraw)
    uint64_t m_NextBetId;       // Next bet ID to assign
    uint64_t m_FirstUnresolvedBetId; // Scan optimization: skip resolved bets
    uint64_t m_RevealEpoch;     // Blocks to wait before reveal
    uint8_t m_Paused;           // Is contract paused?
    uint8_t m_Initialized;      // Is contract initialized?
};

// Bet record
struct Bet {
    uint64_t m_BetId;
    PubKey m_UserPk;            // User's public key
    uint64_t m_Amount;          // Bet amount in groth
    AssetID m_AssetId;          // Asset ID
    uint8_t m_Type;             // 0=Up (>50), 1=Down (<=50), 2=Exact number
    uint8_t m_ExactNumber;      // Exact number guess (1-100), only used for type=2
    uint8_t m_Result;           // Revealed result (1-100)
    uint8_t m_Status;           // BetStatus enum
    uint64_t m_Payout;          // Payout amount if won
    uint64_t m_Multiplier;      // Multiplier locked at placement (x100 scale)
    uint64_t m_MaxPayout;       // Worst-case payout reserved at placement
    uint64_t m_CreatedHeight;   // Block height when bet was placed
    uint64_t m_RevealedHeight;  // Block height when result was revealed
    HashValue m_Commitment;     // User-provided commitment (personalization, cannot be changed after placing)
    HashValue m_PlacementHash;  // Block hash at bet placement (entropy the user cannot predict)
};

// Constructor parameters
struct Params {
    PubKey m_OwnerPk;
};

// Method parameter structures
namespace Method {

struct PlaceBet {
    static const uint32_t s_iMethod = 2;
    PubKey m_UserPk;         // User's public key (passed from app shader)
    uint64_t m_Amount;
    AssetID m_AssetId;
    uint8_t m_Type;
    uint8_t m_ExactNumber;
    HashValue m_Commitment;
};

struct CheckResults {
    static const uint32_t s_iMethod = 3;
    PubKey m_UserPk;         // User's public key (passed from app shader)
    // Checks ALL pending/won bets for this user - reveals and claims
};

struct Deposit {
    static const uint32_t s_iMethod = 4;
    uint64_t m_Amount;
};

struct Withdraw {
    static const uint32_t s_iMethod = 5;
    uint64_t m_Amount;
};

struct SetOwner {
    static const uint32_t s_iMethod = 6;
    PubKey m_OwnerPk;
};

struct SetConfig {
    static const uint32_t s_iMethod = 7;
    uint64_t m_MinBet;
    uint64_t m_MaxBet;
    uint64_t m_UpDownMult;
    uint64_t m_ExactMult;
    uint8_t m_Paused;
    AssetID m_AssetId;
};

// Owner emergency reveal (deterministic - same formula as user reveal)
struct RevealBet {
    static const uint32_t s_iMethod = 8;
    uint64_t m_BetId;
    // Result is computed deterministically; no owner-chosen random value
};

// Check and claim a single bet by ID (user calls)
struct CheckSingleResult {
    static const uint32_t s_iMethod = 9;
    PubKey m_UserPk;    // User's public key (passed from app shader)
    uint64_t m_BetId;   // Specific bet ID to reveal and claim
};

// Resolve expired pending bets (anyone can call - no funds movement, just reveals results)
struct ResolveExpiredBets {
    static const uint32_t s_iMethod = 10;
    uint32_t m_MaxCount;  // Max bets to resolve (capped at 200 internally)
};

} // namespace Method

// Storage tags
struct Tags {
    static const uint8_t s_State = 0;
    static const uint8_t s_Bet = 1;
};

// Storage key structures - MUST be packed to avoid padding in BVM storage keys.
// Without packing, uint8_t + uint64_t has 7 bytes of uninitialized padding,
// causing key mismatches between contract SaveVar_T and app VarReader::Read_T.
#pragma pack(push, 1)
struct StateKey {
    uint8_t m_Tag;
    StateKey() : m_Tag(Tags::s_State) {}
};

struct BetKey {
    uint8_t m_Tag;
    uint64_t m_BetId;
    BetKey() : m_Tag(Tags::s_Bet), m_BetId(0) {}
};
#pragma pack(pop)

} // namespace BeamBet
