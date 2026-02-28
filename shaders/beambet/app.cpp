// BeamBet App Shader
#include "common.h"
#include "app_common_impl.h"
#include "contract.h"

void OnError(const char* sz)
{
    Env::DocGroup root("");
    Env::DocAddText("error", sz);
}

// Owner key derivation
struct OwnerKey {
    ShaderID m_SID;
    uint8_t m_pSeed[16];  // Actual seed: "beambet-owner-ke" (first 16 bytes of "beambet-owner-key")

    OwnerKey() {
        _POD_(m_SID) = BeamBet::s_SID;
        const char szSeed[] = "beambet-owner-key";
        Env::Memcpy(m_pSeed, szSeed, sizeof(m_pSeed));
    }

    void DerivePk(PubKey& pk) const {
        Env::DerivePk(pk, this, sizeof(*this));
    }
};

// User key derivation for betting
struct UserKey {
    ContractID m_Cid;
    uint8_t m_Tag;  // User betting key tag (MUST be 0)

    void DerivePk(PubKey& pk) const {
        Env::DerivePk(pk, this, sizeof(*this));
    }
};

// Compact history entry for circular buffer (used by view_all and result_history)
struct HistSlot {
    uint64_t betId, amount, payout, createdHeight, revealedHeight;
    uint32_t type, exactNumber, result, status;
};
static const uint32_t MAX_HISTORY = 50;

// ============================================================================
// Schema: Method_0
// ============================================================================
BEAM_EXPORT void Method_0()
{
    Env::DocGroup root("");
    {
        Env::DocGroup gr("roles");
        {
            Env::DocGroup grRole("manager");
            {
                Env::DocGroup grMethod("create_contract");
            }
            {
                Env::DocGroup grMethod("view_contracts");
            }
            {
                Env::DocGroup grMethod("view_pool");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grMethod("view_all_bets");
                Env::DocAddText("cid", "ContractID");
            }
            {
                // Emergency reveal: deterministic result, no random_value needed
                Env::DocGroup grMethod("reveal_bet");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("bet_id", "uint64");
            }
            {
                // Resolve expired pending bets (reveals results, no payouts)
                Env::DocGroup grMethod("resolve_bets");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("count", "uint32");
            }
            {
                Env::DocGroup grMethod("deposit");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("amount", "uint64");
            }
            {
                Env::DocGroup grMethod("withdraw");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("amount", "uint64");
            }
            {
                Env::DocGroup grMethod("set_owner");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("owner_pk", "PubKey");
            }
            {
                Env::DocGroup grMethod("set_config");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("min_bet", "uint64");
                Env::DocAddText("max_bet", "uint64");
                Env::DocAddText("up_down_mult", "uint64");
                Env::DocAddText("exact_mult", "uint64");
                Env::DocAddText("paused", "uint32");
                Env::DocAddText("asset_id", "AssetID");
            }
        }
        {
            Env::DocGroup grRole("user");
            {
                Env::DocGroup grMethod("view_params");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grMethod("place_bet");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("amount", "uint64");
                Env::DocAddText("asset_id", "AssetID");
                Env::DocAddText("bet_type", "uint32");
                Env::DocAddText("exact_number", "uint32");
                Env::DocAddText("commitment", "HashValue");
            }
            {
                Env::DocGroup grMethod("check_results");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grMethod("check_result");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("bet_id", "uint64");
            }
            {
                Env::DocGroup grMethod("view_user_pk");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grMethod("my_bets");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grMethod("result_history");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grMethod("view_all");
                Env::DocAddText("cid", "ContractID");
            }
        }
    }
}

// ============================================================================
// Action Handlers
// ============================================================================

void On_create_contract(const ContractID& cid)
{
    (void)cid;
    BeamBet::Params params;
    OwnerKey ok;
    ok.DerivePk(params.m_OwnerPk);

    Env::GenerateKernel(nullptr, 0, &params, sizeof(params), nullptr, 0, nullptr, 0, "create BeamBet contract", 0);
}

void On_view_contracts(const ContractID& cid)
{
    (void)cid;
    Env::DocArray gr("contracts");

    WalkerContracts wlk;
    for (wlk.Enum(BeamBet::s_SID); wlk.MoveNext(); )
    {
        Env::DocGroup root("");
        Env::DocAddBlob_T("cid", wlk.m_Key.m_KeyInContract.m_Cid);
        Env::DocAddNum("height", wlk.m_Height);
    }
}

