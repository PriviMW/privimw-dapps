// BeamBet Contract Shader
#include "common.h"
#include "contract.h"

namespace BeamBet {

// Global state (singleton)
static State g_State;

// Load state on first access
State& GetState() {
    if (!g_State.m_Initialized) {
        StateKey k;
        if (!Env::LoadVar_T(k, g_State)) {
            Env::Halt();
        }
        g_State.m_Initialized = 1;
    }
    return g_State;
}

// Save state
void SaveState() {
    StateKey k;
    Env::SaveVar_T(k, g_State);
}

// Calculate available pool balance (safe, no underflow)
// Available = total_in_contract - pending_max_payout - pending_payouts
// where total_in_contract = deposited + user_bets - already_paid_out
uint64_t GetAvailableBalance() {
    State& s = GetState();
    uint64_t total = s.m_TotalDeposited + s.m_TotalBets;
    if (s.m_TotalPayouts > total) return 0;
    total -= s.m_TotalPayouts;

    // Reserved: pending bets' max possible payouts + won-but-unclaimed payouts
    uint64_t reserved = s.m_PendingMaxPayout + s.m_PendingPayouts;
    if (reserved > total) return 0;
    return total - reserved;
}

// Calculate deterministic random from commitment + placement block hash + reveal height + bet ID
// Returns a value 1-100
// Security: placementHash is the block hash at bet placement time, which is unknowable
// to the user when constructing the PlaceBet transaction (circular dependency on block hash).
uint8_t CalculateRandomResult(const HashValue& commitment, const HashValue& placementHash, Height revealHeight, uint64_t betId) {
    HashProcessor::Sha256 hp;
    hp.Write(commitment);
    hp.Write(placementHash);
    hp.Write(&revealHeight, sizeof(revealHeight));
    hp.Write(&betId, sizeof(betId));

    HashValue resultHash;
    hp >> resultHash;

    // Use first 2 bytes for better distribution
    uint16_t rawValue = (static_cast<uint16_t>(resultHash.m_p[0]) << 8) | resultHash.m_p[1];
    return (rawValue % 100) + 1;  // 1-100
}

// Determine if bet is won
bool IsBetWon(const Bet& b, uint8_t result) {
    switch (b.m_Type) {
        case 0: return (result > 50);                    // Up (>50)
        case 1: return (result < 51);                    // Down (<=50)
        case 2: return (result == b.m_ExactNumber);      // Exact match
        default: return false;
    }
}

// Process bet result: sets Won/Lost status, updates pending tracking
// Must be followed by claiming (FundsUnlock) if Won
void ProcessBetResult(Bet& b, uint8_t result, State& s) {
    b.m_Result = result;
    b.m_RevealedHeight = Env::get_Height();

    if (IsBetWon(b, result)) {
        b.m_Status = BetStatus::Won;
        b.m_Payout = (b.m_Amount * b.m_Multiplier) / 100;  // Use multiplier locked at placement
        s.m_PendingPayouts += b.m_Payout;
    } else {
        b.m_Status = BetStatus::Lost;
        b.m_Payout = 0;
    }

    // Release from pending bets tracking (safe decrement)
    if (b.m_Amount <= s.m_PendingBets)
        s.m_PendingBets -= b.m_Amount;
    else
        s.m_PendingBets = 0;

    // Release the max-payout reservation using stored value (immune to config changes)
    if (b.m_MaxPayout <= s.m_PendingMaxPayout)
        s.m_PendingMaxPayout -= b.m_MaxPayout;
    else
        s.m_PendingMaxPayout = 0;
}

// Accumulate claim accounting: marks bet Claimed, adds to totalPayout. Caller does FundsUnlock.
void AccumulateClaim(Bet& b, State& s, uint64_t& totalPayout) {
    if (b.m_Status == BetStatus::Won && b.m_Payout > 0) {
        totalPayout += b.m_Payout;
        s.m_TotalPayouts += b.m_Payout;
        if (b.m_Payout <= s.m_PendingPayouts)
            s.m_PendingPayouts -= b.m_Payout;
        else
            s.m_PendingPayouts = 0;
        b.m_Status = BetStatus::Claimed;
    }
}

// Advance m_FirstUnresolvedBetId past resolved bets (capped per call for charge safety)
void AdvanceFirstUnresolved(State& s, uint32_t maxAdvance = 50) {
    uint32_t count = 0;
    while (s.m_FirstUnresolvedBetId < s.m_NextBetId && count < maxAdvance) {
        BetKey bk;
        bk.m_BetId = s.m_FirstUnresolvedBetId;
        Bet b;
        if (!Env::LoadVar_T(bk, b)) {
            s.m_FirstUnresolvedBetId++;
            count++;
            continue;
        }
        if (b.m_Status == BetStatus::Pending || b.m_Status == BetStatus::Won)
            break;
        s.m_FirstUnresolvedBetId++;
        count++;
    }
}

} // namespace BeamBet

