// PriviMe Contract Shader
// On-chain identity registry + group chat system.
// Upgradable3 provides Method_2 (upgrade dispatch). Business logic starts at Method_3.
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

// Look up caller's handle from their BVM PubKey (reverse lookup)
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

// Load group info by group_id
bool LoadGroup(const uint8_t* groupId, GroupInfo& gi)
{
    GroupKey gk;
    Env::Memcpy(gk.m_GroupId, groupId, 32);
    return Env::LoadVar_T(gk, gi);
}

// Save group info
void SaveGroup(const uint8_t* groupId, const GroupInfo& gi)
{
    GroupKey gk;
    Env::Memcpy(gk.m_GroupId, groupId, 32);
    Env::SaveVar_T(gk, gi);
}

// Load member info
bool LoadMember(const uint8_t* groupId, const char* handle, MemberInfo& mi)
{
    GroupMemberKey mk;
    Env::Memcpy(mk.m_GroupId, groupId, 32);
    Env::Memcpy(mk.m_Handle, handle, s_MaxHandleLen);
    return Env::LoadVar_T(mk, mi);
}

// Save member info
void SaveMember(const uint8_t* groupId, const char* handle, const MemberInfo& mi)
{
    GroupMemberKey mk;
    Env::Memcpy(mk.m_GroupId, groupId, 32);
    Env::Memcpy(mk.m_Handle, handle, s_MaxHandleLen);
    Env::SaveVar_T(mk, mi);
}

// Delete member record
void DeleteMember(const uint8_t* groupId, const char* handle)
{
    GroupMemberKey mk;
    Env::Memcpy(mk.m_GroupId, groupId, 32);
    Env::Memcpy(mk.m_Handle, handle, s_MaxHandleLen);
    Env::DelVar_T(mk);
}

// Check if handle is registered (exists in identity system)
bool IsRegisteredHandle(const char* handle)
{
    HandleKey hk;
    uint32_t len = SafeStrlen(handle, s_MaxHandleLen);
    Env::Memcpy(hk.m_Handle, handle, len);
    Profile p;
    return Env::LoadVar_T(hk, p);
}

// Check if two handles match
bool HandlesMatch(const char* a, const char* b)
{
    return Env::Memcmp(a, b, s_MaxHandleLen) == 0;
}

// Check if a 32-byte value is all zeros
bool IsZero32(const uint8_t* p)
{
    for (uint32_t i = 0; i < 32; i++)
        if (p[i] != 0) return false;
    return true;
}

// Get user group count
uint32_t GetGroupCount(const char* handle)
{
    UserGroupCountKey ck;
    Env::Memcpy(ck.m_Handle, handle, s_MaxHandleLen);
    UserGroupCount cnt;
    if (Env::LoadVar_T(ck, cnt))
        return cnt.m_Count;
    return 0;
}

// Set user group count
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
    // Verify caller has a registered handle
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Check group creation limit
    uint32_t grpCount = PriviMe::GetGroupCount(callerHandle);
    Env::Halt_if(grpCount >= PriviMe::s_MaxGroupsPerUser);

    // Validate group name
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

    // Ensure group_id doesn't already exist (collision check)
    PriviMe::GroupInfo existing;
    Env::Halt_if(PriviMe::LoadGroup(groupId, existing));

    // Create group info
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
    gi.m_MemberCount = 1; // Creator is first member
    gi.m_PinCount = 0;
    PriviMe::SaveGroup(groupId, gi);

    // Add creator as first member with Creator role + all permissions
    PriviMe::MemberInfo mi;
    mi.m_Role = PriviMe::Role::s_Creator;
    mi.m_Permissions = PriviMe::Perm::s_All;
    mi.m_JoinedHeight = (uint32_t)h;
    PriviMe::SaveMember(groupId, callerHandle, mi);

    // Increment user's group creation count
    PriviMe::SetGroupCount(callerHandle, grpCount + 1);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 11: JoinGroup / AddMember
