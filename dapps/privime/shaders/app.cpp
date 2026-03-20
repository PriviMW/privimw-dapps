// PriviMe App Shader
// Client-side logic: reads identity registry, generates TX kernels.
// Upgradable3 admin actions (schedule/apply upgrade) use owner key.
#include "common.h"
#include "app_common_impl.h"
#include "upgradable3/app_common_impl.h"
#include "contract.h"

void OnError(const char* sz)
{
    // NOTE: Do NOT open a DocGroup here — Method_1 already has root("") open.
    // Adding one here produces double-wrapped output: {{"error":"..."}}
    Env::DocAddText("error", sz);
}

// ============================================================================
// Key derivation
// ============================================================================

// Owner key: SID-based so consistent across any future contract redeployment.
// Seed "privime-owner-ke" (16 bytes of "privime-owner-key").
// Used for: fee withdrawal (Withdraw/SetOwner/SetConfig).
struct OwnerKey {
    uint8_t  m_pSeed[16]; // "privime-owner-ke"
    ShaderID m_SID;       // First SID ties key to this contract family

    OwnerKey() {
        const char szSeed[] = "privime-owner-key"; // Memcpy copies first 16 bytes
        Env::Memcpy(m_pSeed, szSeed, sizeof(m_pSeed));
        _POD_(m_SID) = PriviMe::s_pSID[0];
    }

    void DerivePk(PubKey& pk) const { Env::DerivePk(pk, this, sizeof(*this)); }
};

// User key: CID-based, unique per contract instance.
// Seed "privime-user-key" (16 bytes of "privime-user-key0").
// Used for: register_handle, update_profile, release_handle.
struct UserKey {
    uint8_t    m_pSeed[16]; // "privime-user-key"
    ContractID m_Cid;

    UserKey() { _POD_(*this).SetZero(); }
    UserKey(const ContractID& cid) {
        const char szSeed[] = "privime-user-key0"; // Memcpy copies first 16 bytes
        Env::Memcpy(m_pSeed, szSeed, sizeof(m_pSeed));
        _POD_(m_Cid) = cid;
    }

    void DerivePk(PubKey& pk) const { Env::DerivePk(pk, this, sizeof(*this)); }
};

// ============================================================================
// Upgradable3 version info
// ============================================================================

static Upgradable3::Manager::VerInfo GetVerInfo()
{
    Upgradable3::Manager::VerInfo vi;
    vi.m_pSid = PriviMe::s_pSID;
    vi.m_Versions = sizeof(PriviMe::s_pSID) / sizeof(PriviMe::s_pSID[0]);
    return vi;
}

// ============================================================================
// Helpers
// ============================================================================

// Output a Profile's WalletID as hex string (68 hex chars = 34 bytes)
void DocAddWalletId(const char* szKey, const uint8_t* raw34)
{
    Env::DocAddBlob(szKey, raw34, 34);
}

// Read contract state — halts on error
bool ReadState(const ContractID& cid, PriviMe::State& s)
{
    Env::Key_T<PriviMe::StateKey> k;
    _POD_(k.m_Prefix.m_Cid) = cid;
    if (!Env::VarReader::Read_T(k, s)) {
        OnError("failed to read contract state");
        return false;
    }
    return true;
}

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
                Env::DocGroup grM("create_contract");
            }
            {
                Env::DocGroup grM("view_contracts");
            }
            {
                Env::DocGroup grM("view_pool");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grM("withdraw");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("amount", "uint64");
            }
            {
                Env::DocGroup grM("set_owner");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("owner_pk", "PubKey");
            }
            {
                Env::DocGroup grM("set_config");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("registration_fee", "uint64");
                Env::DocAddText("paused", "uint32");
                Env::DocAddText("asset_id", "AssetID");
            }
            {
                Env::DocGroup grM("view_all");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grM("explicit_upgrade");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grM("schedule_upgrade");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("hTarget", "Height");
                Env::DocAddText("bSkipVerifyVer", "uint32");
                Env::DocAddText("iSender", "uint32");
                Env::DocAddText("approve_mask", "uint32");
            }
            {
                Env::DocGroup grM("replace_admin");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("iAdmin", "uint32");
                Env::DocAddText("pk", "PubKey");
                Env::DocAddText("iSender", "uint32");
                Env::DocAddText("approve_mask", "uint32");
            }
            {
                Env::DocGroup grM("set_approvers");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("newVal", "uint32");
                Env::DocAddText("iSender", "uint32");
                Env::DocAddText("approve_mask", "uint32");
            }
            {
                Env::DocGroup grM("view_contract_info");
            }
        }
        {
            Env::DocGroup grRole("user");
            {
                Env::DocGroup grM("my_handle");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grM("register_handle");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("handle", "string");
                Env::DocAddText("wallet_id", "hex34");
                Env::DocAddText("display_name", "string");
            }
            {
                Env::DocGroup grM("update_profile");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("wallet_id", "hex34");
                Env::DocAddText("display_name", "string");
            }
            {
                Env::DocGroup grM("release_handle");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grM("set_avatar");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("avatar_hash", "string");
            }
            {
                Env::DocGroup grM("resolve_handle");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("handle", "string");
            }
            {
                Env::DocGroup grM("search_handles");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("prefix", "string");
            }
            {
                Env::DocGroup grM("resolve_walletid");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("walletid", "hex34");
            }
            {
                Env::DocGroup grM("view_recent");
                Env::DocAddText("cid", "ContractID");
            }
        }
    }
}

