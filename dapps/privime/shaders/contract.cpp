// PriviMe Contract Shader
// On-chain identity registry: @handle <-> SBBS WalletID.
// Upgradable3 provides Method_2 (upgrade dispatch). Business logic starts at Method_3.
#include "common.h"
#include "upgradable3/contract_impl.h"  // Provides BEAM_EXPORT void Method_2(...)
#include "contract.h"

// ============================================================================
// Upgradable3 callbacks (required by contract_impl.h)
// ============================================================================

// Called by Upgradable3 when a new version of the contract is activated.
// Use to migrate state when upgrading (e.g., adding group methods in Phase 1b).
void Upgradable3::OnUpgraded(uint32_t nPrevVersion)
{
    // Phase 1a (v0): no migration needed
    (void)nPrevVersion;
}

// Returns the current contract version (v0 = Phase 1a).
// Passed as m_PrevVersion to the new contract's OnUpgraded() during upgrade.
uint32_t Upgradable3::get_CurrentVersion()
{
    return 2; // v2 — build variant + m_BuildVariant in State
}

// ============================================================================
// State helpers
// ============================================================================

namespace PriviMe {

static State g_State;

State& GetState()
{
    if (!g_State.m_Initialized) {
        StateKey k;
        if (!Env::LoadVar_T(k, g_State))
            Env::Halt(); // contract not initialized
        g_State.m_Initialized = 1;
    }
    return g_State;
}

void SaveState()
{
    StateKey k;
    Env::SaveVar_T(k, g_State);
}

// Validate @handle: 3-32 chars, lowercase a-z, 0-9, underscore only.
// App shader normalizes to lowercase before calling — this rejects any leftovers.
bool IsValidHandle(const char* h, uint32_t len)
{
    if (len < s_MinHandleLen || len > s_MaxHandleLen)
        return false;
    for (uint32_t i = 0; i < len; i++) {
        char c = h[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
            continue;
        return false;
    }
    return true;
}

// Count null-terminated string length up to maxLen (never reads past maxLen).
uint32_t SafeStrlen(const char* s, uint32_t maxLen)
{
    for (uint32_t i = 0; i < maxLen; i++)
        if (s[i] == 0) return i;
    return maxLen;
}

} // namespace PriviMe

// ============================================================================
// Constructor
// ============================================================================
BEAM_EXPORT void Ctor(const PriviMe::Method::Ctor& r)
{
    // Initialize Upgradable3 settings (admin key, min approvers, upgrade delay)
    r.m_Upgradable.TestNumApprovers();
    r.m_Upgradable.Save();

    // Initialize PriviMe state
    _POD_(PriviMe::g_State).SetZero();
    _POD_(PriviMe::g_State.m_OwnerPk) = r.m_OwnerPk;
    PriviMe::g_State.m_RegistrationFee = PriviMe::s_DefaultFee;
    PriviMe::g_State.m_BuildVariant = PriviMe::s_BuildVariant;
    PriviMe::g_State.m_AssetId = 0; // BEAM
    PriviMe::g_State.m_Initialized = 1;
    PriviMe::SaveState();
}

// ============================================================================
// Destructor — Owner only. Refuses if any handles are still registered.
// ============================================================================
BEAM_EXPORT void Dtor(void*)
{
    PriviMe::State& s = PriviMe::GetState();

    // Reject if users still have active handles — they must release first
    Env::Halt_if(s.m_TotalRegistrations > 0);

    // Return any remaining fee balance to owner
    if (s.m_TotalFees > 0)
        Env::FundsUnlock(s.m_AssetId, s.m_TotalFees);

    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 3: RegisterHandle — Claim a unique @handle (anyone)
// Requires: handle available + caller has no existing handle + fee paid
// ============================================================================
BEAM_EXPORT void Method_3(const PriviMe::Method::RegisterHandle& r)
{
    PriviMe::State& s = PriviMe::GetState();

    Env::Halt_if(s.m_Paused);
    Env::Halt_if(r.m_AssetId != s.m_AssetId);

    // Validate handle format
    uint32_t handleLen = PriviMe::SafeStrlen(r.m_Handle, PriviMe::s_MaxHandleLen);
    Env::Halt_if(!PriviMe::IsValidHandle(r.m_Handle, handleLen));

    // Reject if handle already taken
    PriviMe::HandleKey hk;
    Env::Memcpy(hk.m_Handle, r.m_Handle, handleLen);
    PriviMe::Profile existing;
    Env::Halt_if(Env::LoadVar_T(hk, existing));

    // Reject if caller already has a handle (one per key)
    PriviMe::OwnerKey ok;
    _POD_(ok.m_UserPk) = r.m_UserPk;
    PriviMe::OwnerRecord existingOwner;
    Env::Halt_if(Env::LoadVar_T(ok, existingOwner));

    // Collect registration fee
    Env::FundsLock(s.m_AssetId, s.m_RegistrationFee);

    // Save forward lookup: handle -> profile
    PriviMe::Profile profile;
    _POD_(profile).SetZero();
    _POD_(profile.m_UserPk) = r.m_UserPk;
    Env::Memcpy(profile.m_WalletIdRaw, r.m_WalletIdRaw, sizeof(profile.m_WalletIdRaw));
    profile.m_RegisteredHeight = Env::get_Height();
    uint32_t displayLen = PriviMe::SafeStrlen(r.m_DisplayName, PriviMe::s_MaxDisplayNameLen);
    if (displayLen > 0)
        Env::Memcpy(profile.m_DisplayName, r.m_DisplayName, displayLen);
    Env::SaveVar_T(hk, profile);

    // Save reverse lookup: owner pubkey -> handle
    PriviMe::OwnerRecord ownerRec;
    Env::Memset(ownerRec.m_Handle, 0, sizeof(ownerRec.m_Handle));
    Env::Memcpy(ownerRec.m_Handle, r.m_Handle, handleLen);
    Env::SaveVar_T(ok, ownerRec);

    // Update state
    s.m_TotalRegistrations++;
    s.m_TotalFees += s.m_RegistrationFee;
    PriviMe::SaveState();

    // Caller must sign to prove ownership of this BVM key
    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 4: UpdateProfile — Update WalletID and/or display name (handle owner)
// ============================================================================
BEAM_EXPORT void Method_4(const PriviMe::Method::UpdateProfile& r)
{
    // Find the caller's registered handle via reverse lookup
    PriviMe::OwnerKey ok;
    _POD_(ok.m_UserPk) = r.m_UserPk;
    PriviMe::OwnerRecord ownerRec;
    Env::Halt_if(!Env::LoadVar_T(ok, ownerRec));

    // Load existing profile
    PriviMe::HandleKey hk;
    Env::Memcpy(hk.m_Handle, ownerRec.m_Handle, sizeof(hk.m_Handle));
    PriviMe::Profile profile;
    Env::Halt_if(!Env::LoadVar_T(hk, profile));

    // Update mutable fields
    Env::Memcpy(profile.m_WalletIdRaw, r.m_WalletIdRaw, sizeof(profile.m_WalletIdRaw));
    Env::Memset(profile.m_DisplayName, 0, sizeof(profile.m_DisplayName));
    uint32_t displayLen = PriviMe::SafeStrlen(r.m_DisplayName, PriviMe::s_MaxDisplayNameLen);
    if (displayLen > 0)
        Env::Memcpy(profile.m_DisplayName, r.m_DisplayName, displayLen);
    Env::SaveVar_T(hk, profile);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 5: ReleaseHandle — Give up @handle voluntarily (no refund)
// ============================================================================
BEAM_EXPORT void Method_5(const PriviMe::Method::ReleaseHandle& r)
{
    PriviMe::State& s = PriviMe::GetState();

    // Find the caller's handle via reverse lookup
    PriviMe::OwnerKey ok;
    _POD_(ok.m_UserPk) = r.m_UserPk;
    PriviMe::OwnerRecord ownerRec;
    Env::Halt_if(!Env::LoadVar_T(ok, ownerRec));

    // Delete forward lookup
    PriviMe::HandleKey hk;
    Env::Memcpy(hk.m_Handle, ownerRec.m_Handle, sizeof(hk.m_Handle));
    Env::DelVar_T(hk);

    // Delete reverse lookup
    Env::DelVar_T(ok);

    s.m_TotalRegistrations--;
    PriviMe::SaveState();

    // No refund — registration fee is one-time, non-refundable
    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 6: Deposit — DISABLED (no use case for identity registry)
// Stub kept for method numbering continuity (BVM expects sequential exports).
// ============================================================================
BEAM_EXPORT void Method_6(const PriviMe::Method::DepositDisabled&)
{
    Env::Halt(); // permanently disabled
}

// ============================================================================
// Method 7: Withdraw — Owner withdraws collected fees
// ============================================================================
BEAM_EXPORT void Method_7(const PriviMe::Method::Withdraw& r)
{
    PriviMe::State& s = PriviMe::GetState();
    Env::Halt_if(r.m_Amount == 0);
    Env::Halt_if(r.m_Amount > s.m_TotalFees);

    Env::FundsUnlock(s.m_AssetId, r.m_Amount);
    s.m_TotalFees -= r.m_Amount;
    PriviMe::SaveState();

    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 8: SetOwner — Transfer fee-withdrawal ownership
// ============================================================================
BEAM_EXPORT void Method_8(const PriviMe::Method::SetOwner& r)
{
    PriviMe::State& s = PriviMe::GetState();
    PubKey prevOwner = s.m_OwnerPk;
    _POD_(s.m_OwnerPk) = r.m_OwnerPk;
    PriviMe::SaveState();

    Env::AddSig(prevOwner);
}

// ============================================================================
// Method 9: SetConfig — Update registration fee, pause state, asset ID
// ============================================================================
BEAM_EXPORT void Method_9(const PriviMe::Method::SetConfig& r)
{
    PriviMe::State& s = PriviMe::GetState();

    if (r.m_RegistrationFee > 0)
        s.m_RegistrationFee = r.m_RegistrationFee;
    s.m_Paused = r.m_Paused;
    s.m_AssetId = r.m_AssetId;
    PriviMe::SaveState();

    Env::AddSig(s.m_OwnerPk);
}
