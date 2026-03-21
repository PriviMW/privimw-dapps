// PriviMe Contract Header
// On-chain identity registry + group chat system.
// Uses Upgradable3 (Method_2 reserved for upgrade dispatch).
// Methods 3-9: identity. Methods 10-21: group chat.
#pragma once
#include "upgradable3/contract.h"

namespace PriviMe {

// ShaderID array for Upgradable3 version verification.
// Grows with each upgrade. Update after each recompile (generate-sid.exe).
static const ShaderID s_pSID[] = {
    { 0xad,0x36,0xf9,0x41,0x19,0xcd,0x40,0x45,0xaf,0x3a,0x93,0x0a,0xe1,0x73,0xc9,0x51,0x41,0x52,0x56,0x3f,0x3b,0x75,0xd0,0x15,0x21,0x00,0x2c,0x15,0x14,0x6c,0x2b,0x58 }, // v0 — unpacked Profile
    { 0x22,0x34,0x94,0xa8,0x99,0xe0,0x07,0x3a,0xca,0x5c,0x57,0x87,0xa0,0x8f,0x35,0xa8,0x71,0x55,0xfc,0x6c,0x3e,0x8a,0xd3,0x48,0x15,0x07,0x00,0x79,0x08,0xc0,0xd2,0xc0 }, // v1 — packed Profile/OwnerRecord
    { 0x91,0xef,0xa4,0xa2,0xd1,0xaf,0x89,0x35,0x26,0xcb,0x92,0x68,0x3a,0x9b,0x32,0xb6,0x08,0x96,0xc7,0xb1,0x85,0xe9,0x47,0x40,0x41,0x0b,0xaa,0x5e,0xad,0xc8,0xb7,0xc4 }, // v2-staging (build variant 0)
    { 0xf2,0x8a,0x6a,0x08,0x68,0x07,0x4c,0xbd,0x7f,0x18,0x1f,0x9e,0xea,0x93,0x4d,0x8e,0x14,0x4b,0xcc,0xa6,0x4f,0xbc,0xc1,0xf8,0xcd,0xc9,0x79,0x47,0xeb,0xe6,0x86,0xfd }, // v2-production (build variant 1)
    { 0x7d,0x9e,0xc8,0x20,0xa4,0x83,0xd7,0x5e,0x1a,0x2b,0x49,0xf0,0x96,0x16,0xc8,0x0f,0x03,0x63,0x0d,0xcd,0xff,0xbc,0x84,0xc5,0x17,0xf9,0xd2,0x4b,0xa1,0x7d,0xcc,0x8a }, // v3-staging (upgrade test, build variant 0)
    { 0x66,0x71,0x24,0x5e,0xda,0x34,0x9b,0xc4,0x83,0xcc,0x20,0x85,0xb8,0xc1,0x8b,0xed,0x4d,0x81,0xa1,0x5d,0x8f,0x3d,0x24,0x5b,0x7f,0xaf,0x69,0x33,0x93,0x0a,0x00,0x81 }, // v4-staging (avatar storage, build variant 0)
    { 0xf3,0xbc,0x7b,0x57,0x22,0x0d,0xd4,0xad,0x80,0x02,0x11,0x0c,0x37,0xa1,0x45,0xbf,0x2c,0xc0,0xee,0xc1,0xd9,0x59,0x29,0xc9,0x1c,0x6c,0x2d,0xc8,0xfe,0x00,0x2d,0xaf }, // v5-staging (avatar in args, build variant 0)
    { 0xcf,0xb3,0xdf,0x50,0x68,0x8a,0x8e,0xa5,0x77,0xe1,0x6c,0x15,0x66,0x60,0xa1,0x52,0x61,0x83,0xa3,0xa3,0xbd,0x6d,0x45,0x56,0x2e,0xd1,0x06,0xff,0xac,0x31,0x44,0x3a }, // v6-staging (group chat, build variant 0)
    // v7: production group chat — SID added after build variant 1 compile
};

// Build variant: 0=staging, 1=production. Changes SID → unique CID per environment.
static const uint8_t s_BuildVariant = 0;

// ============================================================================
// Constants
// ============================================================================
static const uint64_t s_DefaultFee        = 100000000ULL;  // 1 BEAM to register
static const uint32_t s_MaxHandleLen      = 32;
static const uint32_t s_MinHandleLen      = 3;
static const uint32_t s_MaxDisplayNameLen = 64;
static const uint32_t s_MaxGroupNameLen   = 32;
static const uint32_t s_MaxGroupMembers   = 200;
static const uint32_t s_MaxGroupsPerUser  = 10;
static const uint32_t s_MaxGroupPins      = 50;

// ============================================================================
// Storage tags
// ============================================================================
struct Tags {
    static const uint8_t s_State  = 0;  // Contract state singleton
    static const uint8_t s_Handle = 1;  // @handle → Profile
    static const uint8_t s_Owner  = 2;  // BVM PubKey → OwnerRecord (reverse lookup)
    static const uint8_t s_Group  = 3;  // group_id → GroupInfo
    static const uint8_t s_Member = 4;  // group_id + handle → MemberInfo
    static const uint8_t s_JoinReq = 5; // group_id + handle → JoinRequest (pending)
    static const uint8_t s_Pin    = 6;  // group_id + pin_index → PinInfo
    static const uint8_t s_Report = 7;  // group_id + reporter + target → ReportInfo
    static const uint8_t s_GrpCnt = 8;  // handle → UserGroupCount
};

// ============================================================================
// Member roles & permissions
// ============================================================================
struct Role {
    static const uint8_t s_Member  = 0;
    static const uint8_t s_Admin   = 1;
    static const uint8_t s_Creator = 2;
    static const uint8_t s_Banned  = 3;
};

struct Perm {
    static const uint32_t s_SendMsg     = 1 << 0;  // Can send messages
    static const uint32_t s_SendMedia   = 1 << 1;  // Can send media/files
    static const uint32_t s_AddMembers  = 1 << 2;  // Can add members
    static const uint32_t s_PinMsg      = 1 << 3;  // Can pin messages
    static const uint32_t s_ChangeInfo  = 1 << 4;  // Can change group info
    static const uint32_t s_DeleteMsg   = 1 << 5;  // Can delete others' messages
    static const uint32_t s_All         = 0x3F;     // All permissions
    static const uint32_t s_Default     = s_SendMsg | s_SendMedia; // Default for new members
};

// ============================================================================
// Storage keys (packed for consistent layout)
// ============================================================================
#pragma pack(push, 1)

struct StateKey { uint8_t m_Tag = Tags::s_State; };

// Contract-wide state (singleton)
struct State {
    PubKey   m_OwnerPk;
    uint64_t m_RegistrationFee;
    uint64_t m_TotalRegistrations;
    uint64_t m_TotalFees;
    AssetID  m_AssetId;
    uint8_t  m_Paused;
    uint8_t  m_Initialized;
    uint8_t  m_BuildVariant;
};

// --- Identity storage keys (Tag 1-2) ---

struct HandleKey {
    uint8_t m_Tag;
    char    m_Handle[s_MaxHandleLen];
    HandleKey() : m_Tag(Tags::s_Handle) { Env::Memset(m_Handle, 0, sizeof(m_Handle)); }
};

struct OwnerKey {
    uint8_t m_Tag;
    PubKey  m_UserPk;
    OwnerKey() : m_Tag(Tags::s_Owner) {}
};

// --- Group storage keys (Tag 3-8) ---

struct GroupKey {
    uint8_t  m_Tag;
    uint8_t  m_GroupId[32];  // SHA256(creator_handle + height + nonce)
    GroupKey() : m_Tag(Tags::s_Group) { Env::Memset(m_GroupId, 0, sizeof(m_GroupId)); }
};

struct GroupMemberKey {
    uint8_t  m_Tag;
    uint8_t  m_GroupId[32];
    char     m_Handle[s_MaxHandleLen];
    GroupMemberKey() : m_Tag(Tags::s_Member) {
        Env::Memset(m_GroupId, 0, sizeof(m_GroupId));
        Env::Memset(m_Handle, 0, sizeof(m_Handle));
    }
};

struct GroupJoinRequestKey {
    uint8_t  m_Tag;
    uint8_t  m_GroupId[32];
    char     m_Handle[s_MaxHandleLen];
    GroupJoinRequestKey() : m_Tag(Tags::s_JoinReq) {
        Env::Memset(m_GroupId, 0, sizeof(m_GroupId));
        Env::Memset(m_Handle, 0, sizeof(m_Handle));
    }
};

struct GroupPinKey {
    uint8_t  m_Tag;
    uint8_t  m_GroupId[32];
    uint32_t m_PinIndex;
    GroupPinKey() : m_Tag(Tags::s_Pin) {
        Env::Memset(m_GroupId, 0, sizeof(m_GroupId));
        m_PinIndex = 0;
    }
};

struct ReportKey {
    uint8_t  m_Tag;
    uint8_t  m_GroupId[32];
    char     m_Reporter[s_MaxHandleLen];
    char     m_Target[s_MaxHandleLen];
    ReportKey() : m_Tag(Tags::s_Report) {
        Env::Memset(m_GroupId, 0, sizeof(m_GroupId));
        Env::Memset(m_Reporter, 0, sizeof(m_Reporter));
        Env::Memset(m_Target, 0, sizeof(m_Target));
    }
};

struct UserGroupCountKey {
    uint8_t  m_Tag;
    char     m_Handle[s_MaxHandleLen];
    UserGroupCountKey() : m_Tag(Tags::s_GrpCnt) { Env::Memset(m_Handle, 0, sizeof(m_Handle)); }
};

#pragma pack(pop)

// ============================================================================
// Value structs stored on-chain (packed)
// ============================================================================
#pragma pack(push, 1)

// Identity values
struct Profile {
    PubKey   m_UserPk;
    uint8_t  m_WalletIdRaw[34];
    uint64_t m_RegisteredHeight;
    char     m_DisplayName[s_MaxDisplayNameLen];
};

struct OwnerRecord {
    char m_Handle[s_MaxHandleLen];
};

// Group values
struct GroupInfo {
    char     m_Name[s_MaxGroupNameLen];
    uint8_t  m_DescriptionHash[32];
    char     m_CreatorHandle[s_MaxHandleLen];
    uint8_t  m_AvatarHash[32];        // Group icon hash (on-chain for groups)
    uint8_t  m_IsPublic;
    uint8_t  m_RequireApproval;
    uint32_t m_MaxMembers;
    uint32_t m_MemberCount;
    uint32_t m_DefaultPermissions;
    uint32_t m_CreatedHeight;
    uint8_t  m_InviteHash[32];        // SHA256(invite_secret) — NOT plaintext
    uint32_t m_InviteExpiryHeight;    // 0 = no expiry
    uint32_t m_PinCount;
};

struct MemberInfo {
    uint8_t  m_Role;          // Role::s_Member/Admin/Creator/Banned
    uint32_t m_Permissions;   // Perm bitmask
    uint32_t m_JoinedHeight;
};

struct JoinRequest {
    uint32_t m_RequestHeight;
};

struct PinInfo {
    char     m_SenderHandle[s_MaxHandleLen];
    uint8_t  m_MessageHash[32];
    uint32_t m_PinnedHeight;
};

struct ReportInfo {
    uint8_t  m_Reason;       // 0=spam, 1=harassment, 2=inappropriate, 3=other
    uint32_t m_ReportHeight;
};

struct UserGroupCount {
    uint32_t m_Count;
};

#pragma pack(pop)

// ============================================================================
// Method args
// ============================================================================
namespace Method {

struct Ctor {
    Upgradable3::Settings m_Upgradable;
    PubKey m_OwnerPk;
};

// NOTE: Method_2 reserved by Upgradable3 for upgrade dispatch

// --- Identity methods (3-9) ---

struct RegisterHandle {
    static const uint32_t s_iMethod = 3;
    PubKey  m_UserPk;
    uint8_t m_WalletIdRaw[34];
    char    m_Handle[s_MaxHandleLen];
    char    m_DisplayName[s_MaxDisplayNameLen];
    AssetID m_AssetId;
};

struct UpdateProfile {
    static const uint32_t s_iMethod = 4;
    PubKey  m_UserPk;
    uint8_t m_WalletIdRaw[34];
    char    m_DisplayName[s_MaxDisplayNameLen];
};

struct ReleaseHandle {
    static const uint32_t s_iMethod = 5;
    PubKey m_UserPk;
};

struct DepositDisabled { static const uint32_t s_iMethod = 6; };
struct Withdraw { static const uint32_t s_iMethod = 7; uint64_t m_Amount; };
struct SetOwner { static const uint32_t s_iMethod = 8; PubKey m_OwnerPk; };

struct SetConfig {
    static const uint32_t s_iMethod = 9;
    uint64_t m_RegistrationFee;
    uint8_t  m_Paused;
    AssetID  m_AssetId;
};

// --- Group methods (10-21) ---

struct CreateGroup {
    static const uint32_t s_iMethod = 10;
    PubKey   m_UserPk;
    char     m_Name[s_MaxGroupNameLen];
    uint8_t  m_IsPublic;
    uint8_t  m_RequireApproval;
    uint32_t m_MaxMembers;            // 0 = use s_MaxGroupMembers
    uint32_t m_DefaultPermissions;    // 0 = use Perm::s_Default
    uint32_t m_Nonce;                 // For unique group_id generation
};

struct JoinGroup {
    static const uint32_t s_iMethod = 11;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    uint8_t  m_InviteSecret[32];      // For private groups (all-zero = public join)
};

struct RemoveMember {
    static const uint32_t s_iMethod = 12;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_TargetHandle[s_MaxHandleLen];
    uint8_t  m_Ban;                   // 1 = ban (can't rejoin), 0 = just remove
};

struct SetMemberRole {
    static const uint32_t s_iMethod = 13;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_TargetHandle[s_MaxHandleLen];
    uint8_t  m_NewRole;               // Role::s_Admin or Role::s_Member only
    uint32_t m_Permissions;           // Custom permissions (0 = group default)
};

struct UpdateGroupInfo {
    static const uint32_t s_iMethod = 14;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_Name[s_MaxGroupNameLen];       // all-zero = no change
    uint8_t  m_DescriptionHash[32];           // all-zero = no change
    uint8_t  m_AvatarHash[32];                // all-zero = no change
    uint8_t  m_IsPublic;                      // 0xFF = no change
    uint8_t  m_RequireApproval;               // 0xFF = no change
    uint32_t m_DefaultPermissions;            // 0 = no change
};

struct LeaveGroup {
    static const uint32_t s_iMethod = 15;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
};

struct ApproveJoinRequest {
    static const uint32_t s_iMethod = 16;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_TargetHandle[s_MaxHandleLen];
    uint8_t  m_Approve;               // 1 = approve, 0 = reject
};

struct SetInviteLink {
    static const uint32_t s_iMethod = 17;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    uint8_t  m_InviteHash[32];        // SHA256(new_secret), all-zero = revoke
    uint32_t m_ExpiryHeight;          // 0 = no expiry
};

struct TransferOwnership {
    static const uint32_t s_iMethod = 18;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_NewCreatorHandle[s_MaxHandleLen];
};

struct SetGroupPin {
    static const uint32_t s_iMethod = 19;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_SenderHandle[s_MaxHandleLen];
    uint8_t  m_MessageHash[32];
    uint8_t  m_Unpin;                 // 1 = unpin (by message_hash), 0 = pin
};

struct ReportMember {
    static const uint32_t s_iMethod = 20;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_TargetHandle[s_MaxHandleLen];
    uint8_t  m_Reason;                // 0=spam, 1=harassment, 2=inappropriate, 3=other
};

struct DeleteGroup {
    static const uint32_t s_iMethod = 21;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
};

} // namespace Method
} // namespace PriviMe