// ============================================================================
// Manager handlers
// ============================================================================

void On_create_contract(const ContractID& /*cid*/)
{
    PriviMe::Method::Ctor args;
    _POD_(args).SetZero();

    // Single admin: derived from OwnerKey. Upgrade delay = 0 (instant) for DAppNet.
    args.m_Upgradable.m_MinApprovers = 1;
    args.m_Upgradable.m_hMinUpgradeDelay = 0;

    OwnerKey ok;
    ok.DerivePk(args.m_Upgradable.m_pAdmin[0]); // admin key = owner key
    ok.DerivePk(args.m_OwnerPk);                 // fee withdrawal key = same

    // Higher nCharge: Upgradable3::Settings (1036 bytes) + contract.wasm + PriviMe state = large initial deploy
    Env::GenerateKernel(nullptr, 0, &args, sizeof(args), nullptr, 0, nullptr, 0,
                        "create PriviMe contract", 1000000);
}

void On_view_pool(const ContractID& cid)
{
    PriviMe::State s;
    if (!ReadState(cid, s)) return;

    Env::DocGroup gr("pool");
    Env::DocAddNum("registration_fee", s.m_RegistrationFee);
    Env::DocAddNum("total_registrations", s.m_TotalRegistrations);
    Env::DocAddNum("total_fees", s.m_TotalFees);
    Env::DocAddNum("asset_id", s.m_AssetId);
    Env::DocAddNum("paused", (uint32_t)s.m_Paused);
}

void On_withdraw(const ContractID& cid)
{
    PriviMe::Method::Withdraw args;
    Env::DocGet("amount", args.m_Amount);

    PriviMe::State s;
    if (!ReadState(cid, s)) return;

    FundsChange fc;
    fc.m_Aid     = s.m_AssetId;
    fc.m_Amount  = args.m_Amount;
    fc.m_Consume = 0;

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, PriviMe::Method::Withdraw::s_iMethod, &args, sizeof(args),
                        &fc, 1, &kid, 1, "PriviMe: withdraw", 75000);
}

void On_set_owner(const ContractID& cid)
{
    PriviMe::Method::SetOwner args;
    Env::DocGet("owner_pk", args.m_OwnerPk);

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, PriviMe::Method::SetOwner::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: set owner", 70000);
}

