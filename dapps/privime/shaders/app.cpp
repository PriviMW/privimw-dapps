// PriviMe App Shader
// Client-side logic: reads identity registry + group chat state, generates TX kernels.
// Upgradable3 admin actions (schedule/apply upgrade) use owner key.
#include "common.h"
#include "app_common_impl.h"
#include "upgradable3/app_common_impl.h"
#include "contract.h"

void OnError(const char* sz)
{
    Env::DocAddText("error", sz);
}

// Forward declaration
bool DocGetTextOrHex(const char* key, char* szOut, uint32_t maxLen);

// ============================================================================
// Key derivation
// ============================================================================

struct OwnerKey {
    uint8_t  m_pSeed[16];
    ShaderID m_SID;

    OwnerKey() {
        const char szSeed[] = "privime-owner-key";
        Env::Memcpy(m_pSeed, szSeed, sizeof(m_pSeed));
        _POD_(m_SID) = PriviMe::s_pSID[0];
    }

    void DerivePk(PubKey& pk) const { Env::DerivePk(pk, this, sizeof(*this)); }
};

struct UserKey {
    uint8_t    m_pSeed[16];
    ContractID m_Cid;

    UserKey() { _POD_(*this).SetZero(); }
    UserKey(const ContractID& cid) {
        const char szSeed[] = "privime-user-key0";
        Env::Memcpy(m_pSeed, szSeed, sizeof(m_pSeed));
        _POD_(m_Cid) = cid;
    }

    void DerivePk(PubKey& pk) const { Env::DerivePk(pk, this, sizeof(*this)); }
};

// ============================================================================
// Helpers
// ============================================================================

static Upgradable3::Manager::VerInfo GetVerInfo()
{
    Upgradable3::Manager::VerInfo vi;
    vi.m_pSid = PriviMe::s_pSID;
    vi.m_Versions = sizeof(PriviMe::s_pSID) / sizeof(PriviMe::s_pSID[0]);
    return vi;
}

void DocAddWalletId(const char* szKey, const uint8_t* raw34)
{
    Env::DocAddBlob(szKey, raw34, 34);
}

void DocAddGroupId(const char* szKey, const uint8_t* id32)
{
    Env::DocAddBlob(szKey, id32, 32);
}

bool IsZero32(const uint8_t* p)
{
    for (uint32_t i = 0; i < 32; i++)
        if (p[i] != 0) return false;
    return true;
}

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

// Read group_id from doc args (64 hex chars = 32 bytes)
bool DocGetGroupId(uint8_t* groupId)
{
    if (Env::DocGetBlob("group_id", groupId, 32) != 32) {
        OnError("group_id must be 32 bytes");
        return false;
    }
    return true;
}

// Read target handle from doc args
bool DocGetTargetHandle(char* handle)
{
    Env::Memset(handle, 0, PriviMe::s_MaxHandleLen);
    char buf[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(buf, 0, sizeof(buf));
    if (!Env::DocGetText("target_handle", buf, sizeof(buf))) {
        OnError("target_handle required");
        return false;
    }
    // Normalize to lowercase
    for (uint32_t i = 0; i < PriviMe::s_MaxHandleLen && buf[i]; i++) {
        char c = buf[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        buf[i] = c;
    }
    Env::Memcpy(handle, buf, PriviMe::s_MaxHandleLen);
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
            {   Env::DocGroup grM("create_contract"); }
            {   Env::DocGroup grM("view_contracts"); }
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
            {   Env::DocGroup grM("view_contract_info"); }
        }
        {
            Env::DocGroup grRole("user");
            // Identity
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
            // Groups
            {
                Env::DocGroup grM("create_group");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("name", "string");
                Env::DocAddText("is_public", "uint32");
                Env::DocAddText("require_approval", "uint32");
                Env::DocAddText("max_members", "uint32");
                Env::DocAddText("default_permissions", "uint32");
            }
            {
                Env::DocGroup grM("join_group");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
            }
            {
                Env::DocGroup grM("remove_member");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
                Env::DocAddText("target_handle", "string");
                Env::DocAddText("ban", "uint32");
            }
            {
                Env::DocGroup grM("set_member_role");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
                Env::DocAddText("target_handle", "string");
                Env::DocAddText("new_role", "uint32");
                Env::DocAddText("permissions", "uint32");
            }
            {
                Env::DocGroup grM("update_group_info");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
                Env::DocAddText("name", "string");
                Env::DocAddText("is_public", "uint32");
                Env::DocAddText("require_approval", "uint32");
                Env::DocAddText("default_permissions", "uint32");
            }
            {
                Env::DocGroup grM("leave_group");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
            }
            {
                Env::DocGroup grM("transfer_ownership");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
                Env::DocAddText("new_creator", "string");
            }
            {
                Env::DocGroup grM("delete_group");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
            }
            // Group views
            {
                Env::DocGroup grM("view_group");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
            }
            {
                Env::DocGroup grM("list_members");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("group_id", "hex32");
            }
            {
                Env::DocGroup grM("list_my_groups");
                Env::DocAddText("cid", "ContractID");
            }
            {
                Env::DocGroup grM("search_groups");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("prefix", "string");
            }
        }
    }
}