// ============================================================================
// Constructor
// ============================================================================
BEAM_EXPORT void Ctor(const BeamBet::Params& r)
{
    _POD_(BeamBet::g_State).SetZero();
    BeamBet::g_State.m_MinBet = BeamBet::s_DefaultMinBet;
    BeamBet::g_State.m_MaxBet = BeamBet::s_DefaultMaxBet;
    BeamBet::g_State.m_UpDownMult = BeamBet::s_DefaultUpDownMult;
    BeamBet::g_State.m_ExactMult = BeamBet::s_DefaultExactMult;
    BeamBet::g_State.m_RevealEpoch = BeamBet::s_RevealEpoch;
    BeamBet::g_State.m_AssetId = 0;

    _POD_(BeamBet::g_State.m_OwnerPk) = r.m_OwnerPk;

    BeamBet::g_State.m_Initialized = 1;
    BeamBet::SaveState();
}

// ============================================================================
// Destructor - Owner only, requires no active obligations
// ============================================================================
BEAM_EXPORT void Dtor(void*)
{
    BeamBet::State& s = BeamBet::GetState();

    // Prevent destruction while any user funds are at risk
    if (s.m_PendingBets > 0) Env::Halt();       // Pending bets exist
    if (s.m_PendingMaxPayout > 0) Env::Halt();   // Max payout reserves exist
    if (s.m_PendingPayouts > 0) Env::Halt();     // Won-but-unclaimed payouts exist

    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 2: Place Bet (Anyone can call)
// ============================================================================
BEAM_EXPORT void Method_2(const BeamBet::Method::PlaceBet& r)
{
    BeamBet::State& s = BeamBet::GetState();

    // Basic validation
    if (s.m_Paused) Env::Halt();
    if (r.m_Amount < s.m_MinBet || r.m_Amount > s.m_MaxBet) Env::Halt();

    // Asset must match contract's configured asset
    if (r.m_AssetId != s.m_AssetId) Env::Halt();

    // Bet type must be valid (0=Up, 1=Down, 2=Exact)
    if (r.m_Type > 2) Env::Halt();

    // Exact number must be in range 1-100 for type=2
    if (r.m_Type == 2 && (r.m_ExactNumber < 1 || r.m_ExactNumber > 100)) Env::Halt();

    // Reserve pool capacity for this bet's actual max payout (multiplier is locked at placement)
    uint64_t mult = (r.m_Type == 2) ? s.m_ExactMult : s.m_UpDownMult;
    uint64_t maxPayout = (r.m_Amount * mult) / 100;
    if (BeamBet::GetAvailableBalance() < maxPayout) Env::Halt();

    // Capture block hash at placement time - this is unknowable to the user when
    // constructing the transaction (it depends on the block that includes this tx).
    // Used as entropy in the result calculation to prevent precomputation attacks.
    // m_Height must be set as input before calling get_HdrInfo.
    BlockHeader::Info hdr;
    hdr.m_Height = Env::get_Height();
    Env::get_HdrInfo(hdr);

    // Create bet record
    BeamBet::Bet b;
    _POD_(b).SetZero();
    b.m_BetId = s.m_NextBetId++;
    _POD_(b.m_UserPk) = r.m_UserPk;
    b.m_Amount = r.m_Amount;
    b.m_AssetId = r.m_AssetId;
    b.m_Type = r.m_Type;
    b.m_ExactNumber = r.m_ExactNumber;
    _POD_(b.m_Commitment) = r.m_Commitment;
    _POD_(b.m_PlacementHash) = hdr.m_Hash;
    b.m_Status = BeamBet::BetStatus::Pending;
    b.m_CreatedHeight = Env::get_Height();
    b.m_Payout = 0;
    b.m_Multiplier = mult;
    b.m_MaxPayout = maxPayout;

    // Lock user's funds into the contract
    Env::FundsLock(r.m_AssetId, r.m_Amount);

    // Update state accounting
    s.m_TotalBets += r.m_Amount;
    s.m_PendingBets += r.m_Amount;
    s.m_PendingMaxPayout += maxPayout;

    // Save bet and state
    BeamBet::BetKey bk;
    bk.m_BetId = b.m_BetId;
    Env::SaveVar_T(bk, b);

    BeamBet::SaveState();
}

// ============================================================================
// Method 3: Check Results (User calls to reveal AND claim all their bets)
// Processes: PENDING bets (reveal + claim) and WON bets (claim only)
// ============================================================================
BEAM_EXPORT void Method_3(const BeamBet::Method::CheckResults& r)
{
    BeamBet::State& s = BeamBet::GetState();
    Height hCurrent = Env::get_Height();

    const PubKey& userPk = r.m_UserPk;

    uint64_t totalPayout = 0;
    AssetID assetId = s.m_AssetId;
    uint32_t processedCount = 0;

    // Start from FirstUnresolvedBetId — skip bets already Lost/Claimed
    for (uint64_t betId = s.m_FirstUnresolvedBetId; betId < s.m_NextBetId && processedCount < 100; betId++) {
        BeamBet::BetKey bk;
        bk.m_BetId = betId;

        BeamBet::Bet b;
        if (!Env::LoadVar_T(bk, b)) continue;

        if (_POD_(b.m_UserPk) != userPk) continue;

        if (b.m_Status == BeamBet::BetStatus::Pending) {
            if (hCurrent < b.m_CreatedHeight + s.m_RevealEpoch) continue;

            Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
            uint8_t result = BeamBet::CalculateRandomResult(b.m_Commitment, b.m_PlacementHash, revealHeight, b.m_BetId);
            BeamBet::ProcessBetResult(b, result, s);
        }
        else if (b.m_Status == BeamBet::BetStatus::Won) {
            // Already revealed — just claim below
        }
        else {
            continue;
        }

        BeamBet::AccumulateClaim(b, s, totalPayout);

        Env::SaveVar_T(bk, b);
        processedCount++;
    }

    BeamBet::AdvanceFirstUnresolved(s);
    BeamBet::SaveState();

    // User must prove ownership of the claiming key (prevents bet theft)
    Env::AddSig(r.m_UserPk);

    if (totalPayout > 0) {
        Env::FundsUnlock(assetId, totalPayout);
    }
}

// ============================================================================
// Method 4: Deposit (Owner only - add funds to pool)
// ============================================================================
BEAM_EXPORT void Method_4(const BeamBet::Method::Deposit& r)
{
    BeamBet::State& s = BeamBet::GetState();
    if (r.m_Amount == 0) Env::Halt();

    Env::FundsLock(s.m_AssetId, r.m_Amount);
    s.m_TotalDeposited += r.m_Amount;
    BeamBet::SaveState();

    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 5: Withdraw (Owner only - remove funds from pool)
// ============================================================================
BEAM_EXPORT void Method_5(const BeamBet::Method::Withdraw& r)
{
    BeamBet::State& s = BeamBet::GetState();
    if (r.m_Amount == 0) Env::Halt();

    uint64_t avail = BeamBet::GetAvailableBalance();
    if (r.m_Amount > avail) Env::Halt();

    Env::FundsUnlock(s.m_AssetId, r.m_Amount);

    // Adjust deposit tracking (profits may exceed original deposit)
    if (r.m_Amount <= s.m_TotalDeposited)
        s.m_TotalDeposited -= r.m_Amount;
    else
        s.m_TotalDeposited = 0;

    BeamBet::SaveState();

    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 6: Set Owner (Owner only)
// ============================================================================
BEAM_EXPORT void Method_6(const BeamBet::Method::SetOwner& r)
{
    BeamBet::State& s = BeamBet::GetState();

    PubKey currentOwner = s.m_OwnerPk;
    _POD_(s.m_OwnerPk) = r.m_OwnerPk;
    BeamBet::SaveState();

    Env::AddSig(currentOwner);
}

// ============================================================================
// Method 7: Set Config (Owner only)
// ============================================================================
BEAM_EXPORT void Method_7(const BeamBet::Method::SetConfig& r)
{
    BeamBet::State& s = BeamBet::GetState();

    if (r.m_MinBet > 0) s.m_MinBet = r.m_MinBet;
    if (r.m_MaxBet > 0) s.m_MaxBet = r.m_MaxBet;
    if (r.m_UpDownMult > 0) s.m_UpDownMult = r.m_UpDownMult;
    if (r.m_ExactMult > 0) s.m_ExactMult = r.m_ExactMult;
    s.m_Paused = r.m_Paused;

    // Overflow protection: maxBet * multiplier must not overflow uint64_t
    // Check BOTH multipliers independently (don't assume one is always larger)
    if (s.m_MaxBet > 0) {
        if (s.m_ExactMult > 0 && s.m_MaxBet > static_cast<uint64_t>(-1) / s.m_ExactMult)
            Env::Halt();
        if (s.m_UpDownMult > 0 && s.m_MaxBet > static_cast<uint64_t>(-1) / s.m_UpDownMult)
            Env::Halt();
    }

    // Prevent changing asset while ANY user obligations exist
    // (pending bets OR won-but-unclaimed payouts — both hold locked funds)
    if (r.m_AssetId != s.m_AssetId && (s.m_PendingBets > 0 || s.m_PendingPayouts > 0))
        Env::Halt();
    s.m_AssetId = r.m_AssetId;

    BeamBet::SaveState();

    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 8: Owner emergency reveal (deterministic - no owner-chosen result)
// Owner triggers this for stuck pending bets. Result is identical to what
// the user would get via check_result. Owner CANNOT influence the outcome.
// ============================================================================
BEAM_EXPORT void Method_8(const BeamBet::Method::RevealBet& r)
{
    BeamBet::State& s = BeamBet::GetState();

    BeamBet::BetKey bk;
    bk.m_BetId = r.m_BetId;

    BeamBet::Bet b;
    if (!Env::LoadVar_T(bk, b)) Env::Halt();
    if (b.m_Status != BeamBet::BetStatus::Pending) Env::Halt();

    // Must wait for reveal epoch
    Height hCurrent = Env::get_Height();
    if (hCurrent < b.m_CreatedHeight + s.m_RevealEpoch) Env::Halt();

    // Deterministic result - same formula as user reveal (owner cannot manipulate)
    Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
    uint8_t result = BeamBet::CalculateRandomResult(b.m_Commitment, b.m_PlacementHash, revealHeight, b.m_BetId);

    BeamBet::ProcessBetResult(b, result, s);
    // Note: does NOT claim - user must call check_result to receive winnings

    Env::SaveVar_T(bk, b);
    BeamBet::SaveState();

    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 9: Check Single Result (User calls to reveal AND claim one bet by ID)
// ============================================================================
BEAM_EXPORT void Method_9(const BeamBet::Method::CheckSingleResult& r)
{
    BeamBet::State& s = BeamBet::GetState();
    Height hCurrent = Env::get_Height();

    BeamBet::BetKey bk;
    bk.m_BetId = r.m_BetId;

    BeamBet::Bet b;
    if (!Env::LoadVar_T(bk, b)) Env::Halt();

    // Only the bet owner can claim
    if (_POD_(b.m_UserPk) != r.m_UserPk) Env::Halt();

    AssetID assetId = b.m_AssetId;

    if (b.m_Status == BeamBet::BetStatus::Pending)
    {
        // Must wait for reveal epoch
        if (hCurrent < b.m_CreatedHeight + s.m_RevealEpoch) Env::Halt();

        Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
        uint8_t result = BeamBet::CalculateRandomResult(b.m_Commitment, b.m_PlacementHash, revealHeight, b.m_BetId);
        BeamBet::ProcessBetResult(b, result, s);
    }
    else if (b.m_Status == BeamBet::BetStatus::Won)
    {
        // Already revealed by emergency reveal - just claim
    }
    else
    {
        // Lost or already Claimed
        Env::Halt();
    }

    // Claim winnings if won
    uint64_t totalPayout = 0;
    BeamBet::AccumulateClaim(b, s, totalPayout);

    Env::SaveVar_T(bk, b);
    BeamBet::AdvanceFirstUnresolved(s, 10);  // Lower cap for single-bet method
    BeamBet::SaveState();

    // User must prove ownership of the claiming key (prevents bet theft)
    Env::AddSig(r.m_UserPk);

    if (totalPayout > 0) {
        Env::FundsUnlock(assetId, totalPayout);
    }
}

// ============================================================================
// Method 10: Resolve Expired Bets (Anyone can call - reveals results only, no payouts)
// Useful for freeing pool capacity from expired pending bets without requiring
// the user to transact. Won bets remain as Won until user calls check_result.
// ============================================================================
BEAM_EXPORT void Method_10(const BeamBet::Method::ResolveExpiredBets& r)
{
    BeamBet::State& s = BeamBet::GetState();
    Height hCurrent = Env::get_Height();

    // Cap the resolve count for gas safety
    uint32_t maxCount = r.m_MaxCount;
    if (maxCount == 0 || maxCount > 200) maxCount = 50;

    uint32_t resolved = 0;

    // Start from FirstUnresolvedBetId — skip bets already Lost/Claimed
    for (uint64_t betId = s.m_FirstUnresolvedBetId; betId < s.m_NextBetId && resolved < maxCount; betId++) {
        BeamBet::BetKey bk;
        bk.m_BetId = betId;

        BeamBet::Bet b;
        if (!Env::LoadVar_T(bk, b)) continue;
        if (b.m_Status != BeamBet::BetStatus::Pending) continue;
        if (hCurrent < b.m_CreatedHeight + s.m_RevealEpoch) continue;

        Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
        uint8_t result = BeamBet::CalculateRandomResult(b.m_Commitment, b.m_PlacementHash, revealHeight, b.m_BetId);
        BeamBet::ProcessBetResult(b, result, s);

        Env::SaveVar_T(bk, b);
        resolved++;
    }

    BeamBet::AdvanceFirstUnresolved(s);
    BeamBet::SaveState();
    // No AddSig required - anyone can resolve expired bets (result is deterministic)
    // No FundsUnlock - only reveals result, user must claim winnings separately
}