void On_set_config(const ContractID& cid)
{
    PriviMe::Method::SetConfig args;
    _POD_(args).SetZero();
    uint32_t tempPaused = 0;
    Env::DocGet("registration_fee", args.m_RegistrationFee);
    Env::DocGet("paused", tempPaused);
    Env::DocGet("asset_id", args.m_AssetId);
    args.m_Paused = (uint8_t)tempPaused;

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Env::GenerateKernel(&cid, PriviMe::Method::SetConfig::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: set config", 70000);
}

void On_view_all(const ContractID& cid)
{
    PriviMe::State s;
    if (!ReadState(cid, s)) return;

    {
        Env::DocGroup gr("state");
        Env::DocAddNum("registration_fee", s.m_RegistrationFee);
        Env::DocAddNum("total_registrations", s.m_TotalRegistrations);
        Env::DocAddNum("total_fees", s.m_TotalFees);
        Env::DocAddNum("asset_id", s.m_AssetId);
        Env::DocAddNum("paused", (uint32_t)s.m_Paused);
    }

    // Range scan all HandleKey entries
    Env::Key_T<PriviMe::HandleKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract).SetZero();
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    _POD_(k1.m_Prefix.m_Cid) = cid;
    Env::Memset(&k1.m_KeyInContract, 0xFF, sizeof(k1.m_KeyInContract));
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    Env::DocArray gr("handles");
    uint32_t count = 0;
    {
        Env::VarReader scanner(k0, k1);
        Env::Key_T<PriviMe::HandleKey> key;
        PriviMe::Profile p;
        while (scanner.MoveNext_T(key, p) && count < 100)
        {
            Env::DocGroup entry("");
            Env::DocAddText("handle", key.m_KeyInContract.m_Handle);
            DocAddWalletId("wallet_id", p.m_WalletIdRaw);
            Env::DocAddNum("registered_height", p.m_RegisteredHeight);
            if (p.m_DisplayName[0])
                Env::DocAddText("display_name", p.m_DisplayName);
            count++;
        }
    }
}

// List all deployed PriviMe contracts (scans ALL known SIDs)
void On_view_contracts(const ContractID& /*cid*/)
{
    Env::DocArray gr("contracts");

    static const uint32_t nVersions = sizeof(PriviMe::s_pSID) / sizeof(PriviMe::s_pSID[0]);
    for (uint32_t v = 0; v < nVersions; v++)
    {
        WalkerContracts wlk;
        for (wlk.Enum(PriviMe::s_pSID[v]); wlk.MoveNext(); )
        {
            Env::DocGroup entry("");
            Env::DocAddBlob_T("cid", wlk.m_Key.m_KeyInContract.m_Cid);
            Env::DocAddNum("height", wlk.m_Height);
            Env::DocAddNum("version", v);
        }
    }
}

// Trigger a pre-scheduled contract upgrade (anyone can call after delay)
void On_explicit_upgrade(const ContractID& cid)
{
    Upgradable3::Manager::MultiSigRitual::Perform_ExplicitUpgrade(cid);
}

// Schedule a contract upgrade (admin only, requires --shader_contract_file)
void On_schedule_upgrade(const ContractID& cid)
{
    Height hTarget = 0;
    Env::DocGet("hTarget", hTarget);
    if (!hTarget)
        return OnError("hTarget required");

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    auto vi = GetVerInfo();
    Upgradable3::Manager::MultiSigRitual::Perform_ScheduleUpgrade(vi, cid, kid, hTarget);
}

// Replace an admin key (admin only)
void On_replace_admin(const ContractID& cid)
{
    uint32_t iAdmin = 0;
    PubKey pk;
    Env::DocGet("iAdmin", iAdmin);
    Env::DocGet("pk", pk);

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Upgradable3::Manager::MultiSigRitual::Perform_ReplaceAdmin(cid, kid, iAdmin, pk);
}

// Change minimum number of admin approvers (admin only)
void On_set_approvers(const ContractID& cid)
{
    uint32_t newVal = 0;
    Env::DocGet("newVal", newVal);
    if (!newVal)
        return OnError("newVal required");

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    Upgradable3::Manager::MultiSigRitual::Perform_SetApprovers(cid, kid, newVal);
}

// Dump contract info: version, admins, scheduled upgrades (view only)
void On_view_contract_info(const ContractID& /*cid*/)
{
    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    auto vi = GetVerInfo();
    vi.DumpAll(&kid);
}

// ============================================================================
// User handlers
// ============================================================================