// ============================================================================
BEAM_EXPORT void Method_11(const PriviMe::Method::JoinGroup& r)
{
    // Verify caller has a registered handle
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Check not already a member
    PriviMe::MemberInfo existingMi;
    if (PriviMe::LoadMember(r.m_GroupId, callerHandle, existingMi)) {
        // Already exists — reject if active member or banned
        Env::Halt_if(existingMi.m_Role != PriviMe::Role::s_Banned); // already a member
        Env::Halt(); // banned — can't rejoin
    }

    // Check member limit
    Env::Halt_if(gi.m_MemberCount >= gi.m_MaxMembers);

    if (gi.m_IsPublic) {
        // Public group
        if (gi.m_RequireApproval) {
            // Create join request (admin must approve)
            PriviMe::GroupJoinRequestKey jk;
            Env::Memcpy(jk.m_GroupId, r.m_GroupId, 32);
            Env::Memcpy(jk.m_Handle, callerHandle, PriviMe::s_MaxHandleLen);
            PriviMe::JoinRequest jr;
            jr.m_RequestHeight = (uint32_t)Env::get_Height();
            Env::SaveVar_T(jk, jr);
            // Don't add as member yet — wait for approval
        } else {
            // Auto-join: add as member immediately
            PriviMe::MemberInfo mi;
            mi.m_Role = PriviMe::Role::s_Member;
            mi.m_Permissions = gi.m_DefaultPermissions;
            mi.m_JoinedHeight = (uint32_t)Env::get_Height();
            PriviMe::SaveMember(r.m_GroupId, callerHandle, mi);

            gi.m_MemberCount++;
            PriviMe::SaveGroup(r.m_GroupId, gi);
        }
    } else {
        // Private group — need valid invite secret
        Env::Halt_if(PriviMe::IsZero32(r.m_InviteSecret)); // must provide secret

        // Check invite hasn't expired
        if (gi.m_InviteExpiryHeight > 0) {
            Env::Halt_if(Env::get_Height() > gi.m_InviteExpiryHeight);
        }

        // Verify: SHA256(provided_secret) == stored invite_hash
        HashProcessor::Sha256 hp;
        Env::HashWrite(hp.m_p, r.m_InviteSecret, 32);
        HashValue computedHash;
        Env::HashGetValue(hp.m_p, &computedHash, sizeof(computedHash));
        Env::Halt_if(Env::Memcmp(&computedHash, gi.m_InviteHash, 32) != 0);

        // Valid invite — add as member
        PriviMe::MemberInfo mi;
        mi.m_Role = PriviMe::Role::s_Member;
        mi.m_Permissions = gi.m_DefaultPermissions;
        mi.m_JoinedHeight = (uint32_t)Env::get_Height();
        PriviMe::SaveMember(r.m_GroupId, callerHandle, mi);

        gi.m_MemberCount++;
        PriviMe::SaveGroup(r.m_GroupId, gi);
    }

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 12: RemoveMember / BanMember
// ============================================================================
BEAM_EXPORT void Method_12(const PriviMe::Method::RemoveMember& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Load caller's membership
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));

    // Must be admin or creator
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Admin && callerMi.m_Role != PriviMe::Role::s_Creator);

    // Can't remove/ban yourself
    Env::Halt_if(PriviMe::HandlesMatch(callerHandle, r.m_TargetHandle));

    // Load target's membership
    PriviMe::MemberInfo targetMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, r.m_TargetHandle, targetMi));

    // Can't remove/ban the creator
    Env::Halt_if(targetMi.m_Role == PriviMe::Role::s_Creator);

    // Admins can't remove other admins (only creator can)
    if (targetMi.m_Role == PriviMe::Role::s_Admin) {
        Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Creator);
    }

    if (r.m_Ban) {
        // Ban: keep record with banned role (prevents rejoin)
        targetMi.m_Role = PriviMe::Role::s_Banned;
        targetMi.m_Permissions = 0;
        PriviMe::SaveMember(r.m_GroupId, r.m_TargetHandle, targetMi);
    } else {
        // Remove: delete member record entirely
        PriviMe::DeleteMember(r.m_GroupId, r.m_TargetHandle);
    }

    gi.m_MemberCount--;
    PriviMe::SaveGroup(r.m_GroupId, gi);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 13: SetMemberRole — Creator only
// ============================================================================
BEAM_EXPORT void Method_13(const PriviMe::Method::SetMemberRole& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Only creator can change roles
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Creator);

    // Can't change own role
    Env::Halt_if(PriviMe::HandlesMatch(callerHandle, r.m_TargetHandle));

    // Load target
    PriviMe::MemberInfo targetMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, r.m_TargetHandle, targetMi));

    // Can't change banned members' roles (must unban first via remove+rejoin)
    Env::Halt_if(targetMi.m_Role == PriviMe::Role::s_Banned);

    // Only allow setting to Admin or Member (not Creator or Banned)
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
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Must be admin or creator with ChangeInfo permission
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Admin && callerMi.m_Role != PriviMe::Role::s_Creator);
    if (callerMi.m_Role == PriviMe::Role::s_Admin)
        Env::Halt_if(!(callerMi.m_Permissions & PriviMe::Perm::s_ChangeInfo));

    // Update fields (only non-sentinel values)
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
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Load caller's membership
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));

    // Creator can't leave — must TransferOwnership or DeleteGroup
    Env::Halt_if(callerMi.m_Role == PriviMe::Role::s_Creator);

    // Remove member
    PriviMe::DeleteMember(r.m_GroupId, callerHandle);

    gi.m_MemberCount--;
    PriviMe::SaveGroup(r.m_GroupId, gi);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 16: ApproveJoinRequest — Admin or Creator
