// PriviMe Contract Shader
// On-chain identity registry + group chat system.
// Upgradable3 provides Method_2 (upgrade dispatch). Business logic starts at Method_3.
// Methods 3-9: identity. Methods 10-17: group chat (8 methods).
#include "common.h"
#include "upgradable3/contract_impl.h"
#include "contract.h"

// ============================================================================
// Upgradable3 callbacks
// ============================================================================

void Upgradable3::OnUpgraded(uint32_t nPrevVersion)
{
    (void)nPrevVersion;
}

uint32_t Upgradable3::get_CurrentVersion()
{
    return 2;
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
            Env::Halt();
        g_State.m_Initialized = 1;
    }
    return g_State;
}

void SaveState()
{
    StateKey k;
    Env::SaveVar_T(k, g_State);
}

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

uint32_t SafeStrlen(const char* s, uint32_t maxLen)
{
    for (uint32_t i = 0; i < maxLen; i++)
        if (s[i] == 0) return i;
    return maxLen;
}

// ============================================================================
// Group helpers
// ============================================================================

bool GetCallerHandle(const PubKey& pk, char* outHandle)
{
    OwnerKey ok;
    _POD_(ok.m_UserPk) = pk;
    OwnerRecord rec;
    if (!Env::LoadVar_T(ok, rec))
        return false;
    Env::Memcpy(outHandle, rec.m_Handle, s_MaxHandleLen);
    return true;
}

bool LoadGroup(const uint8_t* groupId, GroupInfo& gi)
{
    GroupKey gk;
    Env::Memcpy(gk.m_GroupId, groupId, 32);
    return Env::LoadVar_T(gk, gi);
}

void SaveGroup(const uint8_t* groupId, const GroupInfo& gi)
{
    GroupKey gk;
    Env::Memcpy(gk.m_GroupId, groupId, 32);
    Env::SaveVar_T(gk, gi);
}

bool LoadMember(const uint8_t* groupId, const char* handle, MemberInfo& mi)
{
    GroupMemberKey mk;
    Env::Memcpy(mk.m_GroupId, groupId, 32);
    Env::Memcpy(mk.m_Handle, handle, s_MaxHandleLen);
    return Env::LoadVar_T(mk, mi);
}

void SaveMember(const uint8_t* groupId, const char* handle, const MemberInfo& mi)
{
    GroupMemberKey mk;
    Env::Memcpy(mk.m_GroupId, groupId, 32);
    Env::Memcpy(mk.m_Handle, handle, s_MaxHandleLen);
    Env::SaveVar_T(mk, mi);
}

void DeleteMember(const uint8_t* groupId, const char* handle)
{
    GroupMemberKey mk;
    Env::Memcpy(mk.m_GroupId, groupId, 32);
    Env::Memcpy(mk.m_Handle, handle, s_MaxHandleLen);
    Env::DelVar_T(mk);
}

bool IsRegisteredHandle(const char* handle)
{
    HandleKey hk;
    uint32_t len = SafeStrlen(handle, s_MaxHandleLen);
    Env::Memcpy(hk.m_Handle, handle, len);
    Profile p;
    return Env::LoadVar_T(hk, p);
}

bool HandlesMatch(const char* a, const char* b)
{
    return Env::Memcmp(a, b, s_MaxHandleLen) == 0;
}

bool IsZero32(const uint8_t* p)
{
    for (uint32_t i = 0; i < 32; i++)
        if (p[i] != 0) return false;
    return true;
}

uint32_t GetGroupCount(const char* handle)
{
    UserGroupCountKey ck;
    Env::Memcpy(ck.m_Handle, handle, s_MaxHandleLen);
    UserGroupCount cnt;
    if (Env::LoadVar_T(ck, cnt))
        return cnt.m_Count;
    return 0;
}

void SetGroupCount(const char* handle, uint32_t count)
{
    UserGroupCountKey ck;
    Env::Memcpy(ck.m_Handle, handle, s_MaxHandleLen);
    if (count == 0) {
        Env::DelVar_T(ck);
    } else {
        UserGroupCount cnt;
        cnt.m_Count = count;
        Env::SaveVar_T(ck, cnt);
    }
}

} // namespace PriviMe

