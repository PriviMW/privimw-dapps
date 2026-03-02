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
    { 0xfd,0xde,0x22,0x25,0x4a,0x24,0xb2,0x4f,0x7b,0x16,0x89,0xcf,0x57,0x87,0x64,0x80,0x07,0xf3,0xf8,0x9a,0x13,0x04,0xe7,0xf4,0x0e,0xde,0x49,0x76,0xd5,0x1e,0x60,0xdd } // v0
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

// Value stored at HandleKey (forward lookup)
struct Profile {
    PubKey   m_UserPk;
    uint8_t  m_WalletIdRaw[34];              // SBBS WalletID: 2B channel + 32B pubkey
    uint64_t m_RegisteredHeight;
    char     m_DisplayName[s_MaxDisplayNameLen]; // optional, may be all-zero
};

// Value stored at OwnerKey (reverse lookup)
struct OwnerRecord {
    char m_Handle[s_MaxHandleLen]; // null-padded
};

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

struct Deposit  { static const uint32_t s_iMethod = 6; uint64_t m_Amount; };
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
