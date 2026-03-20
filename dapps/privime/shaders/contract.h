// PriviMe Contract Header
// On-chain identity registry: @handle <-> SBBS WalletID mapping.
// Uses Upgradable3 (Method_2 reserved for upgrade dispatch).
// Phase 1a: DM + identity only. Phase 1b: group methods (Method_10+) via upgrade.
#pragma once
#include "upgradable3/contract.h"

namespace PriviMe {

// ShaderID array for Upgradable3 version verification.
// Grows with each upgrade: s_pSID[0]=v0, s_pSID[1]=v1, etc.
// Update after each recompile of contract.wasm (generate-sid.exe).
static const ShaderID s_pSID[] = {
    { 0xad,0x36,0xf9,0x41,0x19,0xcd,0x40,0x45,0xaf,0x3a,0x93,0x0a,0xe1,0x73,0xc9,0x51,0x41,0x52,0x56,0x3f,0x3b,0x75,0xd0,0x15,0x21,0x00,0x2c,0x15,0x14,0x6c,0x2b,0x58 }, // v0 — unpacked Profile
    { 0x22,0x34,0x94,0xa8,0x99,0xe0,0x07,0x3a,0xca,0x5c,0x57,0x87,0xa0,0x8f,0x35,0xa8,0x71,0x55,0xfc,0x6c,0x3e,0x8a,0xd3,0x48,0x15,0x07,0x00,0x79,0x08,0xc0,0xd2,0xc0 }, // v1 — packed Profile/OwnerRecord
    { 0x91,0xef,0xa4,0xa2,0xd1,0xaf,0x89,0x35,0x26,0xcb,0x92,0x68,0x3a,0x9b,0x32,0xb6,0x08,0x96,0xc7,0xb1,0x85,0xe9,0x47,0x40,0x41,0x0b,0xaa,0x5e,0xad,0xc8,0xb7,0xc4 }, // v2-staging (build variant 0)
    { 0xf2,0x8a,0x6a,0x08,0x68,0x07,0x4c,0xbd,0x7f,0x18,0x1f,0x9e,0xea,0x93,0x4d,0x8e,0x14,0x4b,0xcc,0xa6,0x4f,0xbc,0xc1,0xf8,0xcd,0xc9,0x79,0x47,0xeb,0xe6,0x86,0xfd }, // v2-production (build variant 1)
    { 0x7d,0x9e,0xc8,0x20,0xa4,0x83,0xd7,0x5e,0x1a,0x2b,0x49,0xf0,0x96,0x16,0xc8,0x0f,0x03,0x63,0x0d,0xcd,0xff,0xbc,0x84,0xc5,0x17,0xf9,0xd2,0x4b,0xa1,0x7d,0xcc,0x8a }, // v3-staging (upgrade test, build variant 0)
    { 0x66,0x71,0x24,0x5e,0xda,0x34,0x9b,0xc4,0x83,0xcc,0x20,0x85,0xb8,0xc1,0x8b,0xed,0x4d,0x81,0xa1,0x5d,0x8f,0x3d,0x24,0x5b,0x7f,0xaf,0x69,0x33,0x93,0x0a,0x00,0x81 }, // v4-staging (avatar storage, build variant 0)
    { 0xf3,0xbc,0x7b,0x57,0x22,0x0d,0xd4,0xad,0x80,0x02,0x11,0x0c,0x37,0xa1,0x45,0xbf,0x2c,0xc0,0xee,0xc1,0xd9,0x59,0x29,0xc9,0x1c,0x6c,0x2d,0xc8,0xfe,0x00,0x2d,0xaf }  // v5-staging (avatar in register+update args, build variant 0)
};

// Build variant: 0=staging, 1=production. Changes SID → unique CID per environment.
// To build production: set to 1, recompile contract.wasm, deploy fresh.
static const uint8_t s_BuildVariant = 0;

// Constants
static const uint64_t s_DefaultFee      = 100000000ULL;  // 1 BEAM to register
static const uint32_t s_MaxHandleLen    = 32;
static const uint32_t s_MinHandleLen    = 3;
static const uint32_t s_MaxDisplayNameLen = 64;

struct Tags { static const uint8_t s_State = 0, s_Handle = 1, s_Owner = 2, s_Avatar = 3; };

#pragma pack(push, 1)

struct StateKey { uint8_t m_Tag = Tags::s_State; };

// Contract-wide state (singleton)
struct State {
    PubKey   m_OwnerPk;            // Fee withdrawal owner
    uint64_t m_RegistrationFee;    // Groth to register
    uint64_t m_TotalRegistrations; // Active registered handles
    uint64_t m_TotalFees;          // Current fee balance (collected - withdrawn)
    AssetID  m_AssetId;            // 0 = BEAM
    uint8_t  m_Paused;
    uint8_t  m_Initialized;
    uint8_t  m_BuildVariant;    // 0=staging, 1=production (set once in Ctor)
};

// Storage key: forward lookup @handle -> Profile
struct HandleKey {
    uint8_t m_Tag;                  // Tags::s_Handle = 1
    char    m_Handle[s_MaxHandleLen]; // lowercase, null-padded to fixed width
    HandleKey() : m_Tag(Tags::s_Handle) { Env::Memset(m_Handle, 0, sizeof(m_Handle)); }
};

// Storage key: reverse lookup BVM PubKey -> OwnerRecord
struct OwnerKey {
    uint8_t m_Tag;    // Tags::s_Owner = 2
    PubKey  m_UserPk;
    OwnerKey() : m_Tag(Tags::s_Owner) {}
};

// Storage key: avatar lookup @handle -> AvatarHash
struct AvatarKey {
    uint8_t m_Tag;                  // Tags::s_Avatar = 3
    char    m_Handle[s_MaxHandleLen]; // same handle as HandleKey
    AvatarKey() : m_Tag(Tags::s_Avatar) { Env::Memset(m_Handle, 0, sizeof(m_Handle)); }
};

#pragma pack(pop)

// Value structs stored on-chain — packed for consistent layout across compilers.
#pragma pack(push, 1)

// Value stored at HandleKey (forward lookup)
struct Profile {
    PubKey   m_UserPk;
    uint8_t  m_WalletIdRaw[34];              // SBBS WalletID: 2B channel + 32B pubkey
    uint64_t m_RegisteredHeight;
    char     m_DisplayName[s_MaxDisplayNameLen]; // optional, may be all-zero
};

// Value stored at OwnerRecord (reverse lookup)
struct OwnerRecord {
    char m_Handle[s_MaxHandleLen]; // null-padded
};

// Value stored at AvatarKey — SHA-256 hash of avatar image (32 bytes)
struct AvatarData {
    uint8_t m_Hash[32]; // SHA-256 of compressed avatar image
};

#pragma pack(pop)

namespace Method {

struct Ctor {
    Upgradable3::Settings m_Upgradable; // admin key, min approvers, upgrade delay
    PubKey m_OwnerPk;                   // fee withdrawal owner (can be same as admin)
};

// NOTE: Method_2 is reserved by Upgradable3 for upgrade dispatch

struct RegisterHandle {
    static const uint32_t s_iMethod = 3;
    PubKey  m_UserPk;
    uint8_t m_WalletIdRaw[34];
    char    m_Handle[s_MaxHandleLen];
    char    m_DisplayName[s_MaxDisplayNameLen];
    AssetID m_AssetId;
    uint8_t m_AvatarHash[32];  // optional: all-zero = no avatar
};

struct UpdateProfile {
    static const uint32_t s_iMethod = 4;
    PubKey  m_UserPk;
    uint8_t m_WalletIdRaw[34];
    char    m_DisplayName[s_MaxDisplayNameLen];
    uint8_t m_AvatarHash[32];  // optional: all-zero = no change
};

struct ReleaseHandle {
    static const uint32_t s_iMethod = 5;
    PubKey m_UserPk;
};

struct DepositDisabled { static const uint32_t s_iMethod = 6; }; // stub — disabled, kept for method numbering
struct Withdraw { static const uint32_t s_iMethod = 7; uint64_t m_Amount; };
struct SetOwner { static const uint32_t s_iMethod = 8; PubKey m_OwnerPk; };

struct SetConfig {
    static const uint32_t s_iMethod = 9;
    uint64_t m_RegistrationFee; // 0 = keep current
    uint8_t  m_Paused;
    AssetID  m_AssetId;
};

// Phase 1b: avatar support (v3 upgrade)
struct SetAvatar {
    static const uint32_t s_iMethod = 10;
    PubKey  m_UserPk;
    uint8_t m_Hash[32];   // SHA-256 of avatar image (all-zero = clear avatar)
};

// Phase 1c: group methods (Method_11+) will be added via future upgrade

} // namespace Method
} // namespace PriviMe