// ============================================================================
// Constructor
// ============================================================================
BEAM_EXPORT void Ctor(const PriviMe::Method::Ctor& r)
{
    r.m_Upgradable.TestNumApprovers();
    r.m_Upgradable.Save();

    _POD_(PriviMe::g_State).SetZero();
    _POD_(PriviMe::g_State.m_OwnerPk) = r.m_OwnerPk;
    PriviMe::g_State.m_RegistrationFee = PriviMe::s_DefaultFee;
    PriviMe::g_State.m_BuildVariant = PriviMe::s_BuildVariant;
    PriviMe::g_State.m_AssetId = 0;
    PriviMe::g_State.m_Initialized = 1;
    PriviMe::SaveState();
}

// ============================================================================
// Destructor
// ============================================================================
BEAM_EXPORT void Dtor(void*)
{
    PriviMe::State& s = PriviMe::GetState();
    Env::Halt_if(s.m_TotalRegistrations > 0);
    if (s.m_TotalFees > 0)
        Env::FundsUnlock(s.m_AssetId, s.m_TotalFees);
    Env::AddSig(s.m_OwnerPk);
}

// ============================================================================
// Method 3: RegisterHandle
// ============================================================================
BEAM_EXPORT void Method_3(const PriviMe::Method::RegisterHandle& r)
{
    PriviMe::State& s = PriviMe::GetState();
    Env::Halt_if(s.m_Paused);
    Env::Halt_if(r.m_AssetId != s.m_AssetId);

    uint32_t handleLen = PriviMe::SafeStrlen(r.m_Handle, PriviMe::s_MaxHandleLen);
    Env::Halt_if(!PriviMe::IsValidHandle(r.m_Handle, handleLen));

    PriviMe::HandleKey hk;
    Env::Memcpy(hk.m_Handle, r.m_Handle, handleLen);
    PriviMe::Profile existing;
    Env::Halt_if(Env::LoadVar_T(hk, existing));

    PriviMe::OwnerKey ok;
    _POD_(ok.m_UserPk) = r.m_UserPk;
    PriviMe::OwnerRecord existingOwner;
    Env::Halt_if(Env::LoadVar_T(ok, existingOwner));

    Env::FundsLock(s.m_AssetId, s.m_RegistrationFee);

    PriviMe::Profile profile;
    _POD_(profile).SetZero();
    _POD_(profile.m_UserPk) = r.m_UserPk;
    Env::Memcpy(profile.m_WalletIdRaw, r.m_WalletIdRaw, sizeof(profile.m_WalletIdRaw));
    profile.m_RegisteredHeight = Env::get_Height();
    uint32_t displayLen = PriviMe::SafeStrlen(r.m_DisplayName, PriviMe::s_MaxDisplayNameLen);
    if (displayLen > 0)
        Env::Memcpy(profile.m_DisplayName, r.m_DisplayName, displayLen);
    Env::SaveVar_T(hk, profile);

    PriviMe::OwnerRecord ownerRec;
    Env::Memset(ownerRec.m_Handle, 0, sizeof(ownerRec.m_Handle));
    Env::Memcpy(ownerRec.m_Handle, r.m_Handle, handleLen);
    Env::SaveVar_T(ok, ownerRec);

    s.m_TotalRegistrations++;
    s.m_TotalFees += s.m_RegistrationFee;
    PriviMe::SaveState();

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 4: UpdateProfile
// ============================================================================
BEAM_EXPORT void Method_4(const PriviMe::Method::UpdateProfile& r)
{
    PriviMe::OwnerKey ok;
    _POD_(ok.m_UserPk) = r.m_UserPk;
    PriviMe::OwnerRecord ownerRec;
    Env::Halt_if(!Env::LoadVar_T(ok, ownerRec));

    PriviMe::HandleKey hk;
    Env::Memcpy(hk.m_Handle, ownerRec.m_Handle, sizeof(hk.m_Handle));
    PriviMe::Profile profile;
    Env::Halt_if(!Env::LoadVar_T(hk, profile));

    Env::Memcpy(profile.m_WalletIdRaw, r.m_WalletIdRaw, sizeof(profile.m_WalletIdRaw));
    Env::Memset(profile.m_DisplayName, 0, sizeof(profile.m_DisplayName));
    uint32_t displayLen = PriviMe::SafeStrlen(r.m_DisplayName, PriviMe::s_MaxDisplayNameLen);
    if (displayLen > 0)
        Env::Memcpy(profile.m_DisplayName, r.m_DisplayName, displayLen);
    Env::SaveVar_T(hk, profile);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 5: ReleaseHandle
// ============================================================================
BEAM_EXPORT void Method_5(const PriviMe::Method::ReleaseHandle& r)
{
    PriviMe::State& s = PriviMe::GetState();

    PriviMe::OwnerKey ok;
    _POD_(ok.m_UserPk) = r.m_UserPk;
    PriviMe::OwnerRecord ownerRec;
    Env::Halt_if(!Env::LoadVar_T(ok, ownerRec));

    // Block deletion if user is creator of any group (must transfer/delete groups first)
    uint32_t grpCount = PriviMe::GetGroupCount(ownerRec.m_Handle);
    Env::Halt_if(grpCount > 0);

    PriviMe::HandleKey hk;
    Env::Memcpy(hk.m_Handle, ownerRec.m_Handle, sizeof(hk.m_Handle));
    Env::DelVar_T(hk);
    Env::DelVar_T(ok);

    s.m_TotalRegistrations--;
    PriviMe::SaveState();

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 6: Deposit — DISABLED
// ============================================================================
BEAM_EXPORT void Method_6(const PriviMe::Method::DepositDisabled&)
{
    Env::Halt();
}

// ============================================================================
// Method 7: Withdraw
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
// Method 8: SetOwner
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
// Method 9: SetConfig
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

// ============================================================================
// Method 10: CreateGroup
// ============================================================================
BEAM_EXPORT void Method_10(const PriviMe::Method::CreateGroup& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    uint32_t grpCount = PriviMe::GetGroupCount(callerHandle);
    Env::Halt_if(grpCount >= PriviMe::s_MaxGroupsPerUser);

    uint32_t nameLen = PriviMe::SafeStrlen(r.m_Name, PriviMe::s_MaxGroupNameLen);
    Env::Halt_if(nameLen < 1);

    // Generate unique group_id = SHA256(creator_handle + height + nonce + build_variant)
    HashProcessor::Sha256 hp;
    Env::HashWrite(hp.m_p, callerHandle, PriviMe::s_MaxHandleLen);
    Height h = Env::get_Height();
    Env::HashWrite(hp.m_p, &h, sizeof(h));
    Env::HashWrite(hp.m_p, &r.m_Nonce, sizeof(r.m_Nonce));
    Env::HashWrite(hp.m_p, &PriviMe::s_BuildVariant, sizeof(PriviMe::s_BuildVariant));

    HashValue groupIdHash;
    Env::HashGetValue(hp.m_p, &groupIdHash, sizeof(groupIdHash));
    uint8_t groupId[32];
    Env::Memcpy(groupId, &groupIdHash, 32);

    PriviMe::GroupInfo existing;
    Env::Halt_if(PriviMe::LoadGroup(groupId, existing));

    PriviMe::GroupInfo gi;
    _POD_(gi).SetZero();
    Env::Memcpy(gi.m_Name, r.m_Name, nameLen);
    Env::Memcpy(gi.m_CreatorHandle, callerHandle, PriviMe::s_MaxHandleLen);
    gi.m_IsPublic = r.m_IsPublic;
    gi.m_RequireApproval = r.m_RequireApproval;
    gi.m_MaxMembers = (r.m_MaxMembers > 0 && r.m_MaxMembers <= PriviMe::s_MaxGroupMembers)
        ? r.m_MaxMembers : PriviMe::s_MaxGroupMembers;
    gi.m_DefaultPermissions = (r.m_DefaultPermissions > 0)
        ? r.m_DefaultPermissions : PriviMe::Perm::s_Default;
    gi.m_CreatedHeight = (uint32_t)h;
    gi.m_MemberCount = 1;

    // Hash the join password if provided (non-zero)
    if (!PriviMe::IsZero32(r.m_JoinPassword)) {
        HashProcessor::Sha256 pwHp;
        Env::HashWrite(pwHp.m_p, r.m_JoinPassword, 32);
        HashValue pwHash;
        Env::HashGetValue(pwHp.m_p, &pwHash, sizeof(pwHash));
        Env::Memcpy(gi.m_JoinPasswordHash, &pwHash, 32);
    }
    // else: m_JoinPasswordHash is already zero from SetZero()

    PriviMe::SaveGroup(groupId, gi);

    PriviMe::MemberInfo mi;
    mi.m_Role = PriviMe::Role::s_Creator;
    mi.m_Permissions = PriviMe::Perm::s_All;
    mi.m_JoinedHeight = (uint32_t)h;
    PriviMe::SaveMember(groupId, callerHandle, mi);

    PriviMe::SetGroupCount(callerHandle, grpCount + 1);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 11: JoinGroup / AddMember
// ============================================================================
BEAM_EXPORT void Method_11(const PriviMe::Method::JoinGroup& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Check not already a member or banned
    PriviMe::MemberInfo existingMi;
    if (PriviMe::LoadMember(r.m_GroupId, callerHandle, existingMi)) {
        Env::Halt_if(existingMi.m_Role != PriviMe::Role::s_Banned);
        Env::Halt(); // banned — can't rejoin
    }

    Env::Halt_if(gi.m_MemberCount >= gi.m_MaxMembers);

    // If group has a join password, verify it
    if (!PriviMe::IsZero32(gi.m_JoinPasswordHash)) {
        // Caller must provide the correct password
        Env::Halt_if(PriviMe::IsZero32(r.m_JoinPassword)); // no password provided

        // Hash the provided password and compare
        HashProcessor::Sha256 pwHp;
        Env::HashWrite(pwHp.m_p, r.m_JoinPassword, 32);
        HashValue computedHash;
        Env::HashGetValue(pwHp.m_p, &computedHash, sizeof(computedHash));
        Env::Halt_if(Env::Memcmp(&computedHash, gi.m_JoinPasswordHash, 32) != 0);
    }

    PriviMe::MemberInfo mi;
    mi.m_Role = PriviMe::Role::s_Member;
    mi.m_Permissions = gi.m_DefaultPermissions;
    mi.m_JoinedHeight = (uint32_t)Env::get_Height();
    PriviMe::SaveMember(r.m_GroupId, callerHandle, mi);

    gi.m_MemberCount++;
    PriviMe::SaveGroup(r.m_GroupId, gi);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 12: RemoveMember / BanMember
// ============================================================================
BEAM_EXPORT void Method_12(const PriviMe::Method::RemoveMember& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Admin && callerMi.m_Role != PriviMe::Role::s_Creator);

    Env::Halt_if(PriviMe::HandlesMatch(callerHandle, r.m_TargetHandle));

    PriviMe::MemberInfo targetMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, r.m_TargetHandle, targetMi));
    Env::Halt_if(targetMi.m_Role == PriviMe::Role::s_Creator);

    if (targetMi.m_Role == PriviMe::Role::s_Admin) {
        Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Creator);
    }

    if (targetMi.m_Role == PriviMe::Role::s_Banned) {
        // Target is banned — m_Ban=0 means unban (delete ban record, user can rejoin)
        if (!r.m_Ban) {
            PriviMe::DeleteMember(r.m_GroupId, r.m_TargetHandle);
            // Don't increment member count — they're not a member yet, just un-blocked
        }
        // else: already banned, nothing to do
    } else if (r.m_Ban) {
        targetMi.m_Role = PriviMe::Role::s_Banned;
        targetMi.m_Permissions = 0;
        PriviMe::SaveMember(r.m_GroupId, r.m_TargetHandle, targetMi);
        gi.m_MemberCount--;
        PriviMe::SaveGroup(r.m_GroupId, gi);
    } else {
        PriviMe::DeleteMember(r.m_GroupId, r.m_TargetHandle);
        gi.m_MemberCount--;
        PriviMe::SaveGroup(r.m_GroupId, gi);
    }

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 13: SetMemberRole — Creator only
// ============================================================================
BEAM_EXPORT void Method_13(const PriviMe::Method::SetMemberRole& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Creator);

    Env::Halt_if(PriviMe::HandlesMatch(callerHandle, r.m_TargetHandle));

    PriviMe::MemberInfo targetMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, r.m_TargetHandle, targetMi));
    Env::Halt_if(targetMi.m_Role == PriviMe::Role::s_Banned);

    Env::Halt_if(r.m_NewRole != PriviMe::Role::s_Admin && r.m_NewRole != PriviMe::Role::s_Member);

    targetMi.m_Role = r.m_NewRole;
    targetMi.m_Permissions = (r.m_Permissions > 0) ? r.m_Permissions :
        (r.m_NewRole == PriviMe::Role::s_Admin ? PriviMe::Perm::s_All : gi.m_DefaultPermissions);
    PriviMe::SaveMember(r.m_GroupId, r.m_TargetHandle, targetMi);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 14: UpdateGroupInfo — Admin or Creator