// Returns { registered: 1, handle, display_name, wallet_id } or { registered: 0 }
void On_my_handle(const ContractID& cid)
{
    UserKey uk(cid);
    PubKey userPk;
    uk.DerivePk(userPk);

    Env::Key_T<PriviMe::OwnerKey> ok;
    _POD_(ok.m_Prefix.m_Cid) = cid;
    ok.m_KeyInContract.m_Tag = PriviMe::Tags::s_Owner;
    _POD_(ok.m_KeyInContract.m_UserPk) = userPk;

    PriviMe::OwnerRecord ownerRec;
    if (!Env::VarReader::Read_T(ok, ownerRec)) {
        Env::DocAddNum("registered", 0u);
        return;
    }

    // Load the profile to get WalletID
    Env::Key_T<PriviMe::HandleKey> hk;
    _POD_(hk.m_Prefix.m_Cid) = cid;
    hk.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;
    Env::Memset(hk.m_KeyInContract.m_Handle, 0, sizeof(hk.m_KeyInContract.m_Handle));
    Env::Memcpy(hk.m_KeyInContract.m_Handle, ownerRec.m_Handle,
                PriviMe::s_MaxHandleLen);

    PriviMe::Profile profile;
    if (!Env::VarReader::Read_T(hk, profile)) {
        Env::DocAddNum("registered", 0u);
        return;
    }

    Env::DocAddNum("registered", 1u);
    Env::DocAddText("handle", ownerRec.m_Handle);
    DocAddWalletId("wallet_id", profile.m_WalletIdRaw);
    if (profile.m_DisplayName[0])
        Env::DocAddText("display_name", profile.m_DisplayName);
    Env::DocAddNum("registered_height", profile.m_RegisteredHeight);

    // Avatar hash lookup
    Env::Key_T<PriviMe::AvatarKey> avk;
    _POD_(avk.m_Prefix.m_Cid) = cid;
    avk.m_KeyInContract.m_Tag = PriviMe::Tags::s_Avatar;
    Env::Memset(avk.m_KeyInContract.m_Handle, 0, sizeof(avk.m_KeyInContract.m_Handle));
    Env::Memcpy(avk.m_KeyInContract.m_Handle, ownerRec.m_Handle, PriviMe::s_MaxHandleLen);
    PriviMe::AvatarData avd;
    if (Env::VarReader::Read_T(avk, avd))
        Env::DocAddBlob_T("avatar_hash", avd.m_Hash);
}

