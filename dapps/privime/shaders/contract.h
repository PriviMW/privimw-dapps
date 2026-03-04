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
    { 0x22,0x34,0x94,0xa8,0x99,0xe0,0x07,0x3a,0xca,0x5c,0x57,0x87,0xa0,0x8f,0x35,0xa8,0x71,0x55,0xfc,0x6c,0x3e,0x8a,0xd3,0x48,0x15,0x07,0x00,0x79,0x08,0xc0,0xd2,0xc0 }  // v1 — packed Profile/OwnerRecord
};

// Constants
static const uint64_t s_DefaultFee      = 100000000ULL;  // 1 BEAM to register
static const uint32_t s_MaxHandleLen    = 32;
static const uint32_t s_MinHandleLen    = 3;
static const uint32_t s_MaxDisplayNameLen = 64;

struct Tags { static const uint8_t s_State = 0, s_Handle = 1, s_Owner = 2; };

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

struct DepositDisabled { static const uint32_t s_iMethod = 6; }; // stub — disabled, kept for method numbering
struct Withdraw { static const uint32_t s_iMethod = 7; uint64_t m_Amount; };
struct SetOwner { static const uint32_t s_iMethod = 8; PubKey m_OwnerPk; };

struct SetConfig {
    static const uint32_t s_iMethod = 9;
    uint64_t m_RegistrationFee; // 0 = keep current
    uint8_t  m_Paused;
    AssetID  m_AssetId;
};

// Phase 1b group methods (Method_10-13) will be added via Upgradable3 upgrade

} // namespace Method
} // namespace PriviMe