// ============================================================================
BEAM_EXPORT void Method_14(const PriviMe::Method::UpdateGroupInfo& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Admin && callerMi.m_Role != PriviMe::Role::s_Creator);
    if (callerMi.m_Role == PriviMe::Role::s_Admin)
        Env::Halt_if(!(callerMi.m_Permissions & PriviMe::Perm::s_ChangeInfo));

    uint32_t nameLen = PriviMe::SafeStrlen(r.m_Name, PriviMe::s_MaxGroupNameLen);
    if (nameLen > 0) {
        Env::Memset(gi.m_Name, 0, sizeof(gi.m_Name));
        Env::Memcpy(gi.m_Name, r.m_Name, nameLen);
    }

    if (!PriviMe::IsZero32(r.m_DescriptionHash))
        Env::Memcpy(gi.m_DescriptionHash, r.m_DescriptionHash, 32);

    if (!PriviMe::IsZero32(r.m_AvatarHash))
        Env::Memcpy(gi.m_AvatarHash, r.m_AvatarHash, 32);

    if (r.m_IsPublic != 0xFF)
        gi.m_IsPublic = r.m_IsPublic;

    if (r.m_RequireApproval != 0xFF)
        gi.m_RequireApproval = r.m_RequireApproval;

    if (r.m_DefaultPermissions > 0)
        gi.m_DefaultPermissions = r.m_DefaultPermissions;

    PriviMe::SaveGroup(r.m_GroupId, gi);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 15: LeaveGroup