// ============================================================================
BEAM_EXPORT void Method_16(const PriviMe::Method::ApproveJoinRequest& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Must be admin or creator
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Admin && callerMi.m_Role != PriviMe::Role::s_Creator);

    // Load join request
    PriviMe::GroupJoinRequestKey jk;
    Env::Memcpy(jk.m_GroupId, r.m_GroupId, 32);
    Env::Memcpy(jk.m_Handle, r.m_TargetHandle, PriviMe::s_MaxHandleLen);
    PriviMe::JoinRequest jr;
    Env::Halt_if(!Env::LoadVar_T(jk, jr));

    // Delete the join request regardless of approve/reject
    Env::DelVar_T(jk);

    if (r.m_Approve) {
        // Check member limit
        Env::Halt_if(gi.m_MemberCount >= gi.m_MaxMembers);

        // Verify target handle still exists
        Env::Halt_if(!PriviMe::IsRegisteredHandle(r.m_TargetHandle));

        // Add as member
        PriviMe::MemberInfo mi;
        mi.m_Role = PriviMe::Role::s_Member;
        mi.m_Permissions = gi.m_DefaultPermissions;
        mi.m_JoinedHeight = (uint32_t)Env::get_Height();
        PriviMe::SaveMember(r.m_GroupId, r.m_TargetHandle, mi);

        gi.m_MemberCount++;
        PriviMe::SaveGroup(r.m_GroupId, gi);
    }

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 17: SetInviteLink — Admin or Creator
// ============================================================================
BEAM_EXPORT void Method_17(const PriviMe::Method::SetInviteLink& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Must be admin or creator
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Admin && callerMi.m_Role != PriviMe::Role::s_Creator);

    // Update invite hash and expiry
    Env::Memcpy(gi.m_InviteHash, r.m_InviteHash, 32);
    gi.m_InviteExpiryHeight = r.m_ExpiryHeight;
    PriviMe::SaveGroup(r.m_GroupId, gi);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 18: TransferOwnership — Creator only
// ============================================================================
BEAM_EXPORT void Method_18(const PriviMe::Method::TransferOwnership& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Only creator can transfer
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Creator);

    // Target must be a current member (not banned)
    PriviMe::MemberInfo targetMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, r.m_NewCreatorHandle, targetMi));
    Env::Halt_if(targetMi.m_Role == PriviMe::Role::s_Banned);

    // Transfer: new creator gets Creator role + all permissions
    targetMi.m_Role = PriviMe::Role::s_Creator;
    targetMi.m_Permissions = PriviMe::Perm::s_All;
    PriviMe::SaveMember(r.m_GroupId, r.m_NewCreatorHandle, targetMi);

    // Old creator becomes admin
    callerMi.m_Role = PriviMe::Role::s_Admin;
    PriviMe::SaveMember(r.m_GroupId, callerHandle, callerMi);

    // Update creator in group info
    Env::Memcpy(gi.m_CreatorHandle, r.m_NewCreatorHandle, PriviMe::s_MaxHandleLen);
    PriviMe::SaveGroup(r.m_GroupId, gi);

    // Transfer group count: decrement old creator, increment new
    uint32_t oldCount = PriviMe::GetGroupCount(callerHandle);
    if (oldCount > 0)
        PriviMe::SetGroupCount(callerHandle, oldCount - 1);

    uint32_t newCount = PriviMe::GetGroupCount(r.m_NewCreatorHandle);
    PriviMe::SetGroupCount(r.m_NewCreatorHandle, newCount + 1);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 19: SetGroupPin — Admin or Creator