// ============================================================================
// Manager handlers (unchanged from v2)
// ============================================================================

void On_create_contract(const ContractID&)
{
    PriviMe::Method::Ctor args;
    _POD_(args).SetZero();
    args.m_Upgradable.m_MinApprovers = 1;
    args.m_Upgradable.m_hMinUpgradeDelay = 0;

    OwnerKey ok;
    ok.DerivePk(args.m_Upgradable.m_pAdmin[0]);
    ok.DerivePk(args.m_OwnerPk);

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
    fc.m_Aid = s.m_AssetId;
    fc.m_Amount = args.m_Amount;
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

    Env::Key_T<PriviMe::HandleKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract).SetZero();
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    _POD_(k1.m_Prefix.m_Cid) = cid;
    Env::Memset(&k1.m_KeyInContract, 0xFF, sizeof(k1.m_KeyInContract));
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;

    Env::DocArray gr("handles");
    uint32_t count = 0;
    Env::VarReader scanner(k0, k1);
    Env::Key_T<PriviMe::HandleKey> key;
    PriviMe::Profile p;
    while (scanner.MoveNext_T(key, p) && count < 100) {
        Env::DocGroup entry("");
        Env::DocAddText("handle", key.m_KeyInContract.m_Handle);
        DocAddWalletId("wallet_id", p.m_WalletIdRaw);
        Env::DocAddNum("registered_height", p.m_RegisteredHeight);
        if (p.m_DisplayName[0])
            Env::DocAddText("display_name", p.m_DisplayName);
        count++;
    }
}

void On_view_contracts(const ContractID&)
{
    Env::DocArray gr("contracts");
    static const uint32_t nVersions = sizeof(PriviMe::s_pSID) / sizeof(PriviMe::s_pSID[0]);
    for (uint32_t v = 0; v < nVersions; v++) {
        WalkerContracts wlk;
        for (wlk.Enum(PriviMe::s_pSID[v]); wlk.MoveNext(); ) {
            Env::DocGroup entry("");
            Env::DocAddBlob_T("cid", wlk.m_Key.m_KeyInContract.m_Cid);
            Env::DocAddNum("height", wlk.m_Height);
            Env::DocAddNum("version", v);
        }
    }
}

void On_explicit_upgrade(const ContractID& cid) { Upgradable3::Manager::MultiSigRitual::Perform_ExplicitUpgrade(cid); }

void On_schedule_upgrade(const ContractID& cid)
{
    Height hTarget = 0;
    Env::DocGet("hTarget", hTarget);
    if (!hTarget) return OnError("hTarget required");

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));
    auto vi = GetVerInfo();
    Upgradable3::Manager::MultiSigRitual::Perform_ScheduleUpgrade(vi, cid, kid, hTarget);
}

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

void On_set_approvers(const ContractID& cid)
{
    uint32_t newVal = 0;
    Env::DocGet("newVal", newVal);
    if (!newVal) return OnError("newVal required");

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));
    Upgradable3::Manager::MultiSigRitual::Perform_SetApprovers(cid, kid, newVal);
}

void On_view_contract_info(const ContractID&)
{
    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));
    auto vi = GetVerInfo();
    vi.DumpAll(&kid);
}

// ============================================================================
// User handlers — Identity
// ============================================================================

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

    Env::Key_T<PriviMe::HandleKey> hk;
    _POD_(hk.m_Prefix.m_Cid) = cid;
    hk.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;
    Env::Memset(hk.m_KeyInContract.m_Handle, 0, sizeof(hk.m_KeyInContract.m_Handle));
    Env::Memcpy(hk.m_KeyInContract.m_Handle, ownerRec.m_Handle, PriviMe::s_MaxHandleLen);

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
}