// ============================================================================
BEAM_EXPORT void Method_15(const PriviMe::Method::LeaveGroup& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));

    // Creator can't leave — must TransferOwnership or DeleteGroup
    Env::Halt_if(callerMi.m_Role == PriviMe::Role::s_Creator);

    PriviMe::DeleteMember(r.m_GroupId, callerHandle);

    gi.m_MemberCount--;
    PriviMe::SaveGroup(r.m_GroupId, gi);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 16: TransferOwnership — Creator only
// ============================================================================
BEAM_EXPORT void Method_16(const PriviMe::Method::TransferOwnership& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Creator);

    PriviMe::MemberInfo targetMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, r.m_NewCreatorHandle, targetMi));
    Env::Halt_if(targetMi.m_Role == PriviMe::Role::s_Banned);

    targetMi.m_Role = PriviMe::Role::s_Creator;
    targetMi.m_Permissions = PriviMe::Perm::s_All;
    PriviMe::SaveMember(r.m_GroupId, r.m_NewCreatorHandle, targetMi);

    callerMi.m_Role = PriviMe::Role::s_Admin;
    PriviMe::SaveMember(r.m_GroupId, callerHandle, callerMi);

    Env::Memcpy(gi.m_CreatorHandle, r.m_NewCreatorHandle, PriviMe::s_MaxHandleLen);
    PriviMe::SaveGroup(r.m_GroupId, gi);

    uint32_t oldCount = PriviMe::GetGroupCount(callerHandle);
    if (oldCount > 0)
        PriviMe::SetGroupCount(callerHandle, oldCount - 1);

    uint32_t newCount = PriviMe::GetGroupCount(r.m_NewCreatorHandle);
    PriviMe::SetGroupCount(r.m_NewCreatorHandle, newCount + 1);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 17: DeleteGroup — Creator only
// ============================================================================
BEAM_EXPORT void Method_17(const PriviMe::Method::DeleteGroup& r)
{
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    Env::Halt_if(!PriviMe::HandlesMatch(callerHandle, gi.m_CreatorHandle));

    PriviMe::GroupKey gk;
    Env::Memcpy(gk.m_GroupId, r.m_GroupId, 32);
    Env::DelVar_T(gk);

    // Note: Member records (Tag 4) are NOT deleted — would require scanning all handles.
    // They become orphaned but harmless (group_id no longer resolves).

    uint32_t grpCount = PriviMe::GetGroupCount(callerHandle);
    if (grpCount > 0)
        PriviMe::SetGroupCount(callerHandle, grpCount - 1);

    Env::AddSig(r.m_UserPk);
}