void On_view_pool(const ContractID& cid)
{
    Env::Key_T<BeamBet::StateKey> k;
    k.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(k, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    // Mirror GetAvailableBalance() from contract
    uint64_t total = s.m_TotalDeposited + s.m_TotalBets;
    if (s.m_TotalPayouts > total) total = 0;
    else total -= s.m_TotalPayouts;
    uint64_t reserved = s.m_PendingMaxPayout + s.m_PendingPayouts;
    uint64_t available = (reserved <= total) ? (total - reserved) : 0;

    Env::DocGroup gr("pool");
    Env::DocAddNum("total_deposited", s.m_TotalDeposited);
    Env::DocAddNum("total_bets", s.m_TotalBets);
    Env::DocAddNum("total_payouts", s.m_TotalPayouts);
    Env::DocAddNum("pending_bets", s.m_PendingBets);
    Env::DocAddNum("pending_max_payout", s.m_PendingMaxPayout);
    Env::DocAddNum("pending_payouts", s.m_PendingPayouts);
    Env::DocAddNum("available_balance", available);
    Env::DocAddNum("asset_id", s.m_AssetId);
    Env::DocAddNum("paused", (uint32_t)s.m_Paused);
}

void On_view_params(const ContractID& cid)
{
    Env::Key_T<BeamBet::StateKey> k;
    k.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(k, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    Env::DocGroup gr("params");
    Env::DocAddNum("min_bet", s.m_MinBet);
    Env::DocAddNum("max_bet", s.m_MaxBet);
    Env::DocAddNum("up_down_mult", s.m_UpDownMult);
    Env::DocAddNum("exact_mult", s.m_ExactMult);
    Env::DocAddNum("reveal_epoch", s.m_RevealEpoch);
    Env::DocAddNum("paused", (uint32_t)s.m_Paused);
    Env::DocAddNum("asset_id", s.m_AssetId);
}

void On_place_bet(const ContractID& cid)
{
    BeamBet::Method::PlaceBet args;
    uint32_t tempType = 0, tempExact = 0;

    UserKey uk;
    _POD_(uk).SetZero();
    uk.m_Cid = cid;
    uk.DerivePk(args.m_UserPk);

    Env::DocGet("amount", args.m_Amount);
    Env::DocGet("asset_id", args.m_AssetId);
    Env::DocGet("bet_type", tempType);
    Env::DocGet("exact_number", tempExact);
    Env::DocGet("commitment", args.m_Commitment);

    args.m_Type = (uint8_t)tempType;
    args.m_ExactNumber = (uint8_t)tempExact;

    FundsChange fc;
    fc.m_Aid = args.m_AssetId;
    fc.m_Amount = args.m_Amount;
    fc.m_Consume = 1;

    // nCharge covers: PlaceBet base (~130K) + auto-resolve up to 5 expired bets (~250K) + AdvanceFirstUnresolved (~20K)
    Env::GenerateKernel(&cid, BeamBet::Method::PlaceBet::s_iMethod, &args, sizeof(args), &fc, 1, nullptr, 0, "BeamBet: place bet", 400000);
}

void On_check_results(const ContractID& cid)
{
    BeamBet::Method::CheckResults args;

    UserKey uk;
    _POD_(uk).SetZero();
    uk.m_Cid = cid;
    uk.DerivePk(args.m_UserPk);

    Env::Key_T<BeamBet::StateKey> sk;
    sk.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(sk, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    Height hCurrent = Env::get_Height();
    uint64_t totalPayout = 0;
    uint32_t processedCount = 0;

    // Batch range scan — one Vars_Enum instead of N individual reads
    if (s.m_FirstUnresolvedBetId < s.m_NextBetId)
    {
        Env::Key_T<BeamBet::BetKey> k0, k1;
        k0.m_Prefix.m_Cid = cid;
        k0.m_KeyInContract.m_BetId = s.m_FirstUnresolvedBetId;
        k1.m_Prefix.m_Cid = cid;
        k1.m_KeyInContract.m_BetId = s.m_NextBetId - 1;

        Env::VarReader scanner(k0, k1);
        Env::Key_T<BeamBet::BetKey> key;
        BeamBet::Bet b;
        while (scanner.MoveNext_T(key, b) && processedCount < 100)
        {
            if (_POD_(b.m_UserPk) != args.m_UserPk) continue;

            if (b.m_Status == BeamBet::BetStatus::Pending)
            {
                if (hCurrent < b.m_CreatedHeight + s.m_RevealEpoch) continue;

                Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
                HashProcessor::Sha256 hp;
                hp.Write(b.m_Commitment);
                hp.Write(b.m_PlacementHash);
                hp.Write(&revealHeight, sizeof(revealHeight));
                hp.Write(&b.m_BetId, sizeof(b.m_BetId));
                HashValue resultHash;
                hp >> resultHash;
                uint16_t rawValue = ((uint16_t)resultHash.m_p[0] << 8) | resultHash.m_p[1];
                uint8_t result = (rawValue % 100) + 1;

                bool won = false;
                switch (b.m_Type) {
                    case 0: won = (result > 50);               break;
                    case 1: won = (result < 51);               break;
                    case 2: won = (result == b.m_ExactNumber); break;
                }

                if (won) {
                    totalPayout += (b.m_Amount * b.m_Multiplier) / 100;
                }
            }
            else if (b.m_Status == BeamBet::BetStatus::Won && b.m_Payout > 0)
            {
                totalPayout += b.m_Payout;
            }
            else
            {
                continue;
            }

            processedCount++;
        }
    }

    // User must sign to prove ownership (matches AddSig in contract)
    Env::KeyID kid(&uk, sizeof(uk));

    // Dynamic charge: base overhead + per-bet cost (LoadVar + SHA256 + SaveVar)
    // Base: state load/save + AddSig + FundsUnlock + AdvanceFirstUnresolved(50) ≈ 700K
    // Per bet: LoadVar(Bet) + SHA256 + SaveVar(Bet) ≈ 50K
    uint32_t nCharge = 700000 + processedCount * 50000;
    if (nCharge < 300000) nCharge = 300000;   // Floor for minimal overhead

    if (totalPayout > 0)
    {
        FundsChange fc;
        fc.m_Aid = s.m_AssetId;
        fc.m_Amount = totalPayout;
        fc.m_Consume = 0;
        Env::GenerateKernel(&cid, BeamBet::Method::CheckResults::s_iMethod, &args, sizeof(args), &fc, 1, &kid, 1, "BeamBet: check results", nCharge);
    }
    else
    {
        Env::GenerateKernel(&cid, BeamBet::Method::CheckResults::s_iMethod, &args, sizeof(args), nullptr, 0, &kid, 1, "BeamBet: check results", nCharge);
    }
}

void On_view_user_pk(const ContractID& cid)
{
    UserKey uk;
    _POD_(uk).SetZero();
    uk.m_Cid = cid;
    PubKey userPk;
    uk.DerivePk(userPk);
    Env::DocAddBlob_T("user_pk", userPk);
}

void On_my_bets(const ContractID& cid)
{
    UserKey uk;
    _POD_(uk).SetZero();
    uk.m_Cid = cid;
    PubKey userPk;
    uk.DerivePk(userPk);

    Env::Key_T<BeamBet::StateKey> sk;
    sk.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(sk, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    Height currentHeight = Env::get_Height();

    Env::DocArray gr("bets");

    // Batch range scan — one Vars_Enum instead of N individual reads
    if (s.m_FirstUnresolvedBetId < s.m_NextBetId)
    {
        Env::Key_T<BeamBet::BetKey> k0, k1;
        k0.m_Prefix.m_Cid = cid;
        k0.m_KeyInContract.m_BetId = s.m_FirstUnresolvedBetId;
        k1.m_Prefix.m_Cid = cid;
        k1.m_KeyInContract.m_BetId = s.m_NextBetId - 1;

        Env::VarReader scanner(k0, k1);
        Env::Key_T<BeamBet::BetKey> key;
        BeamBet::Bet b;
        while (scanner.MoveNext_T(key, b))
        {
            if (_POD_(b.m_UserPk) != userPk) continue;
            if (b.m_Status != BeamBet::BetStatus::Pending) continue;

            uint64_t blocksRemaining = 0;
            if (b.m_CreatedHeight + s.m_RevealEpoch > currentHeight)
                blocksRemaining = (b.m_CreatedHeight + s.m_RevealEpoch) - currentHeight;

            Env::DocGroup betGr("");
            Env::DocAddNum("bet_id", b.m_BetId);
            Env::DocAddNum("amount", b.m_Amount);
            Env::DocAddNum("type", (uint32_t)b.m_Type);
            Env::DocAddNum("exact_number", (uint32_t)b.m_ExactNumber);
            Env::DocAddNum("status", (uint32_t)b.m_Status);
            Env::DocAddNum("created_height", b.m_CreatedHeight);
            Env::DocAddNum("blocks_remaining", blocksRemaining);
            Env::DocAddNum("can_reveal", (uint32_t)(blocksRemaining == 0 ? 1 : 0));

            // Preview result for ready bets (deterministic — no TX needed to know outcome)
            if (blocksRemaining == 0)
            {
                Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
                HashProcessor::Sha256 hp;
                hp.Write(b.m_Commitment);
                hp.Write(b.m_PlacementHash);
                hp.Write(&revealHeight, sizeof(revealHeight));
                hp.Write(&b.m_BetId, sizeof(b.m_BetId));
                HashValue resultHash;
                hp >> resultHash;
                uint16_t rawValue = ((uint16_t)resultHash.m_p[0] << 8) | resultHash.m_p[1];
                uint8_t result = (rawValue % 100) + 1;

                bool won = false;
                switch (b.m_Type) {
                    case 0: won = (result > 50); break;
                    case 1: won = (result < 51); break;
                    case 2: won = (result == b.m_ExactNumber); break;
                }

                Env::DocAddNum("preview_result", (uint32_t)result);
                Env::DocAddNum("preview_won", (uint32_t)(won ? 1 : 0));
                if (won) {
                    Env::DocAddNum("preview_payout", (b.m_Amount * b.m_Multiplier) / 100);
                }
            }
        }
    }
}

void On_result_history(const ContractID& cid)
{
    UserKey uk;
    _POD_(uk).SetZero();
    uk.m_Cid = cid;
    PubKey userPk;
    uk.DerivePk(userPk);

    Env::Key_T<BeamBet::StateKey> sk;
    sk.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(sk, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    // Batch range scan with circular buffer for last 50 entries (newest first)
    HistSlot* histBuf = (HistSlot*) Env::Heap_Alloc(sizeof(HistSlot) * MAX_HISTORY);
    uint32_t histCount = 0, histWrite = 0;

    if (s.m_NextBetId > 0)
    {
        Env::Key_T<BeamBet::BetKey> k0, k1;
        k0.m_Prefix.m_Cid = cid;
        k0.m_KeyInContract.m_BetId = 0;
        k1.m_Prefix.m_Cid = cid;
        k1.m_KeyInContract.m_BetId = s.m_NextBetId - 1;

        Env::VarReader scanner(k0, k1);
        Env::Key_T<BeamBet::BetKey> key;
        BeamBet::Bet b;
        while (scanner.MoveNext_T(key, b))
        {
            if (_POD_(b.m_UserPk) != userPk) continue;
            if (b.m_Status == BeamBet::BetStatus::Pending) continue;

            HistSlot& h = histBuf[histWrite];
            h.betId = b.m_BetId;
            h.amount = b.m_Amount;
            h.type = (uint32_t)b.m_Type;
            h.exactNumber = (uint32_t)b.m_ExactNumber;
            h.result = (uint32_t)b.m_Result;
            h.status = (uint32_t)b.m_Status;
            h.payout = b.m_Payout;
            h.createdHeight = b.m_CreatedHeight;
            h.revealedHeight = b.m_RevealedHeight;
            histWrite = (histWrite + 1) % MAX_HISTORY;
            if (histCount < MAX_HISTORY) histCount++;
        }
    }

    // Output newest first
    Env::DocArray gr("history");
    for (uint32_t i = 0; i < histCount; i++)
    {
        uint32_t idx = (histWrite + MAX_HISTORY - 1 - i) % MAX_HISTORY;
        HistSlot& h = histBuf[idx];

        Env::DocGroup betGr("");
        Env::DocAddNum("bet_id", h.betId);
        Env::DocAddNum("amount", h.amount);
        Env::DocAddNum("type", h.type);
        Env::DocAddNum("exact_number", h.exactNumber);
        Env::DocAddNum("result", h.result);
        Env::DocAddNum("status", h.status);
        Env::DocAddNum("payout", h.payout);
        Env::DocAddNum("created_height", h.createdHeight);
        Env::DocAddNum("revealed_height", h.revealedHeight);

        const char* statusText = "unknown";
        if (h.status == BeamBet::BetStatus::Won) statusText = "won";
        else if (h.status == BeamBet::BetStatus::Lost) statusText = "lost";
        else if (h.status == BeamBet::BetStatus::Claimed) statusText = "claimed";
        Env::DocAddText("status_text", statusText);
    }

    Env::Heap_Free(histBuf);
}

// Combined view: params + pool + bets + history in ONE call (one state read, one bet scan)
void On_view_all(const ContractID& cid)
{
    Env::Key_T<BeamBet::StateKey> sk;
    sk.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(sk, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    // === PARAMS ===
    {
        Env::DocGroup gr("params");
        Env::DocAddNum("min_bet", s.m_MinBet);
        Env::DocAddNum("max_bet", s.m_MaxBet);
        Env::DocAddNum("up_down_mult", s.m_UpDownMult);
        Env::DocAddNum("exact_mult", s.m_ExactMult);
        Env::DocAddNum("reveal_epoch", s.m_RevealEpoch);
        Env::DocAddNum("paused", (uint32_t)s.m_Paused);
        Env::DocAddNum("asset_id", s.m_AssetId);
    }

    // === POOL ===
    {
        uint64_t total = s.m_TotalDeposited + s.m_TotalBets;
        if (s.m_TotalPayouts > total) total = 0;
        else total -= s.m_TotalPayouts;
        uint64_t reserved = s.m_PendingMaxPayout + s.m_PendingPayouts;
        uint64_t available = (reserved <= total) ? (total - reserved) : 0;

        Env::DocGroup gr("pool");
        Env::DocAddNum("total_deposited", s.m_TotalDeposited);
        Env::DocAddNum("total_bets", s.m_TotalBets);
        Env::DocAddNum("total_payouts", s.m_TotalPayouts);
        Env::DocAddNum("pending_bets", s.m_PendingBets);
        Env::DocAddNum("pending_max_payout", s.m_PendingMaxPayout);
        Env::DocAddNum("pending_payouts", s.m_PendingPayouts);
        Env::DocAddNum("available_balance", available);
        Env::DocAddNum("asset_id", s.m_AssetId);
        Env::DocAddNum("paused", (uint32_t)s.m_Paused);
    }

    // Derive user PK once
    UserKey uk;
    _POD_(uk).SetZero();
    uk.m_Cid = cid;
    PubKey userPk;
    uk.DerivePk(userPk);

    Height currentHeight = Env::get_Height();

    // === SINGLE RANGE SCAN for pending bets, unclaimed wins, and history ===
    // One Vars_Enum call instead of N individual Read_T calls (O(1) vs O(N) node round-trips)
    HistSlot* histBuf = (HistSlot*) Env::Heap_Alloc(sizeof(HistSlot) * MAX_HISTORY);
    uint32_t histCount = 0, histWrite = 0;

    // Separate buffer for unclaimed wins — NOT in circular buffer so they never get lost
    static const uint32_t MAX_UNCLAIMED = 100;
    HistSlot* unclaimedBuf = (HistSlot*) Env::Heap_Alloc(sizeof(HistSlot) * MAX_UNCLAIMED);
    uint32_t unclaimedCount = 0;

    {
        Env::DocArray gr("bets");
        if (s.m_NextBetId > 0)
        {
            Env::Key_T<BeamBet::BetKey> k0, k1;
            k0.m_Prefix.m_Cid = cid;
            k0.m_KeyInContract.m_BetId = 0;
            k1.m_Prefix.m_Cid = cid;
            k1.m_KeyInContract.m_BetId = s.m_NextBetId - 1;

            Env::VarReader scanner(k0, k1);
            Env::Key_T<BeamBet::BetKey> key;
            BeamBet::Bet b;
            while (scanner.MoveNext_T(key, b))
            {
                if (_POD_(b.m_UserPk) != userPk) continue;

                if (b.m_Status == BeamBet::BetStatus::Pending)
                {
                    // Output pending bet directly to "bets" array
                    uint64_t blocksRemaining = 0;
                    if (b.m_CreatedHeight + s.m_RevealEpoch > currentHeight)
                        blocksRemaining = (b.m_CreatedHeight + s.m_RevealEpoch) - currentHeight;

                    Env::DocGroup betGr("");
                    Env::DocAddNum("bet_id", b.m_BetId);
                    Env::DocAddNum("amount", b.m_Amount);
                    Env::DocAddNum("type", (uint32_t)b.m_Type);
                    Env::DocAddNum("exact_number", (uint32_t)b.m_ExactNumber);
                    Env::DocAddNum("status", (uint32_t)b.m_Status);
                    Env::DocAddNum("created_height", b.m_CreatedHeight);
                    Env::DocAddNum("blocks_remaining", blocksRemaining);
                    Env::DocAddNum("can_reveal", (uint32_t)(blocksRemaining == 0 ? 1 : 0));

                    // Preview result for ready bets (deterministic — no TX needed to know outcome)
                    if (blocksRemaining == 0)
                    {
                        Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
                        HashProcessor::Sha256 hp;
                        hp.Write(b.m_Commitment);
                        hp.Write(b.m_PlacementHash);
                        hp.Write(&revealHeight, sizeof(revealHeight));
                        hp.Write(&b.m_BetId, sizeof(b.m_BetId));
                        HashValue resultHash;
                        hp >> resultHash;
                        uint16_t rawValue = ((uint16_t)resultHash.m_p[0] << 8) | resultHash.m_p[1];
                        uint8_t result = (rawValue % 100) + 1;

                        bool won = false;
                        switch (b.m_Type) {
                            case 0: won = (result > 50); break;
                            case 1: won = (result < 51); break;
                            case 2: won = (result == b.m_ExactNumber); break;
                        }

                        Env::DocAddNum("preview_result", (uint32_t)result);
                        Env::DocAddNum("preview_won", (uint32_t)(won ? 1 : 0));
                        if (won) {
                            Env::DocAddNum("preview_payout", (b.m_Amount * b.m_Multiplier) / 100);
                        }
                    }
                }
                else if (b.m_Status == BeamBet::BetStatus::Won)
                {
                    // Unclaimed win — buffer separately (never lost to circular overflow)
                    if (unclaimedCount < MAX_UNCLAIMED)
                    {
                        HistSlot& u = unclaimedBuf[unclaimedCount++];
                        u.betId = b.m_BetId;
                        u.amount = b.m_Amount;
                        u.type = (uint32_t)b.m_Type;
                        u.exactNumber = (uint32_t)b.m_ExactNumber;
                        u.result = (uint32_t)b.m_Result;
                        u.status = (uint32_t)b.m_Status;
                        u.payout = b.m_Payout;
                        u.createdHeight = b.m_CreatedHeight;
                        u.revealedHeight = b.m_RevealedHeight;
                    }
                }
                else
                {
                    // Lost/Claimed — circular buffer for history (keeps last 50)
                    HistSlot& h = histBuf[histWrite];
                    h.betId = b.m_BetId;
                    h.amount = b.m_Amount;
                    h.type = (uint32_t)b.m_Type;
                    h.exactNumber = (uint32_t)b.m_ExactNumber;
                    h.result = (uint32_t)b.m_Result;
                    h.status = (uint32_t)b.m_Status;
                    h.payout = b.m_Payout;
                    h.createdHeight = b.m_CreatedHeight;
                    h.revealedHeight = b.m_RevealedHeight;
                    histWrite = (histWrite + 1) % MAX_HISTORY;
                    if (histCount < MAX_HISTORY) histCount++;
                }
            }
        }
    }

    // === UNCLAIMED WINS — always output all of them ===
    {
        Env::DocArray gr("unclaimed");
        for (uint32_t i = 0; i < unclaimedCount; i++)
        {
            HistSlot& u = unclaimedBuf[i];
            Env::DocGroup betGr("");
            Env::DocAddNum("bet_id", u.betId);
            Env::DocAddNum("amount", u.amount);
            Env::DocAddNum("type", u.type);
            Env::DocAddNum("exact_number", u.exactNumber);
            Env::DocAddNum("result", u.result);
            Env::DocAddNum("status", u.status);
            Env::DocAddNum("payout", u.payout);
            Env::DocAddNum("created_height", u.createdHeight);
            Env::DocAddNum("revealed_height", u.revealedHeight);
            Env::DocAddText("status_text", "won");
        }
    }

    // === RESULT HISTORY — output from circular buffer, newest first ===
    {
        Env::DocArray gr("history");
        for (uint32_t i = 0; i < histCount; i++)
        {
            uint32_t idx = (histWrite + MAX_HISTORY - 1 - i) % MAX_HISTORY;
            HistSlot& h = histBuf[idx];

            Env::DocGroup betGr("");
            Env::DocAddNum("bet_id", h.betId);
            Env::DocAddNum("amount", h.amount);
            Env::DocAddNum("type", h.type);
            Env::DocAddNum("exact_number", h.exactNumber);
            Env::DocAddNum("result", h.result);
            Env::DocAddNum("status", h.status);
            Env::DocAddNum("payout", h.payout);
            Env::DocAddNum("created_height", h.createdHeight);
            Env::DocAddNum("revealed_height", h.revealedHeight);

            const char* statusText = "unknown";
            if (h.status == BeamBet::BetStatus::Lost) statusText = "lost";
            else if (h.status == BeamBet::BetStatus::Claimed) statusText = "claimed";
            Env::DocAddText("status_text", statusText);
        }
    }

    Env::Heap_Free(unclaimedBuf);
    Env::Heap_Free(histBuf);
}

void On_check_result(const ContractID& cid)
{
    BeamBet::Method::CheckSingleResult args;

    Env::DocGet("bet_id", args.m_BetId);

    UserKey uk;
    _POD_(uk).SetZero();
    uk.m_Cid = cid;
    uk.DerivePk(args.m_UserPk);

    Env::Key_T<BeamBet::StateKey> sk;
    sk.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(sk, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    Env::Key_T<BeamBet::BetKey> k;
    k.m_Prefix.m_Cid = cid;
    k.m_KeyInContract.m_BetId = args.m_BetId;

    BeamBet::Bet b;
    if (!Env::VarReader::Read_T(k, b))
    {
        OnError("Bet not found");
        return;
    }

    if (_POD_(b.m_UserPk) != args.m_UserPk)
    {
        OnError("Not your bet");
        return;
    }

    uint64_t payout = 0;
    Height hCurrent = Env::get_Height();

    if (b.m_Status == BeamBet::BetStatus::Pending)
    {
        if (hCurrent < b.m_CreatedHeight + s.m_RevealEpoch)
        {
            OnError("Bet not ready for reveal yet");
            return;
        }

        // Replicate CalculateRandomResult (commitment + placementHash + revealHeight + betId)
        Height revealHeight = b.m_CreatedHeight + s.m_RevealEpoch;
        HashProcessor::Sha256 hp;
        hp.Write(b.m_Commitment);
        hp.Write(b.m_PlacementHash);
        hp.Write(&revealHeight, sizeof(revealHeight));
        hp.Write(&b.m_BetId, sizeof(b.m_BetId));
        HashValue resultHash;
        hp >> resultHash;
        uint16_t rawValue = ((uint16_t)resultHash.m_p[0] << 8) | resultHash.m_p[1];
        uint8_t result = (rawValue % 100) + 1;

        bool won = false;
        switch (b.m_Type) {
            case 0: won = (result > 50);               break;
            case 1: won = (result < 51);               break;
            case 2: won = (result == b.m_ExactNumber); break;
        }

        if (won) {
            payout = (b.m_Amount * b.m_Multiplier) / 100;  // Use stored multiplier
        }
    }
    else if (b.m_Status == BeamBet::BetStatus::Won && b.m_Payout > 0)
    {
        payout = b.m_Payout;
    }
    else
    {
        OnError("Bet is not pending or won");
        return;
    }

    // User must sign to prove ownership (matches AddSig in contract)
    Env::KeyID kid(&uk, sizeof(uk));

    if (payout > 0)
    {
        FundsChange fc;
        fc.m_Aid = b.m_AssetId;  // Use bet's actual asset (matches contract Method_9)
        fc.m_Amount = payout;
        fc.m_Consume = 0;
        Env::GenerateKernel(&cid, BeamBet::Method::CheckSingleResult::s_iMethod, &args, sizeof(args), &fc, 1, &kid, 1, "BeamBet: check single result", 250000);
    }
    else
    {
        Env::GenerateKernel(&cid, BeamBet::Method::CheckSingleResult::s_iMethod, &args, sizeof(args), nullptr, 0, &kid, 1, "BeamBet: check single result", 250000);
    }
}

void On_view_all_bets(const ContractID& cid)
{
    Env::Key_T<BeamBet::StateKey> sk;
    sk.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(sk, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    Height hCurrent = Env::get_Height();

    Env::DocAddNum("total_bets_ever", s.m_NextBetId);

    Env::DocArray gr("bets");

    // Batch range scan — one Vars_Enum instead of N individual reads
    if (s.m_NextBetId > 0)
    {
        Env::Key_T<BeamBet::BetKey> k0, k1;
        k0.m_Prefix.m_Cid = cid;
        k0.m_KeyInContract.m_BetId = 0;
        k1.m_Prefix.m_Cid = cid;
        k1.m_KeyInContract.m_BetId = s.m_NextBetId - 1;

        Env::VarReader scanner(k0, k1);
        Env::Key_T<BeamBet::BetKey> key;
        BeamBet::Bet b;
        while (scanner.MoveNext_T(key, b))
        {
            Env::DocGroup betGr("");
            Env::DocAddNum("bet_id", b.m_BetId);
            Env::DocAddBlob_T("user_pk", b.m_UserPk);
            Env::DocAddNum("amount", b.m_Amount);
            Env::DocAddNum("asset_id", b.m_AssetId);
            Env::DocAddNum("type", (uint32_t)b.m_Type);
            Env::DocAddNum("exact_number", (uint32_t)b.m_ExactNumber);
            Env::DocAddNum("result", (uint32_t)b.m_Result);
            Env::DocAddNum("status", (uint32_t)b.m_Status);
            Env::DocAddNum("payout", b.m_Payout);
            Env::DocAddNum("created_height", b.m_CreatedHeight);
            Env::DocAddNum("revealed_height", b.m_RevealedHeight);

            if (b.m_Status == BeamBet::BetStatus::Pending)
            {
                uint64_t readyAt = b.m_CreatedHeight + s.m_RevealEpoch;
                uint64_t blocksRemaining = (hCurrent < readyAt) ? (readyAt - hCurrent) : 0;
                Env::DocAddNum("blocks_remaining", blocksRemaining);
                Env::DocAddNum("can_reveal", (uint32_t)(blocksRemaining == 0 ? 1 : 0));
            }

            const char* statusText = "pending";
            if (b.m_Status == BeamBet::BetStatus::Won)     statusText = "won";
            else if (b.m_Status == BeamBet::BetStatus::Lost)    statusText = "lost";
            else if (b.m_Status == BeamBet::BetStatus::Claimed) statusText = "claimed";
            Env::DocAddText("status_text", statusText);

            const char* typeText = "exact";
            if (b.m_Type == 0)      typeText = "up";
            else if (b.m_Type == 1) typeText = "down";
            Env::DocAddText("type_text", typeText);
        }
    }
}

void On_reveal_bet(const ContractID& cid)
{
    // Emergency reveal: deterministic result (no random_value, no user_nonce)
    BeamBet::Method::RevealBet args;
    Env::DocGet("bet_id", args.m_BetId);

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, BeamBet::Method::RevealBet::s_iMethod, &args, sizeof(args), nullptr, 0, &kid, 1, "BeamBet: reveal bet", 130000);
}

void On_resolve_bets(const ContractID& cid)
{
    BeamBet::Method::ResolveExpiredBets args;
    uint32_t tempCount = 50;
    Env::DocGet("count", tempCount);
    if (tempCount == 0 || tempCount > 200) tempCount = 50;
    args.m_MaxCount = tempCount;

    // Dynamic charge: base covers state + AdvanceFirstUnresolved(50), per-bet covers LoadVar + SHA256 + SaveVar
    uint32_t nCharge = 750000 + tempCount * 50000;

    // No FundsChange needed - resolve_bets only reveals results, no payouts
    Env::GenerateKernel(&cid, BeamBet::Method::ResolveExpiredBets::s_iMethod, &args, sizeof(args), nullptr, 0, nullptr, 0, "BeamBet: resolve expired bets", nCharge);
}

void On_deposit(const ContractID& cid)
{
    BeamBet::Method::Deposit args;
    Env::DocGet("amount", args.m_Amount);

    Env::Key_T<BeamBet::StateKey> k;
    k.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(k, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    FundsChange fc;
    fc.m_Aid = s.m_AssetId;
    fc.m_Amount = args.m_Amount;
    fc.m_Consume = 1;

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, BeamBet::Method::Deposit::s_iMethod, &args, sizeof(args), &fc, 1, &kid, 1, "BeamBet: deposit", 75000);
}

void On_withdraw(const ContractID& cid)
{
    BeamBet::Method::Withdraw args;
    Env::DocGet("amount", args.m_Amount);

    Env::Key_T<BeamBet::StateKey> k;
    k.m_Prefix.m_Cid = cid;

    BeamBet::State s;
    if (!Env::VarReader::Read_T(k, s))
    {
        OnError("Failed to read contract state");
        return;
    }

    FundsChange fc;
    fc.m_Aid = s.m_AssetId;
    fc.m_Amount = args.m_Amount;
    fc.m_Consume = 0;

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, BeamBet::Method::Withdraw::s_iMethod, &args, sizeof(args), &fc, 1, &kid, 1, "BeamBet: withdraw", 75000);
}

void On_set_owner(const ContractID& cid)
{
    BeamBet::Method::SetOwner args;
    Env::DocGet("owner_pk", args.m_OwnerPk);

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, BeamBet::Method::SetOwner::s_iMethod, &args, sizeof(args), nullptr, 0, &kid, 1, "BeamBet: set owner", 70000);
}

void On_set_config(const ContractID& cid)
{
    BeamBet::Method::SetConfig args;
    uint32_t tempPaused = 0;

    Env::DocGet("min_bet", args.m_MinBet);
    Env::DocGet("max_bet", args.m_MaxBet);
    Env::DocGet("up_down_mult", args.m_UpDownMult);
    Env::DocGet("exact_mult", args.m_ExactMult);
    Env::DocGet("paused", tempPaused);
    Env::DocGet("asset_id", args.m_AssetId);

    args.m_Paused = (uint8_t)tempPaused;

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, BeamBet::Method::SetConfig::s_iMethod, &args, sizeof(args), nullptr, 0, &kid, 1, "BeamBet: set config", 70000);
}

// ============================================================================
// Dispatcher: Method_1
// ============================================================================
BEAM_EXPORT void Method_1()
{
    Env::DocGroup root("");

    char szRole[16], szAction[32];

    if (!Env::DocGetText("role", szRole, sizeof(szRole)))
        return OnError("Role not specified");

    if (!Env::DocGetText("action", szAction, sizeof(szAction)))
        return OnError("Action not specified");

    ContractID cid;
    Env::DocGet("cid", cid);

    if (!Env::Strcmp(szRole, "manager"))
    {
        if (!Env::Strcmp(szAction, "create_contract"))
            return On_create_contract(cid);
        if (!Env::Strcmp(szAction, "view_contracts"))
            return On_view_contracts(cid);
        if (!Env::Strcmp(szAction, "view_pool"))
            return On_view_pool(cid);
        if (!Env::Strcmp(szAction, "view_all_bets"))
            return On_view_all_bets(cid);
        if (!Env::Strcmp(szAction, "reveal_bet"))
            return On_reveal_bet(cid);
        if (!Env::Strcmp(szAction, "resolve_bets"))
            return On_resolve_bets(cid);
        if (!Env::Strcmp(szAction, "deposit"))
            return On_deposit(cid);
        if (!Env::Strcmp(szAction, "withdraw"))
            return On_withdraw(cid);
        if (!Env::Strcmp(szAction, "set_owner"))
            return On_set_owner(cid);
        if (!Env::Strcmp(szAction, "set_config"))
            return On_set_config(cid);

        return OnError("Invalid action");
    }

    if (!Env::Strcmp(szRole, "user"))
    {
        if (!Env::Strcmp(szAction, "view_params"))
            return On_view_params(cid);
        if (!Env::Strcmp(szAction, "view_user_pk"))
            return On_view_user_pk(cid);
        if (!Env::Strcmp(szAction, "place_bet"))
            return On_place_bet(cid);
        if (!Env::Strcmp(szAction, "check_results"))
            return On_check_results(cid);
        if (!Env::Strcmp(szAction, "check_result"))
            return On_check_result(cid);
        if (!Env::Strcmp(szAction, "my_bets"))
            return On_my_bets(cid);
        if (!Env::Strcmp(szAction, "result_history"))
            return On_result_history(cid);
        if (!Env::Strcmp(szAction, "view_all"))
            return On_view_all(cid);

        return OnError("Invalid action");
    }

    OnError("Invalid role");
}