// Claim @handle: generates a TX kernel for Method_3 (RegisterHandle)
void On_register_handle(const ContractID& cid)
{
    PriviMe::Method::RegisterHandle args;
    _POD_(args).SetZero();

    // Read and normalize handle to lowercase
    char szHandle[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(szHandle, 0, sizeof(szHandle));
    if (!Env::DocGetText("handle", szHandle, sizeof(szHandle))) {
        return OnError("handle required");
    }
    // Normalize to lowercase
    for (uint32_t i = 0; i < sizeof(szHandle) - 1 && szHandle[i]; i++) {
        char c = szHandle[i];
        if (c >= 'A' && c <= 'Z') szHandle[i] = c - 'A' + 'a';
    }
    Env::Memcpy(args.m_Handle, szHandle, PriviMe::s_MaxHandleLen);

    // Read display name (optional)
    Env::DocGetText("display_name", args.m_DisplayName, sizeof(args.m_DisplayName));

    // Read WalletID as 34-byte blob (68 hex chars in JSON)
    if (Env::DocGetBlob("wallet_id", args.m_WalletIdRaw, sizeof(args.m_WalletIdRaw))
        != sizeof(args.m_WalletIdRaw)) {
        return OnError("wallet_id must be 34 bytes");
    }

    // Read state to get asset ID and fee
    PriviMe::State s;
    if (!ReadState(cid, s)) return;
    args.m_AssetId = s.m_AssetId;

    // Derive user key for this contract
    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    FundsChange fc;
    fc.m_Aid     = s.m_AssetId;
    fc.m_Amount  = s.m_RegistrationFee;
    fc.m_Consume = 1;

    Env::GenerateKernel(&cid, PriviMe::Method::RegisterHandle::s_iMethod, &args, sizeof(args),
                        &fc, 1, &kid, 1, "PriviMe: register handle", 200000);
}

// Update WalletID and/or display name: generates TX for Method_4 (UpdateProfile)
void On_update_profile(const ContractID& cid)
{
    PriviMe::Method::UpdateProfile args;
    _POD_(args).SetZero();

    if (Env::DocGetBlob("wallet_id", args.m_WalletIdRaw, sizeof(args.m_WalletIdRaw))
        != sizeof(args.m_WalletIdRaw)) {
        return OnError("wallet_id must be 34 bytes");
    }
    Env::DocGetText("display_name", args.m_DisplayName, sizeof(args.m_DisplayName));

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::UpdateProfile::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: update profile", 150000);
}

// Give up @handle: generates TX for Method_5 (ReleaseHandle)
void On_release_handle(const ContractID& cid)
{
    PriviMe::Method::ReleaseHandle args;

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::ReleaseHandle::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: release handle", 130000);
}

// Set or clear avatar hash (generates TX kernel for Method_10)
void On_set_avatar(const ContractID& cid)
{
    PriviMe::Method::SetAvatar args;
    _POD_(args).SetZero();

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);

    // Read avatar_hash (hex string, 64 chars = 32 bytes)
    char szHash[65];
    Env::Memset(szHash, 0, sizeof(szHash));
    if (Env::DocGetText("avatar_hash", szHash, sizeof(szHash))) {
        // Parse hex string to bytes
        for (uint32_t i = 0; i < 32; i++) {
            uint8_t hi = 0, lo = 0;
            char ch = szHash[i * 2];
            char cl = szHash[i * 2 + 1];
            if (ch >= '0' && ch <= '9') hi = ch - '0';
            else if (ch >= 'a' && ch <= 'f') hi = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') hi = ch - 'A' + 10;
            if (cl >= '0' && cl <= '9') lo = cl - '0';
            else if (cl >= 'a' && cl <= 'f') lo = cl - 'a' + 10;
            else if (cl >= 'A' && cl <= 'F') lo = cl - 'A' + 10;
            args.m_Hash[i] = (hi << 4) | lo;
        }
    }
    // If no avatar_hash provided, m_Hash stays all-zero → clears avatar

    Env::KeyID kid(&uk, sizeof(uk));
    Env::GenerateKernel(&cid, PriviMe::Method::SetAvatar::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: set avatar", 130000);
}

// Resolve @handle -> WalletID + display_name (view only, no TX)
void On_resolve_handle(const ContractID& cid)
{
    char szHandle[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(szHandle, 0, sizeof(szHandle));
    if (!Env::DocGetText("handle", szHandle, sizeof(szHandle)))
        return OnError("handle required");

    // Normalize to lowercase
    for (uint32_t i = 0; i < sizeof(szHandle) - 1 && szHandle[i]; i++) {
        char c = szHandle[i];
        if (c >= 'A' && c <= 'Z') szHandle[i] = c - 'A' + 'a';
    }

    Env::Key_T<PriviMe::HandleKey> k;
    _POD_(k.m_Prefix.m_Cid) = cid;
    k.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;
    Env::Memset(k.m_KeyInContract.m_Handle, 0, sizeof(k.m_KeyInContract.m_Handle));
    Env::Memcpy(k.m_KeyInContract.m_Handle, szHandle, PriviMe::s_MaxHandleLen);

    PriviMe::Profile profile;
    if (!Env::VarReader::Read_T(k, profile))
        return OnError("handle not found");

    DocAddWalletId("wallet_id", profile.m_WalletIdRaw);
    Env::DocAddNum("registered_height", profile.m_RegisteredHeight);
    if (profile.m_DisplayName[0])
        Env::DocAddText("display_name", profile.m_DisplayName);

    // Look up avatar hash (tag=3)
    Env::Key_T<PriviMe::AvatarKey> ak;
    _POD_(ak.m_Prefix.m_Cid) = cid;
    ak.m_KeyInContract.m_Tag = PriviMe::Tags::s_Avatar;
    Env::Memset(ak.m_KeyInContract.m_Handle, 0, sizeof(ak.m_KeyInContract.m_Handle));
    Env::Memcpy(ak.m_KeyInContract.m_Handle, szHandle, PriviMe::s_MaxHandleLen);
    PriviMe::AvatarData avatarData;
    if (Env::VarReader::Read_T(ak, avatarData))
        Env::DocAddBlob_T("avatar_hash", avatarData.m_Hash);
}

// Prefix search: returns all handles starting with the given prefix (max 20 results)
void On_search_handles(const ContractID& cid)
{
    char szPrefix[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(szPrefix, 0, sizeof(szPrefix));
    if (!Env::DocGetText("prefix", szPrefix, sizeof(szPrefix)))
        return OnError("prefix required");

    // Normalize to lowercase
    uint32_t prefixLen = 0;
    for (uint32_t i = 0; i < PriviMe::s_MaxHandleLen && szPrefix[i]; i++) {
        char c = szPrefix[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        szPrefix[i] = c;
        prefixLen++;
    }
    if (prefixLen == 0) return OnError("prefix empty");

    // Build key range: k0 = prefix padded with 0x00, k1 = prefix padded with 0xFF
    Env::Key_T<PriviMe::HandleKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;
    Env::Memset(k0.m_KeyInContract.m_Handle, 0, sizeof(k0.m_KeyInContract.m_Handle));
    Env::Memcpy(k0.m_KeyInContract.m_Handle, szPrefix, prefixLen);

    _POD_(k1.m_Prefix.m_Cid) = cid;
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;
    Env::Memset(k1.m_KeyInContract.m_Handle, 0, sizeof(k1.m_KeyInContract.m_Handle));
    Env::Memcpy(k1.m_KeyInContract.m_Handle, szPrefix, prefixLen);
    // Fill remaining bytes after prefix with 0xFF to create upper bound
    for (uint32_t i = prefixLen; i < PriviMe::s_MaxHandleLen; i++)
        k1.m_KeyInContract.m_Handle[i] = (char)0xFF;

    Env::DocArray gr("results");
    uint32_t count = 0;
    Env::VarReader scanner(k0, k1);
    Env::Key_T<PriviMe::HandleKey> key;
    PriviMe::Profile p;

    while (scanner.MoveNext_T(key, p) && count < 20)
    {
        Env::DocGroup entry("");
        Env::DocAddText("handle", key.m_KeyInContract.m_Handle);
        DocAddWalletId("wallet_id", p.m_WalletIdRaw);
        Env::DocAddNum("registered_height", p.m_RegisteredHeight);
        if (p.m_DisplayName[0])
            Env::DocAddText("display_name", p.m_DisplayName);
        // Avatar hash lookup
        Env::Key_T<PriviMe::AvatarKey> avk;
        _POD_(avk.m_Prefix.m_Cid) = cid;
        avk.m_KeyInContract.m_Tag = PriviMe::Tags::s_Avatar;
        Env::Memcpy(avk.m_KeyInContract.m_Handle, key.m_KeyInContract.m_Handle, sizeof(avk.m_KeyInContract.m_Handle));
        PriviMe::AvatarData avd;
        if (Env::VarReader::Read_T(avk, avd))
            Env::DocAddBlob_T("avatar_hash", avd.m_Hash);
        count++;
    }
}

// Reverse lookup: WalletID -> @handle + display_name (O(n) scan — acceptable for v1)
// Used by UI to show @handle next to incoming messages (sender is a WalletID)
void On_resolve_walletid(const ContractID& cid)
{
    uint8_t targetId[34];
    if (Env::DocGetBlob("walletid", targetId, sizeof(targetId)) != sizeof(targetId))
        return OnError("walletid must be 34 bytes");

    // Scan all HandleKey entries, compare profile.m_WalletIdRaw
    Env::Key_T<PriviMe::HandleKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract).SetZero();
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    _POD_(k1.m_Prefix.m_Cid) = cid;
    Env::Memset(&k1.m_KeyInContract, 0xFF, sizeof(k1.m_KeyInContract));
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    Env::VarReader scanner(k0, k1);
    Env::Key_T<PriviMe::HandleKey> key;
    PriviMe::Profile p;

    while (scanner.MoveNext_T(key, p))
    {
        // Compare raw WalletID bytes
        bool match = true;
        for (uint32_t i = 0; i < sizeof(targetId); i++) {
            if (p.m_WalletIdRaw[i] != targetId[i]) { match = false; break; }
        }
        if (!match) continue;

        // Found
        Env::DocAddText("handle", key.m_KeyInContract.m_Handle);
        if (p.m_DisplayName[0])
            Env::DocAddText("display_name", p.m_DisplayName);
        Env::DocAddNum("registered_height", p.m_RegisteredHeight);
        return;
    }

    OnError("wallet ID not found");
}

// Returns last 20 registered handles sorted by registered_height (most recent first)
void On_view_recent(const ContractID& cid)
{
    // Compact slot for circular buffer
    struct Slot {
        char     handle[PriviMe::s_MaxHandleLen];
        char     display_name[PriviMe::s_MaxDisplayNameLen];
        uint8_t  wallet_id[34];
        uint64_t registered_height;
    };

    static const uint32_t MAX_RECENT = 20;
    Slot* buf = (Slot*) Env::Heap_Alloc(sizeof(Slot) * MAX_RECENT);
    uint32_t count = 0, write = 0;

    // Scan all handles into circular buffer (last 20 by registered_height)
    Env::Key_T<PriviMe::HandleKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract).SetZero();
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    _POD_(k1.m_Prefix.m_Cid) = cid;
    Env::Memset(&k1.m_KeyInContract, 0xFF, sizeof(k1.m_KeyInContract));
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    {
        Env::VarReader scanner(k0, k1);
        Env::Key_T<PriviMe::HandleKey> key;
        PriviMe::Profile p;
        while (scanner.MoveNext_T(key, p))
        {
            Slot& s = buf[write];
            Env::Memcpy(s.handle, key.m_KeyInContract.m_Handle, sizeof(s.handle));
            Env::Memcpy(s.display_name, p.m_DisplayName, sizeof(s.display_name));
            Env::Memcpy(s.wallet_id, p.m_WalletIdRaw, sizeof(s.wallet_id));
            s.registered_height = p.m_RegisteredHeight;
            write = (write + 1) % MAX_RECENT;
            if (count < MAX_RECENT) count++;
        }
    }

    // Output newest first
    Env::DocArray gr("recent");
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t idx = (write + MAX_RECENT - 1 - i) % MAX_RECENT;
        Slot& s = buf[idx];
        Env::DocGroup entry("");
        Env::DocAddText("handle", s.handle);
        DocAddWalletId("wallet_id", s.wallet_id);
        Env::DocAddNum("registered_height", s.registered_height);
        if (s.display_name[0])
            Env::DocAddText("display_name", s.display_name);
    }

    Env::Heap_Free(buf);
}

// ============================================================================
// Dispatcher: Method_1
// ============================================================================
BEAM_EXPORT void Method_1()
{
    Env::DocGroup root("");

    char szRole[16], szAction[32];
    if (!Env::DocGetText("role", szRole, sizeof(szRole)))
        return OnError("role required");
    if (!Env::DocGetText("action", szAction, sizeof(szAction)))
        return OnError("action required");

    ContractID cid;
    Env::DocGet("cid", cid);

    if (!Env::Strcmp(szRole, "manager"))
    {
        if (!Env::Strcmp(szAction, "create_contract"))  return On_create_contract(cid);
        if (!Env::Strcmp(szAction, "view_contracts"))   return On_view_contracts(cid);
        if (!Env::Strcmp(szAction, "view_pool"))        return On_view_pool(cid);
        if (!Env::Strcmp(szAction, "withdraw"))         return On_withdraw(cid);
        if (!Env::Strcmp(szAction, "set_owner"))        return On_set_owner(cid);
        if (!Env::Strcmp(szAction, "set_config"))       return On_set_config(cid);
        if (!Env::Strcmp(szAction, "view_all"))         return On_view_all(cid);
        if (!Env::Strcmp(szAction, "explicit_upgrade")) return On_explicit_upgrade(cid);
        if (!Env::Strcmp(szAction, "schedule_upgrade"))   return On_schedule_upgrade(cid);
        if (!Env::Strcmp(szAction, "replace_admin"))       return On_replace_admin(cid);
        if (!Env::Strcmp(szAction, "set_approvers"))       return On_set_approvers(cid);
        if (!Env::Strcmp(szAction, "view_contract_info"))  return On_view_contract_info(cid);
        return OnError("invalid action");
    }

    if (!Env::Strcmp(szRole, "user"))
    {
        if (!Env::Strcmp(szAction, "my_handle"))        return On_my_handle(cid);
        if (!Env::Strcmp(szAction, "register_handle"))  return On_register_handle(cid);
        if (!Env::Strcmp(szAction, "update_profile"))   return On_update_profile(cid);
        if (!Env::Strcmp(szAction, "release_handle"))   return On_release_handle(cid);
        if (!Env::Strcmp(szAction, "set_avatar"))       return On_set_avatar(cid);
        if (!Env::Strcmp(szAction, "resolve_handle"))   return On_resolve_handle(cid);
        if (!Env::Strcmp(szAction, "search_handles"))  return On_search_handles(cid);
        if (!Env::Strcmp(szAction, "resolve_walletid")) return On_resolve_walletid(cid);
        if (!Env::Strcmp(szAction, "view_recent"))      return On_view_recent(cid);
        return OnError("invalid action");
    }

    OnError("invalid role");
}
