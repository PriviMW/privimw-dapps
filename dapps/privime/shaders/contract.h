// PriviMe Contract Header
// On-chain identity registry + group chat system.
// Uses Upgradable3 (Method_2 reserved for upgrade dispatch).
// Methods 3-9: identity. Methods 10-17: group chat (8 methods).
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
    { 0xcf,0xb3,0xdf,0x50,0x68,0x8a,0x8e,0xa5,0x77,0xe1,0x6c,0x15,0x66,0x60,0xa1,0x52,0x61,0x83,0xa3,0xa3,0xbd,0x6d,0x45,0x56,0x2e,0xd1,0x06,0xff,0xac,0x31,0x44,0x3a }, // v6-staging (group chat 13 methods, build variant 0)
    { 0x44,0x49,0x12,0xff,0x95,0x4f,0x64,0xac,0xad,0x5f,0x53,0xaf,0x4c,0x0c,0x7f,0x3d,0xdc,0x24,0x79,0xba,0xda,0x70,0xd4,0x52,0x9c,0x37,0x83,0x51,0xe5,0x55,0x42,0x13 }, // v7-staging (8 methods, build variant 0)
    { 0x91,0x4e,0x83,0xbe,0x1c,0x59,0x5a,0x97,0x38,0x23,0x4a,0x48,0xc4,0x09,0x1c,0x85,0xdc,0xe8,0xed,0x34,0xa0,0x95,0x33,0x43,0x94,0xb9,0x7e,0x78,0x8c,0xe9,0x7e,0x3d }, // v8-staging (open join, build variant 0)
    { 0x7f,0x6e,0xf9,0x43,0x33,0xc4,0x3a,0x4f,0xbb,0x08,0x1a,0x36,0x6b,0xfe,0xe4,0x22,0xff,0x01,0x59,0x37,0xa7,0x53,0x77,0x96,0xd3,0x29,0x40,0x31,0xfe,0x82,0x47,0x21 }, // v9-staging (join password, build variant 0)
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

// ============================================================================
// Storage tags
// ============================================================================
struct Tags {
    static const uint8_t s_State  = 0;  // Contract state singleton
    static const uint8_t s_Handle = 1;  // @handle → Profile
    static const uint8_t s_Owner  = 2;  // BVM PubKey → OwnerRecord (reverse lookup)
    static const uint8_t s_Group  = 3;  // group_id → GroupInfo
    static const uint8_t s_Member = 4;  // group_id + handle → MemberInfo
    static const uint8_t s_GrpCnt = 5;  // handle → UserGroupCount
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
    static const uint32_t s_SendMsg     = 1 << 0;
    static const uint32_t s_SendMedia   = 1 << 1;
    static const uint32_t s_AddMembers  = 1 << 2;
    static const uint32_t s_PinMsg      = 1 << 3;
    static const uint32_t s_ChangeInfo  = 1 << 4;
    static const uint32_t s_DeleteMsg   = 1 << 5;
    static const uint32_t s_All         = 0x3F;
    static const uint32_t s_Default     = s_SendMsg | s_SendMedia;
};

// ============================================================================
// Storage keys (packed for consistent layout)
// ============================================================================
#pragma pack(push, 1)

struct StateKey { uint8_t m_Tag = Tags::s_State; };

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

// --- Group storage keys (Tag 3-5) ---

struct GroupKey {
    uint8_t  m_Tag;
    uint8_t  m_GroupId[32];
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
    uint8_t  m_AvatarHash[32];
    uint8_t  m_IsPublic;
    uint8_t  m_RequireApproval;
    uint32_t m_MaxMembers;
    uint32_t m_MemberCount;
    uint32_t m_DefaultPermissions;
    uint32_t m_CreatedHeight;
    uint8_t  m_JoinPasswordHash[32]; // SHA256(password). All-zero = no password (public join).
};

struct MemberInfo {
    uint8_t  m_Role;
    uint32_t m_Permissions;
    uint32_t m_JoinedHeight;
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

// --- Group methods (10-17) ---

struct CreateGroup {
    static const uint32_t s_iMethod = 10;
    PubKey   m_UserPk;
    char     m_Name[s_MaxGroupNameLen];
    uint8_t  m_IsPublic;
    uint8_t  m_RequireApproval;
    uint32_t m_MaxMembers;
    uint32_t m_DefaultPermissions;
    uint32_t m_Nonce;
    uint8_t  m_JoinPassword[32]; // Raw password. Hashed and stored. All-zero = no password.
};

struct JoinGroup {
    static const uint32_t s_iMethod = 11;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    uint8_t  m_JoinPassword[32]; // Raw password. All-zero = public join (no password).
};

struct RemoveMember {
    static const uint32_t s_iMethod = 12;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_TargetHandle[s_MaxHandleLen];
    uint8_t  m_Ban;
};

struct SetMemberRole {
    static const uint32_t s_iMethod = 13;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_TargetHandle[s_MaxHandleLen];
    uint8_t  m_NewRole;
    uint32_t m_Permissions;
};

struct UpdateGroupInfo {
    static const uint32_t s_iMethod = 14;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_Name[s_MaxGroupNameLen];
    uint8_t  m_DescriptionHash[32];
    uint8_t  m_AvatarHash[32];
    uint8_t  m_IsPublic;
    uint8_t  m_RequireApproval;
    uint32_t m_DefaultPermissions;
};

struct LeaveGroup {
    static const uint32_t s_iMethod = 15;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
};

struct TransferOwnership {
    static const uint32_t s_iMethod = 16;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
    char     m_NewCreatorHandle[s_MaxHandleLen];
};

struct DeleteGroup {
    static const uint32_t s_iMethod = 17;
    PubKey   m_UserPk;
    uint8_t  m_GroupId[32];
};

} // namespace Method
} // namespace PriviMe