// ============================================================================
BEAM_EXPORT void Method_19(const PriviMe::Method::SetGroupPin& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Must be admin or creator with PinMsg permission
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role != PriviMe::Role::s_Admin && callerMi.m_Role != PriviMe::Role::s_Creator);
    if (callerMi.m_Role == PriviMe::Role::s_Admin)
        Env::Halt_if(!(callerMi.m_Permissions & PriviMe::Perm::s_PinMsg));

    if (r.m_Unpin) {
        // Find and remove pin by message hash
        for (uint32_t i = 0; i < gi.m_PinCount; i++) {
            PriviMe::GroupPinKey pk;
            Env::Memcpy(pk.m_GroupId, r.m_GroupId, 32);
            pk.m_PinIndex = i;
            PriviMe::PinInfo pi;
            if (Env::LoadVar_T(pk, pi)) {
                if (Env::Memcmp(pi.m_MessageHash, r.m_MessageHash, 32) == 0) {
                    // Found — delete this pin and shift remaining
                    Env::DelVar_T(pk);

                    // Move last pin to this slot (swap-remove)
                    if (i < gi.m_PinCount - 1) {
                        PriviMe::GroupPinKey lastPk;
                        Env::Memcpy(lastPk.m_GroupId, r.m_GroupId, 32);
                        lastPk.m_PinIndex = gi.m_PinCount - 1;
                        PriviMe::PinInfo lastPi;
                        if (Env::LoadVar_T(lastPk, lastPi)) {
                            Env::SaveVar_T(pk, lastPi); // move last to deleted slot
                            Env::DelVar_T(lastPk);      // delete last
                        }
                    }

                    gi.m_PinCount--;
                    PriviMe::SaveGroup(r.m_GroupId, gi);
                    break;
                }
            }
        }
    } else {
        // Pin: check limit
        Env::Halt_if(gi.m_PinCount >= PriviMe::s_MaxGroupPins);

        // Add pin at next index
        PriviMe::GroupPinKey pk;
        Env::Memcpy(pk.m_GroupId, r.m_GroupId, 32);
        pk.m_PinIndex = gi.m_PinCount;

        PriviMe::PinInfo pi;
        _POD_(pi).SetZero();
        Env::Memcpy(pi.m_SenderHandle, r.m_SenderHandle, PriviMe::s_MaxHandleLen);
        Env::Memcpy(pi.m_MessageHash, r.m_MessageHash, 32);
        pi.m_PinnedHeight = (uint32_t)Env::get_Height();
        Env::SaveVar_T(pk, pi);

        gi.m_PinCount++;
        PriviMe::SaveGroup(r.m_GroupId, gi);
    }

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 20: ReportMember
// ============================================================================
BEAM_EXPORT void Method_20(const PriviMe::Method::ReportMember& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Caller must be a member
    PriviMe::MemberInfo callerMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, callerHandle, callerMi));
    Env::Halt_if(callerMi.m_Role == PriviMe::Role::s_Banned);

    // Can't report yourself
    Env::Halt_if(PriviMe::HandlesMatch(callerHandle, r.m_TargetHandle));

    // Target must be in the group
    PriviMe::MemberInfo targetMi;
    Env::Halt_if(!PriviMe::LoadMember(r.m_GroupId, r.m_TargetHandle, targetMi));

    // Valid reason
    Env::Halt_if(r.m_Reason > 3);

    // Save report (one per reporter-target pair per group)
    PriviMe::ReportKey rk;
    Env::Memcpy(rk.m_GroupId, r.m_GroupId, 32);
    Env::Memcpy(rk.m_Reporter, callerHandle, PriviMe::s_MaxHandleLen);
    Env::Memcpy(rk.m_Target, r.m_TargetHandle, PriviMe::s_MaxHandleLen);

    PriviMe::ReportInfo ri;
    ri.m_Reason = r.m_Reason;
    ri.m_ReportHeight = (uint32_t)Env::get_Height();
    Env::SaveVar_T(rk, ri);

    Env::AddSig(r.m_UserPk);
}

// ============================================================================
// Method 21: DeleteGroup — Creator only
// ============================================================================
BEAM_EXPORT void Method_21(const PriviMe::Method::DeleteGroup& r)
{
    // Verify caller
    char callerHandle[PriviMe::s_MaxHandleLen];
    Env::Memset(callerHandle, 0, sizeof(callerHandle));
    Env::Halt_if(!PriviMe::GetCallerHandle(r.m_UserPk, callerHandle));

    // Load group
    PriviMe::GroupInfo gi;
    Env::Halt_if(!PriviMe::LoadGroup(r.m_GroupId, gi));

    // Only creator can delete
    Env::Halt_if(!PriviMe::HandlesMatch(callerHandle, gi.m_CreatorHandle));

    // Delete group info (Tag 3)
    PriviMe::GroupKey gk;
    Env::Memcpy(gk.m_GroupId, r.m_GroupId, 32);
    Env::DelVar_T(gk);

    // Delete all pins (Tag 6)
    for (uint32_t i = 0; i < gi.m_PinCount; i++) {
        PriviMe::GroupPinKey pk;
        Env::Memcpy(pk.m_GroupId, r.m_GroupId, 32);
        pk.m_PinIndex = i;
        Env::DelVar_T(pk);
    }

    // Note: Member records (Tag 4), join requests (Tag 5), and reports (Tag 7)
    // are NOT deleted here — would require scanning all handles which is expensive.
    // They become orphaned but harmless (group_id no longer resolves).
    // App-level: filter by checking if group exists before displaying.

    // Decrement creator's group count
    uint32_t grpCount = PriviMe::GetGroupCount(callerHandle);
    if (grpCount > 0)
        PriviMe::SetGroupCount(callerHandle, grpCount - 1);

    Env::AddSig(r.m_UserPk);
}