void On_register_handle(const ContractID& cid)
{
    PriviMe::Method::RegisterHandle args;
    _POD_(args).SetZero();

    char szHandle[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(szHandle, 0, sizeof(szHandle));
    if (!Env::DocGetText("handle", szHandle, sizeof(szHandle)))
        return OnError("handle required");

    for (uint32_t i = 0; i < sizeof(szHandle) - 1 && szHandle[i]; i++) {
        char c = szHandle[i];
        if (c >= 'A' && c <= 'Z') szHandle[i] = c - 'A' + 'a';
    }
    Env::Memcpy(args.m_Handle, szHandle, PriviMe::s_MaxHandleLen);

    DocGetTextOrHex("display_name", args.m_DisplayName, sizeof(args.m_DisplayName));

    if (Env::DocGetBlob("wallet_id", args.m_WalletIdRaw, sizeof(args.m_WalletIdRaw))
        != sizeof(args.m_WalletIdRaw))
        return OnError("wallet_id must be 34 bytes");

    PriviMe::State s;
    if (!ReadState(cid, s)) return;
    args.m_AssetId = s.m_AssetId;

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    FundsChange fc;
    fc.m_Aid = s.m_AssetId;
    fc.m_Amount = s.m_RegistrationFee;
    fc.m_Consume = 1;

    Env::GenerateKernel(&cid, PriviMe::Method::RegisterHandle::s_iMethod, &args, sizeof(args),
                        &fc, 1, &kid, 1, "PriviMe: register handle", 200000);
}

void On_update_profile(const ContractID& cid)
{
    PriviMe::Method::UpdateProfile args;
    _POD_(args).SetZero();

    if (Env::DocGetBlob("wallet_id", args.m_WalletIdRaw, sizeof(args.m_WalletIdRaw))
        != sizeof(args.m_WalletIdRaw))
        return OnError("wallet_id must be 34 bytes");

    DocGetTextOrHex("display_name", args.m_DisplayName, sizeof(args.m_DisplayName));

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::UpdateProfile::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: update profile", 150000);
}

void On_release_handle(const ContractID& cid)
{
    PriviMe::Method::ReleaseHandle args;

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::ReleaseHandle::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: release handle", 130000);
}

void On_resolve_handle(const ContractID& cid)
{
    char szHandle[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(szHandle, 0, sizeof(szHandle));
    if (!Env::DocGetText("handle", szHandle, sizeof(szHandle)))
        return OnError("handle required");

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
}

void On_search_handles(const ContractID& cid)
{
    char szPrefix[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(szPrefix, 0, sizeof(szPrefix));
    if (!Env::DocGetText("prefix", szPrefix, sizeof(szPrefix)))
        return OnError("prefix required");

    uint32_t prefixLen = 0;
    for (uint32_t i = 0; i < PriviMe::s_MaxHandleLen && szPrefix[i]; i++) {
        char c = szPrefix[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        szPrefix[i] = c;
        prefixLen++;
    }
    if (prefixLen == 0) return OnError("prefix empty");

    Env::Key_T<PriviMe::HandleKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;
    Env::Memset(k0.m_KeyInContract.m_Handle, 0, sizeof(k0.m_KeyInContract.m_Handle));
    Env::Memcpy(k0.m_KeyInContract.m_Handle, szPrefix, prefixLen);

    _POD_(k1.m_Prefix.m_Cid) = cid;
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Handle;
    Env::Memset(k1.m_KeyInContract.m_Handle, 0, sizeof(k1.m_KeyInContract.m_Handle));
    Env::Memcpy(k1.m_KeyInContract.m_Handle, szPrefix, prefixLen);
    for (uint32_t i = prefixLen; i < PriviMe::s_MaxHandleLen; i++)
        k1.m_KeyInContract.m_Handle[i] = (char)0xFF;

    Env::DocArray gr("results");
    uint32_t count = 0;
    Env::VarReader scanner(k0, k1);
    Env::Key_T<PriviMe::HandleKey> key;
    PriviMe::Profile p;
    while (scanner.MoveNext_T(key, p) && count < 20) {
        Env::DocGroup entry("");
        Env::DocAddText("handle", key.m_KeyInContract.m_Handle);
        DocAddWalletId("wallet_id", p.m_WalletIdRaw);
        Env::DocAddNum("registered_height", p.m_RegisteredHeight);
        if (p.m_DisplayName[0])
            Env::DocAddText("display_name", p.m_DisplayName);
        count++;
    }
}

void On_resolve_walletid(const ContractID& cid)
{
    uint8_t targetId[34];
    if (Env::DocGetBlob("walletid", targetId, sizeof(targetId)) != sizeof(targetId))
        return OnError("walletid must be 34 bytes");

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
    while (scanner.MoveNext_T(key, p)) {
        bool match = true;
        for (uint32_t i = 0; i < sizeof(targetId); i++) {
            if (p.m_WalletIdRaw[i] != targetId[i]) { match = false; break; }
        }
        if (!match) continue;

        Env::DocAddText("handle", key.m_KeyInContract.m_Handle);
        if (p.m_DisplayName[0])
            Env::DocAddText("display_name", p.m_DisplayName);
        Env::DocAddNum("registered_height", p.m_RegisteredHeight);
        return;
    }
    OnError("wallet ID not found");
}

void On_view_recent(const ContractID& cid)
{
    struct Slot {
        char handle[PriviMe::s_MaxHandleLen];
        char display_name[PriviMe::s_MaxDisplayNameLen];
        uint8_t wallet_id[34];
        uint64_t registered_height;
    };
    static const uint32_t MAX_RECENT = 20;
    Slot* buf = (Slot*)Env::Heap_Alloc(sizeof(Slot) * MAX_RECENT);
    uint32_t count = 0, write = 0;

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
        while (scanner.MoveNext_T(key, p)) {
            Slot& s = buf[write];
            Env::Memcpy(s.handle, key.m_KeyInContract.m_Handle, sizeof(s.handle));
            Env::Memcpy(s.display_name, p.m_DisplayName, sizeof(s.display_name));
            Env::Memcpy(s.wallet_id, p.m_WalletIdRaw, sizeof(s.wallet_id));
            s.registered_height = p.m_RegisteredHeight;
            write = (write + 1) % MAX_RECENT;
            if (count < MAX_RECENT) count++;
        }
    }

    Env::DocArray gr("recent");
    for (uint32_t i = 0; i < count; i++) {
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
// Hex decode helper — decodes hex string to raw bytes (for values with spaces/emoji)
// ============================================================================

static uint8_t HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xFF;
}

// Try to read text from "key" first, then fall back to "key_hex" (hex-encoded UTF-8).
// Returns true if either succeeds, output in szOut (null-terminated).
bool DocGetTextOrHex(const char* key, char* szOut, uint32_t maxLen)
{
    if (Env::DocGetText(key, szOut, maxLen))
        return true;

    // Try hex variant: "key_hex"
    char hexKey[48];
    uint32_t keyLen = 0;
    while (key[keyLen] && keyLen < 40) { hexKey[keyLen] = key[keyLen]; keyLen++; }
    hexKey[keyLen] = '_'; hexKey[keyLen+1] = 'h'; hexKey[keyLen+2] = 'e'; hexKey[keyLen+3] = 'x'; hexKey[keyLen+4] = 0;

    char hexBuf[256];
    Env::Memset(hexBuf, 0, sizeof(hexBuf));
    if (!Env::DocGetText(hexKey, hexBuf, sizeof(hexBuf)))
        return false;

    // Decode hex → raw bytes
    Env::Memset(szOut, 0, maxLen);
    uint32_t i = 0, o = 0;
    while (hexBuf[i] && hexBuf[i+1] && o < maxLen - 1) {
        uint8_t hi = HexDigit(hexBuf[i]);
        uint8_t lo = HexDigit(hexBuf[i+1]);
        if (hi == 0xFF || lo == 0xFF) break;
        szOut[o++] = (char)((hi << 4) | lo);
        i += 2;
    }
    return o > 0;
}

// Try to read blob from "key" first, then fall back to "key_hex" (hex-encoded bytes).
// Returns number of bytes read (>0) if either succeeds, 0 otherwise.
uint32_t DocGetBlobOrHex(const char* key, uint8_t* out, uint32_t size)
{
    uint32_t n = Env::DocGetBlob(key, out, size);
    if (n > 0) return n;

    // Try hex variant: "key_hex"
    char hexKey[48];
    uint32_t keyLen = 0;
    while (key[keyLen] && keyLen < 40) { hexKey[keyLen] = key[keyLen]; keyLen++; }
    hexKey[keyLen] = '_'; hexKey[keyLen+1] = 'h'; hexKey[keyLen+2] = 'e'; hexKey[keyLen+3] = 'x'; hexKey[keyLen+4] = 0;

    char hexBuf[256];
    Env::Memset(hexBuf, 0, sizeof(hexBuf));
    if (!Env::DocGetText(hexKey, hexBuf, sizeof(hexBuf)))
        return 0;

    // Decode hex -> raw bytes
    Env::Memset(out, 0, size);
    uint32_t i = 0, o = 0;
    while (hexBuf[i] && hexBuf[i+1] && o < size) {
        uint8_t hi = HexDigit(hexBuf[i]);
        uint8_t lo = HexDigit(hexBuf[i+1]);
        if (hi == 0xFF || lo == 0xFF) break;
        out[o++] = (uint8_t)((hi << 4) | lo);
        i += 2;
    }
    return o;
}

// ============================================================================
// User handlers — Group TX actions
// ============================================================================

void On_create_group(const ContractID& cid)
{
    PriviMe::Method::CreateGroup args;
    _POD_(args).SetZero();

    char szName[PriviMe::s_MaxGroupNameLen + 1];
    Env::Memset(szName, 0, sizeof(szName));
    if (!DocGetTextOrHex("name", szName, sizeof(szName)))
        return OnError("name required");
    Env::Memcpy(args.m_Name, szName, PriviMe::s_MaxGroupNameLen);

    uint32_t isPublic = 0, requireApproval = 0;
    Env::DocGet("is_public", isPublic);
    Env::DocGet("require_approval", requireApproval);
    args.m_IsPublic = (uint8_t)isPublic;
    args.m_RequireApproval = (uint8_t)requireApproval;

    Env::DocGet("max_members", args.m_MaxMembers);
    Env::DocGet("default_permissions", args.m_DefaultPermissions);

    // Optional join password for private groups (32 raw bytes, supports hex fallback)
    DocGetBlobOrHex("join_password", args.m_JoinPassword, sizeof(args.m_JoinPassword));

    // Generate nonce from current height for unique group_id
    Height h = Env::get_Height();
    args.m_Nonce = (uint32_t)(h ^ 0xDEADBEEF);

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::CreateGroup::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: create group", 250000);
}

void On_join_group(const ContractID& cid)
{
    PriviMe::Method::JoinGroup args;
    _POD_(args).SetZero();

    if (!DocGetGroupId(args.m_GroupId)) return;

    // Optional join password for private groups (supports hex fallback)
    DocGetBlobOrHex("join_password", args.m_JoinPassword, sizeof(args.m_JoinPassword));

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::JoinGroup::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: join group", 250000);
}

void On_remove_member(const ContractID& cid)
{
    PriviMe::Method::RemoveMember args;
    _POD_(args).SetZero();

    if (!DocGetGroupId(args.m_GroupId)) return;
    if (!DocGetTargetHandle(args.m_TargetHandle)) return;

    uint32_t ban = 0;
    Env::DocGet("ban", ban);
    args.m_Ban = (uint8_t)ban;

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::RemoveMember::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: remove member", 200000);
}

void On_set_member_role(const ContractID& cid)
{
    PriviMe::Method::SetMemberRole args;
    _POD_(args).SetZero();

    if (!DocGetGroupId(args.m_GroupId)) return;
    if (!DocGetTargetHandle(args.m_TargetHandle)) return;

    uint32_t newRole = 0;
    Env::DocGet("new_role", newRole);
    args.m_NewRole = (uint8_t)newRole;

    Env::DocGet("permissions", args.m_Permissions);

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::SetMemberRole::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: set member role", 200000);
}

void On_update_group_info(const ContractID& cid)
{
    PriviMe::Method::UpdateGroupInfo args;
    _POD_(args).SetZero();
    args.m_IsPublic = 0xFF;         // no change sentinel
    args.m_RequireApproval = 0xFF;   // no change sentinel

    if (!DocGetGroupId(args.m_GroupId)) return;

    char szName[PriviMe::s_MaxGroupNameLen + 1];
    Env::Memset(szName, 0, sizeof(szName));
    if (DocGetTextOrHex("name", szName, sizeof(szName)))
        Env::Memcpy(args.m_Name, szName, PriviMe::s_MaxGroupNameLen);

    uint32_t isPublic = 0xFF, requireApproval = 0xFF;
    if (Env::DocGet("is_public", isPublic))
        args.m_IsPublic = (uint8_t)isPublic;
    if (Env::DocGet("require_approval", requireApproval))
        args.m_RequireApproval = (uint8_t)requireApproval;

    Env::DocGet("default_permissions", args.m_DefaultPermissions);

    // Description hash and avatar hash can be passed as blobs
    Env::DocGetBlob("description_hash", args.m_DescriptionHash, 32);
    Env::DocGetBlob("avatar_hash", args.m_AvatarHash, 32);

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::UpdateGroupInfo::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: update group info", 200000);
}

void On_leave_group(const ContractID& cid)
{
    PriviMe::Method::LeaveGroup args;
    _POD_(args).SetZero();

    if (!DocGetGroupId(args.m_GroupId)) return;

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::LeaveGroup::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: leave group", 150000);
}

void On_transfer_ownership(const ContractID& cid)
{
    PriviMe::Method::TransferOwnership args;
    _POD_(args).SetZero();

    if (!DocGetGroupId(args.m_GroupId)) return;

    char szHandle[PriviMe::s_MaxHandleLen + 1];
    Env::Memset(szHandle, 0, sizeof(szHandle));
    if (!Env::DocGetText("new_creator", szHandle, sizeof(szHandle)))
        return OnError("new_creator required");
    for (uint32_t i = 0; i < sizeof(szHandle) - 1 && szHandle[i]; i++) {
        char c = szHandle[i];
        if (c >= 'A' && c <= 'Z') szHandle[i] = c - 'A' + 'a';
    }
    Env::Memcpy(args.m_NewCreatorHandle, szHandle, PriviMe::s_MaxHandleLen);

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::TransferOwnership::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: transfer ownership", 200000);
}

void On_delete_group(const ContractID& cid)
{
    PriviMe::Method::DeleteGroup args;
    _POD_(args).SetZero();

    if (!DocGetGroupId(args.m_GroupId)) return;

    UserKey uk(cid);
    uk.DerivePk(args.m_UserPk);
    Env::KeyID kid(&uk, sizeof(uk));

    Env::GenerateKernel(&cid, PriviMe::Method::DeleteGroup::s_iMethod, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "PriviMe: delete group", 300000);
}

// ============================================================================
// User handlers — Group view actions
// ============================================================================

void On_view_group(const ContractID& cid)
{
    uint8_t groupId[32];
    if (!DocGetGroupId(groupId)) return;

    Env::Key_T<PriviMe::GroupKey> gk;
    _POD_(gk.m_Prefix.m_Cid) = cid;
    gk.m_KeyInContract.m_Tag = PriviMe::Tags::s_Group;
    Env::Memcpy(gk.m_KeyInContract.m_GroupId, groupId, 32);

    PriviMe::GroupInfo gi;
    if (!Env::VarReader::Read_T(gk, gi))
        return OnError("group not found");

    DocAddGroupId("group_id", groupId);
    Env::DocAddText("name", gi.m_Name);
    Env::DocAddText("creator", gi.m_CreatorHandle);
    Env::DocAddNum("is_public", (uint32_t)gi.m_IsPublic);
    Env::DocAddNum("require_approval", (uint32_t)gi.m_RequireApproval);
    Env::DocAddNum("max_members", gi.m_MaxMembers);
    Env::DocAddNum("member_count", gi.m_MemberCount);
    Env::DocAddNum("default_permissions", gi.m_DefaultPermissions);
    Env::DocAddNum("created_height", gi.m_CreatedHeight);
    if (!IsZero32(gi.m_DescriptionHash))
        DocAddGroupId("description_hash", gi.m_DescriptionHash);
    if (!IsZero32(gi.m_AvatarHash))
        DocAddGroupId("avatar_hash", gi.m_AvatarHash);
}

void On_list_members(const ContractID& cid)
{
    uint8_t groupId[32];
    if (!DocGetGroupId(groupId)) return;

    Env::Key_T<PriviMe::GroupMemberKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Member;
    Env::Memcpy(k0.m_KeyInContract.m_GroupId, groupId, 32);
    Env::Memset(k0.m_KeyInContract.m_Handle, 0, sizeof(k0.m_KeyInContract.m_Handle));

    _POD_(k1.m_Prefix.m_Cid) = cid;
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Member;
    Env::Memcpy(k1.m_KeyInContract.m_GroupId, groupId, 32);
    Env::Memset(k1.m_KeyInContract.m_Handle, 0xFF, sizeof(k1.m_KeyInContract.m_Handle));

    Env::DocArray gr("members");
    uint32_t count = 0;
    Env::VarReader scanner(k0, k1);
    Env::Key_T<PriviMe::GroupMemberKey> key;
    PriviMe::MemberInfo mi;
    while (scanner.MoveNext_T(key, mi) && count < 200) {
        Env::DocGroup entry("");
        Env::DocAddText("handle", key.m_KeyInContract.m_Handle);
        Env::DocAddNum("role", (uint32_t)mi.m_Role);
        Env::DocAddNum("permissions", mi.m_Permissions);
        Env::DocAddNum("joined_height", mi.m_JoinedHeight);
        count++;
    }
}

void On_list_my_groups(const ContractID& cid)
{
    // Get caller's handle
    UserKey uk(cid);
    PubKey userPk;
    uk.DerivePk(userPk);

    Env::Key_T<PriviMe::OwnerKey> ok;
    _POD_(ok.m_Prefix.m_Cid) = cid;
    ok.m_KeyInContract.m_Tag = PriviMe::Tags::s_Owner;
    _POD_(ok.m_KeyInContract.m_UserPk) = userPk;

    PriviMe::OwnerRecord ownerRec;
    if (!Env::VarReader::Read_T(ok, ownerRec))
        return OnError("not registered");

    // Scan all GroupMemberKey entries, filter by caller's handle
    Env::Key_T<PriviMe::GroupMemberKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract).SetZero();
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Member;

    _POD_(k1.m_Prefix.m_Cid) = cid;
    Env::Memset(&k1.m_KeyInContract, 0xFF, sizeof(k1.m_KeyInContract));
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Member;

    Env::DocArray gr("groups");
    uint32_t count = 0;
    Env::VarReader scanner(k0, k1);
    Env::Key_T<PriviMe::GroupMemberKey> key;
    PriviMe::MemberInfo mi;
    while (scanner.MoveNext_T(key, mi) && count < 50) {
        // Check if this entry is for our handle
        if (Env::Memcmp(key.m_KeyInContract.m_Handle, ownerRec.m_Handle, PriviMe::s_MaxHandleLen) != 0)
            continue;

        // Skip banned
        if (mi.m_Role == PriviMe::Role::s_Banned)
            continue;

        // Load group info
        Env::Key_T<PriviMe::GroupKey> gk;
        _POD_(gk.m_Prefix.m_Cid) = cid;
        gk.m_KeyInContract.m_Tag = PriviMe::Tags::s_Group;
        Env::Memcpy(gk.m_KeyInContract.m_GroupId, key.m_KeyInContract.m_GroupId, 32);

        PriviMe::GroupInfo gi;
        if (!Env::VarReader::Read_T(gk, gi))
            continue; // orphaned member record (group deleted)

        Env::DocGroup entry("");
        DocAddGroupId("group_id", key.m_KeyInContract.m_GroupId);
        Env::DocAddText("name", gi.m_Name);
        Env::DocAddNum("member_count", gi.m_MemberCount);
        Env::DocAddNum("role", (uint32_t)mi.m_Role);
        Env::DocAddNum("is_public", (uint32_t)gi.m_IsPublic);
        count++;
    }
}

void On_search_groups(const ContractID& cid)
{
    char szPrefix[PriviMe::s_MaxGroupNameLen + 1];
    Env::Memset(szPrefix, 0, sizeof(szPrefix));
    if (!Env::DocGetText("prefix", szPrefix, sizeof(szPrefix)))
        return OnError("prefix required");

    uint32_t prefixLen = 0;
    for (uint32_t i = 0; i < PriviMe::s_MaxGroupNameLen && szPrefix[i]; i++) {
        prefixLen++;
    }
    if (prefixLen == 0) return OnError("prefix empty");

    // Scan all GroupKey entries, match name prefix
    Env::Key_T<PriviMe::GroupKey> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract).SetZero();
    k0.m_KeyInContract.m_Tag = PriviMe::Tags::s_Group;

    _POD_(k1.m_Prefix.m_Cid) = cid;
    Env::Memset(&k1.m_KeyInContract, 0xFF, sizeof(k1.m_KeyInContract));
    k1.m_KeyInContract.m_Tag = PriviMe::Tags::s_Group;

    Env::DocArray gr("results");
    uint32_t count = 0;
    Env::VarReader scanner(k0, k1);
    Env::Key_T<PriviMe::GroupKey> key;
    PriviMe::GroupInfo gi;
    while (scanner.MoveNext_T(key, gi) && count < 20) {
        // Only show public groups
        if (!gi.m_IsPublic)
            continue;

        // Check name prefix match
        bool match = true;
        for (uint32_t i = 0; i < prefixLen; i++) {
            char a = szPrefix[i];
            char b = gi.m_Name[i];
            // Case-insensitive compare
            if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
            if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
            if (a != b) { match = false; break; }
        }
        if (!match) continue;

        Env::DocGroup entry("");
        DocAddGroupId("group_id", key.m_KeyInContract.m_GroupId);
        Env::DocAddText("name", gi.m_Name);
        Env::DocAddText("creator", gi.m_CreatorHandle);
        Env::DocAddNum("member_count", gi.m_MemberCount);
        Env::DocAddNum("require_approval", (uint32_t)gi.m_RequireApproval);
        count++;
    }
}

// ============================================================================
// Dispatcher: Method_1
// ============================================================================
BEAM_EXPORT void Method_1()
{
    Env::DocGroup root("");

    char szRole[16], szAction[40];
    if (!Env::DocGetText("role", szRole, sizeof(szRole)))
        return OnError("role required");
    if (!Env::DocGetText("action", szAction, sizeof(szAction)))
        return OnError("action required");

    ContractID cid;
    Env::DocGet("cid", cid);

    if (!Env::Strcmp(szRole, "manager"))
    {
        if (!Env::Strcmp(szAction, "create_contract"))    return On_create_contract(cid);
        if (!Env::Strcmp(szAction, "view_contracts"))     return On_view_contracts(cid);
        if (!Env::Strcmp(szAction, "view_pool"))          return On_view_pool(cid);
        if (!Env::Strcmp(szAction, "withdraw"))           return On_withdraw(cid);
        if (!Env::Strcmp(szAction, "set_owner"))          return On_set_owner(cid);
        if (!Env::Strcmp(szAction, "set_config"))         return On_set_config(cid);
        if (!Env::Strcmp(szAction, "view_all"))           return On_view_all(cid);
        if (!Env::Strcmp(szAction, "explicit_upgrade"))   return On_explicit_upgrade(cid);
        if (!Env::Strcmp(szAction, "schedule_upgrade"))   return On_schedule_upgrade(cid);
        if (!Env::Strcmp(szAction, "replace_admin"))      return On_replace_admin(cid);
        if (!Env::Strcmp(szAction, "set_approvers"))      return On_set_approvers(cid);
        if (!Env::Strcmp(szAction, "view_contract_info")) return On_view_contract_info(cid);
        return OnError("invalid action");
    }

    if (!Env::Strcmp(szRole, "user"))
    {
        // Identity
        if (!Env::Strcmp(szAction, "my_handle"))           return On_my_handle(cid);
        if (!Env::Strcmp(szAction, "register_handle"))     return On_register_handle(cid);
        if (!Env::Strcmp(szAction, "update_profile"))      return On_update_profile(cid);
        if (!Env::Strcmp(szAction, "release_handle"))      return On_release_handle(cid);
        if (!Env::Strcmp(szAction, "resolve_handle"))      return On_resolve_handle(cid);
        if (!Env::Strcmp(szAction, "search_handles"))      return On_search_handles(cid);
        if (!Env::Strcmp(szAction, "resolve_walletid"))    return On_resolve_walletid(cid);
        if (!Env::Strcmp(szAction, "view_recent"))         return On_view_recent(cid);
        // Group TX
        if (!Env::Strcmp(szAction, "create_group"))        return On_create_group(cid);
        if (!Env::Strcmp(szAction, "join_group"))          return On_join_group(cid);
        if (!Env::Strcmp(szAction, "remove_member"))       return On_remove_member(cid);
        if (!Env::Strcmp(szAction, "set_member_role"))     return On_set_member_role(cid);
        if (!Env::Strcmp(szAction, "update_group_info"))   return On_update_group_info(cid);
        if (!Env::Strcmp(szAction, "leave_group"))         return On_leave_group(cid);
        if (!Env::Strcmp(szAction, "transfer_ownership"))  return On_transfer_ownership(cid);
        if (!Env::Strcmp(szAction, "delete_group"))        return On_delete_group(cid);
        // Group views
        if (!Env::Strcmp(szAction, "view_group"))          return On_view_group(cid);
        if (!Env::Strcmp(szAction, "list_members"))        return On_list_members(cid);
        if (!Env::Strcmp(szAction, "list_my_groups"))      return On_list_my_groups(cid);
        if (!Env::Strcmp(szAction, "search_groups"))       return On_search_groups(cid);
        return OnError("invalid action");
    }

    OnError("invalid role");
}
