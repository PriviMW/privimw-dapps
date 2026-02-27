# Beam Shader Development Guide

Complete reference for building **any kind** of Beam blockchain shaders and DApps. This guide covers the full development workflow, API patterns, and best practices for creating confidential, privacy-focused decentralized applications on Beam.

---

## Quick Navigation

- **New to Beam Shaders?** Start with [Shader Architecture](#shader-architecture)
- **Building a Shader?** See [Contract Shader Development](#contract-shader-development) and [App Shader Development](#app-shader-development)
- **Deploying?** Jump to [Testing & Deployment](#testing--deployment)
- **Stuck?** Check [Common Pitfalls](#common-pitfalls)

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Shader Architecture](#shader-architecture)
3. [Project Structure](#project-structure)
4. [Contract Shader Development](#contract-shader-development)
5. [App Shader Development](#app-shader-development)
6. [Key Beam SDK APIs](#key-beam-sdk-apis)
7. [Owner Authentication Pattern](#owner-authentication-pattern)
8. [Cost/Charge Calculation](#costcharge-calculation)
9. [Building & Compilation](#building--compilation)
10. [Testing & Deployment](#testing--deployment)
11. [Common Pitfalls](#common-pitfalls)
12. [Complete Example](#complete-example)

---

## Prerequisites

### Required Software

| Tool | Path/Installation |
|------|-------------------|
| WASI SDK | `C:\beam-shader-sdk\shader-sdk\wasi-sdk-14.0\` |
| Beam Shader SDK | `C:\beam-shader-sdk\shader-sdk\` |
| Beam Wallet CLI | For testing shaders |

### Environment Variables

```bash
export WASI_SDK_PREFIX="C:/beam-shader-sdk/shader-sdk/wasi-sdk-14.0"
```

### Required Files to Copy

Copy these from Beam SDK to your shader folder:

```bash
cp "C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders/common.h" ./your-shader/
cp "C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders/app_common_impl.h" ./your-shader/
cp "C:/beam-shader-sdk/shader-sdk/beam/bvm/bvm2_cost.h" ./your-shader/
```

---

## Shader Architecture

Beam uses **two separate shaders** that work together:

```
┌─────────────────────────────────────────────────────────────┐
│                      APP SHADER (app.wasm)                   │
│                                                              │
│  - Runs in WALLET (client-side)                              │
│  - Shows schema to user (Method_0)                           │
│  - Creates transactions (Method_1 dispatcher)                │
│  - Derives keys for signing                                  │
│  - Reads contract state for display                          │
│  - NEVER holds funds or secrets permanently                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ Generates transactions
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   CONTRACT SHADER (contract.wasm)            │
│                                                              │
│  - Deployed ON-CHAIN                                         │
│  - Holds funds (locked in contract)                          │
│  - Enforces business logic                                   │
│  - Verifies signatures (AddSig)                              │
│  - Manages state (LoadVar/SaveVar)                           │
│  - Entry points: Ctor, Dtor, Method_2, Method_3, ...        │
└─────────────────────────────────────────────────────────────┘
```

### Key Distinction

| Aspect | App Shader | Contract Shader |
|--------|------------|-----------------|
| Where it runs | In wallet | On blockchain |
| Who calls it | User via wallet CLI | Blockchain during tx |
| State access | Read-only | Read/Write |
| Fund handling | Creates tx with FundsChange | Actually locks/unlocks |
| Signature | Derives keys, passes to kernel | Verifies with AddSig |

---

## Project Structure

```
shaders/
└── your-contract/
    ├── contract.h        # Shared structures, constants, ShaderID
    ├── contract.cpp      # Contract shader implementation
    ├── app.cpp           # App shader implementation
    ├── common.h          # Beam SDK types and Env:: API
    ├── app_common_impl.h # App shader utilities
    └── bvm2_cost.h       # Cost constants for charges
```

---

## Contract Shader Development

### Header File (contract.h)

```cpp
#pragma once

namespace YourContract {

// CRITICAL: ShaderID (generated after first compile)
// Run: generate-sid.exe contract.wasm
// Then paste the result here:
static const ShaderID s_SID = {0x??, 0x??, ...};

// Constants
static const uint64_t s_DefaultValue = 1000000ULL;

// State structures
struct State {
    PubKey m_OwnerPk;
    uint64_t m_SomeValue;
    AssetID m_AssetId;
    uint8_t m_Paused;
    uint8_t m_Initialized;
};

// Method parameter structures
namespace Method {

struct SomeAction {
    static const uint32_t s_iMethod = 2;  // Method number
    uint64_t m_Param1;
    AssetID m_AssetId;
};

} // namespace Method
} // namespace YourContract
```

### Implementation File (contract.cpp)

```cpp
#include "common.h"
#include "contract.h"

namespace YourContract {

// Storage tags
struct Tags {
    static const uint8_t s_State = 0;
    static const uint8_t s_Data = 1;
};

// Key structures
struct StateKey {
    uint8_t m_Tag = Tags::s_State;
};

// Global state cache
static State g_State;

State& GetState() {
    if (!g_State.m_Initialized) {
        StateKey k;
        if (!Env::LoadVar_T(k, g_State)) {
            Env::Halt();  // Contract not initialized
        }
        g_State.m_Initialized = 1;
    }
    return g_State;
}

void SaveState() {
    StateKey k;
    Env::SaveVar_T(k, g_State);
}

} // namespace YourContract

// ============================================================================
// Constructor - Called once when contract is created
// ============================================================================
BEAM_EXPORT void Ctor(const YourContract::Params& r)
{
    _POD_(YourContract::g_State).SetZero();

    // Set owner from params (passed by app shader)
    _POD_(YourContract::g_State.m_OwnerPk) = r.m_OwnerPk;

    YourContract::g_State.m_Initialized = 1;
    YourContract::SaveState();
}

// ============================================================================
// Destructor - Called when contract is destroyed
// ============================================================================
BEAM_EXPORT void Dtor(void*)
{
    // Cleanup if needed
}

// ============================================================================
// Methods (numbered starting from 2)
// ============================================================================
BEAM_EXPORT void Method_2(const YourContract::Method::SomeAction& r)
{
    YourContract::State& s = YourContract::GetState();

    // Validation
    if (s.m_Paused) Env::Halt();

    // Business logic
    // ...

    // Save state
    YourContract::SaveState();

    // Require owner signature (for protected actions)
    Env::AddSig(s.m_OwnerPk);
}
```

### Critical Contract Rules

1. **Never use `Env::DerivePk` in contract shader** - It's for app shaders only
2. **Always use `Env::AddSig(pubkey)`** for owner verification
3. **Use `Env::Halt()` to reject invalid operations**
4. **State is cached in global variable** - Load once, save when modified
5. **Method numbering starts at 2** (0 and 1 reserved for app shader)

---

## App Shader Development

### Schema (Method_0)

Shows the API to users/wallets:

```cpp
BEAM_EXPORT void Method_0()
{
    Env::DocGroup root("");
    {
        Env::DocGroup gr("roles");
        {
            Env::DocGroup grRole("manager");
            {
                Env::DocGroup grMethod("protected_action");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("amount", "uint64");
            }
        }
        {
            Env::DocGroup grRole("user");
            {
                Env::DocGroup grMethod("public_action");
                Env::DocAddText("cid", "ContractID");
            }
        }
    }
}
```

### Dispatcher (Method_1)

```cpp
BEAM_EXPORT void Method_1()
{
    Env::DocGroup root("");

    char szRole[16], szAction[32];

    if (!Env::DocGetText("role", szRole, sizeof(szRole)))
        return OnError("Role not specified");

    if (!Env::DocGetText("action", szAction, sizeof(szAction)))
        return OnError("Action not specified");

    ContractID cid;
    Env::DocGet("cid", cid);

    // Dispatch based on role/action
    if (!Env::Strcmp(szRole, "manager")) {
        if (!Env::Strcmp(szAction, "protected_action"))
            return On_protected_action(cid);
        // ...
    }

    if (!Env::Strcmp(szRole, "user")) {
        if (!Env::Strcmp(szAction, "public_action"))
            return On_public_action(cid);
        // ...
    }

    OnError("Invalid role or action");
}
```

### Key Derivation for Owner

```cpp
// Owner key structure (deterministic per wallet)
struct OwnerKey {
    ShaderID m_SID;
    uint8_t m_pSeed[16];  // "your-contract-owner"

    OwnerKey() {
        _POD_(m_SID) = YourContract::s_SID;
        const char szSeed[] = "your-contract-owner";
        Env::Memcpy(m_pSeed, szSeed, sizeof(m_pSeed));
    }

    void DerivePk(PubKey& pk) const {
        Env::DerivePk(pk, this, sizeof(*this));
    }
};

// Usage in action handler
void On_protected_action(const ContractID& cid)
{
    // Get parameters
    YourContract::Method::SomeAction args;
    Env::DocGet("amount", args.m_Param1);

    // Derive owner key
    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    // Calculate charge (see Cost section)
    uint32_t nCharge = /* ... */;

    // Generate transaction
    Env::GenerateKernel(
        &cid,                                    // Contract ID
        YourContract::Method::SomeAction::s_iMethod,  // Method number
        &args, sizeof(args),                     // Method arguments
        nullptr, 0,                              // Funds changes (if any)
        &kid, 1,                                 // Signatures (owner key)
        "YourContract: action description",      // Comment
        nCharge                                  // BVM charge
    );
}
```

---

## Key Beam SDK APIs

### Storage

```cpp
// Save variable
Env::SaveVar_T(key, value);

// Load variable
if (!Env::LoadVar_T(key, value)) {
    // Not found
}

// Delete variable
Env::DelVar_T(key);
```

### Funds

```cpp
// Lock funds INTO contract (user sends to contract)
Env::FundsLock(assetId, amount);

// Unlock funds FROM contract (contract pays out)
Env::FundsUnlock(assetId, amount);
```

### Signatures

```cpp
// Require signature from specific public key
Env::AddSig(pubKey);

// Derive public key from seed (for deterministic keys)
Env::DerivePk(pubKey, seedData, seedSize);
```

### Blockchain Info

```cpp
Height h = Env::get_Height();  // Current block height
```

### Transaction Generation

```cpp
Env::GenerateKernel(
    &cid,              // Contract ID (nullptr for contract creation)
    methodNum,         // Method number (0 for constructor)
    &args, sizeof(args),  // Method arguments
    &fundsChanges, numChanges,  // Funds changes array
    &keyIds, numKeys,  // Key IDs for signing
    "comment",         // Transaction comment
    charge             // BVM charge (gas)
);
```

---

## Owner Authentication Pattern

### How It Works

```
┌─────────────────────────────────────────────────────────────┐
│  WALLET A (creator)                                         │
│                                                             │
│  OwnerKey = ShaderID + "seed"                               │
│  DerivePk(OwnerKey) = PubKey_A  (using wallet secret)       │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ Ctor stores PubKey_A as owner
                              ▼
        ┌─────────────────────────────────────────┐
        │  CONTRACT STATE                         │
        │  m_OwnerPk = PubKey_A                   │
        └─────────────────────────────────────────┘
                              │
        ┌─────────────────────┴───────────────────┐
        ▼                                         ▼
┌───────────────┐                      ┌───────────────┐
│ WALLET A      │                      │ WALLET B      │
│ DerivePk = A  │                      │ DerivePk = B  │
│ AddSig(A) ✓   │                      │ AddSig(B) ✗   │
└───────────────┘                      └───────────────┘
```

### Security Properties

1. **Wallet Secret Required** - DerivePk uses wallet's internal secret
2. **Deterministic** - Same wallet + same seed = same key
3. **Cryptographic** - Based on elliptic curve cryptography
4. **No Password Needed** - Wallet already protects with seed phrase

### Implementation

**App Shader:**
```cpp
struct OwnerKey {
    ShaderID m_SID;
    uint8_t m_pSeed[16];
    // ...
};

void On_manager_action(const ContractID& cid) {
    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));
    // ...
    Env::GenerateKernel(&cid, methodNum, &args, sizeof(args),
                        nullptr, 0, &kid, 1, "comment", nCharge);
}
```

**Contract Shader:**
```cpp
BEAM_EXPORT void Method_X(const Args& r) {
    State& s = GetState();
    // ... business logic ...
    Env::AddSig(s.m_OwnerPk);  // Require owner signature
}
```

---

## Cost/Charge Calculation

Every `GenerateKernel` must include sufficient charge for BVM operations.

### Cost Components

```cpp
uint32_t nCharge =
    // Variable operations
    Env::Cost::LoadVar_For(sizeof(State)) +      // Loading state
    Env::Cost::SaveVar_For(sizeof(State)) +      // Saving state
    Env::Cost::LoadVar_For(sizeof(Bet)) +        // Loading other vars

    // Fund operations
    Env::Cost::FundsLock +                        // Locking funds
    // Note: No FundsUnlock constant, use FundsLock

    // Signature verification
    Env::Cost::AddSig +                           // One signature

    // Computation overhead
    (Env::Cost::Cycle * 500);                    // CPU cycles (be generous)
```

### Common Mistake

```cpp
// WRONG - Will fail with "Low fee" error
Env::GenerateKernel(&cid, method, &args, sizeof(args), nullptr, 0, &kid, 1, "comment", 0);

// CORRECT - Include proper charge
uint32_t nCharge = /* calculate as above */;
Env::GenerateKernel(&cid, method, &args, sizeof(args), nullptr, 0, &kid, 1, "comment", nCharge);
```

### If You Get "Low fee" Error

1. Check the error for minimum required fee
2. Increase `Cycle` multiplier (e.g., from 200 to 500 to 800)
3. Add more `LoadVar_For` / `SaveVar_For` calls
4. When in doubt, overestimate - excess is refunded

---

## Building & Compilation

### Compile Contract Shader

```bash
cd your-shader-folder

"$WASI_SDK_PREFIX/bin/clang++.exe" \
    -O3 \
    -std=c++17 \
    -fno-rtti \
    -fno-exceptions \
    -nostartfiles \
    -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -isystem "$WASI_SDK_PREFIX/share/wasi-sysroot" \
    -I. \
    -I"C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders" \
    -Wl,--export=Ctor \
    -Wl,--export=Dtor \
    -Wl,--export=Method_2 \
    -Wl,--export=Method_3 \
    -o contract.wasm \
    contract.cpp
```

### Generate ShaderID

```bash
"C:/beam-shader-sdk/shader-sdk/bin/generate-sid.exe" contract.wasm
```

**Output:**
```cpp
// SID: abc123...
static const ShaderID s_SID = {0xab, 0xc1, 0x23, ...};
```

**Important:** Copy this to `contract.h`, then recompile both shaders.

### Compile App Shader

```bash
"$WASI_SDK_PREFIX/bin/clang++.exe" \
    -O3 \
    -std=c++17 \
    -fno-rtti \
    -fno-exceptions \
    -nostartfiles \
    -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -isystem "$WASI_SDK_PREFIX/share/wasi-sysroot" \
    -I. \
    -I"C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders" \
    -o app.wasm \
    app.cpp
```

---

## Testing & Deployment

### Test App Shader Schema

```bash
cd beam-wallet-cli-folder

./beam-wallet.exe shader \
    --shader_app_file path/to/app.wasm
```

### Deploy New Contract

```bash
./beam-wallet.exe shader \
    --shader_app_file path/to/app.wasm \
    --shader_contract_file path/to/contract.wasm \
    --shader_args "role=manager,action=create_contract"
```

### View Deployed Contracts

```bash
./beam-wallet.exe shader \
    --shader_app_file path/to/app.wasm \
    --shader_args "role=manager,action=view_contracts"
```

### Call Contract Method

```bash
# User action (no signature needed)
./beam-wallet.exe shader \
    --shader_app_file path/to/app.wasm \
    --shader_args "role=user,action=public_action,cid=CONTRACT_ID,param=value"

# Manager action (requires owner signature)
./beam-wallet.exe shader \
    --shader_app_file path/to/app.wasm \
    --shader_args "role=manager,action=protected_action,cid=CONTRACT_ID,param=value"
```

---

## Common Pitfalls

### 1. Using DerivePk in Contract Shader

```cpp
// WRONG - DerivePk is for APP shader only
BEAM_EXPORT void Method_X(const Args& r) {
    PubKey callerPk;
    Env::DerivePk(callerPk, nullptr, 0);  // ERROR!
}

// CORRECT - Use AddSig for verification
BEAM_EXPORT void Method_X(const Args& r) {
    State& s = GetState();
    // ... logic ...
    Env::AddSig(s.m_OwnerPk);  // Correct!
}
```

### 2. Zero Charge in GenerateKernel

```cpp
// WRONG - Will fail
Env::GenerateKernel(&cid, method, &args, sizeof(args), nullptr, 0, &kid, 1, "comment", 0);

// CORRECT - Calculate charge
uint32_t nCharge = /* ... */;
Env::GenerateKernel(&cid, method, &args, sizeof(args), nullptr, 0, &kid, 1, "comment", nCharge);
```

### 3. Wrong Type for DocAddNum

```cpp
// WRONG - Ambiguous with uint8_t
Env::DocAddNum("status", b.m_Status);  // m_Status is uint8_t

// CORRECT - Cast to uint32_t
Env::DocAddNum("status", (uint32_t)b.m_Status);
```

### 4. Missing ShaderID in Header

```cpp
// WRONG - No ShaderID or outdated
// static const ShaderID s_SID = {...};  // Missing!

// CORRECT - Generate and paste
static const ShaderID s_SID = {0x73, 0x67, ...};  // From generate-sid.exe
```

### 5. Not Waiting for Reveal Epoch

```cpp
// In contract - enforce timing
if (Env::get_Height() < b.m_CreatedHeight + s.m_RevealEpoch)
    Env::Halt();
```

### 6. Forgetting to Save State

```cpp
// WRONG - State not persisted
s.m_SomeValue = newValue;
// Missing SaveState()!

// CORRECT
s.m_SomeValue = newValue;
SaveState();
```

### 7. Using STL in Shaders

```cpp
// WRONG - STL not available with -nostdlib
#include <vector>
std::vector<int> v;  // ERROR!

// CORRECT - Use C-style arrays or custom structures
int arr[100];
```

---

## Complete Example

### contract.h

```cpp
#pragma once

namespace MyContract {

static const ShaderID s_SID = {/* from generate-sid.exe */};

struct Params {
    PubKey m_OwnerPk;
};

struct State {
    PubKey m_OwnerPk;
    uint64_t m_Counter;
    uint8_t m_Initialized;
};

namespace Method {

struct Increment {
    static const uint32_t s_iMethod = 2;
    uint64_t m_Amount;
};

struct SetOwner {
    static const uint32_t s_iMethod = 3;
    PubKey m_OwnerPk;
};

} // namespace Method
} // namespace MyContract
```

### contract.cpp

```cpp
#include "common.h"
#include "contract.h"

namespace MyContract {

struct StateKey { uint8_t m_Tag = 0; };
static State g_State;

State& GetState() {
    if (!g_State.m_Initialized) {
        StateKey k;
        if (!Env::LoadVar_T(k, g_State)) Env::Halt();
        g_State.m_Initialized = 1;
    }
    return g_State;
}

void SaveState() {
    StateKey k;
    Env::SaveVar_T(k, g_State);
}

} // namespace MyContract

BEAM_EXPORT void Ctor(const MyContract::Params& r) {
    _POD_(MyContract::g_State).SetZero();
    _POD_(MyContract::g_State.m_OwnerPk) = r.m_OwnerPk;
    MyContract::g_State.m_Initialized = 1;
    MyContract::SaveState();
}

BEAM_EXPORT void Dtor(void*) {}

BEAM_EXPORT void Method_2(const MyContract::Method::Increment& r) {
    MyContract::State& s = MyContract::GetState();
    s.m_Counter += r.m_Amount;
    MyContract::SaveState();
}

BEAM_EXPORT void Method_3(const MyContract::Method::SetOwner& r) {
    MyContract::State& s = MyContract::GetState();
    PubKey currentOwner = s.m_OwnerPk;
    _POD_(s.m_OwnerPk) = r.m_OwnerPk;
    MyContract::SaveState();
    Env::AddSig(currentOwner);
}
```

### app.cpp

```cpp
#include "common.h"
#include "app_common_impl.h"
#include "contract.h"

void OnError(const char* sz) {
    Env::DocGroup root("");
    Env::DocAddText("error", sz);
}

struct OwnerKey {
    ShaderID m_SID;
    uint8_t m_pSeed[16];
    OwnerKey() {
        _POD_(m_SID) = MyContract::s_SID;
        const char sz[] = "mycontract-owner";
        Env::Memcpy(m_pSeed, sz, sizeof(m_pSeed));
    }
};

BEAM_EXPORT void Method_0() {
    Env::DocGroup root("");
    { Env::DocGroup gr("roles");
        { Env::DocGroup grRole("manager");
            { Env::DocGroup gr("create_contract"); }
            { Env::DocGroup gr("set_owner");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("owner_pk", "PubKey");
            }
        }
        { Env::DocGroup grRole("user");
            { Env::DocGroup gr("increment");
                Env::DocAddText("cid", "ContractID");
                Env::DocAddText("amount", "uint64");
            }
        }
    }
}

void On_create_contract(const ContractID& cid) {
    MyContract::Params params;
    OwnerKey ok;
    ok.DerivePk(params.m_OwnerPk);
    Env::GenerateKernel(nullptr, 0, &params, sizeof(params),
                        nullptr, 0, nullptr, 0, "create contract", 0);
}

void On_increment(const ContractID& cid) {
    MyContract::Method::Increment args;
    Env::DocGet("amount", args.m_Amount);

    uint32_t nCharge = Env::Cost::LoadVar_For(sizeof(MyContract::State)) +
                       Env::Cost::SaveVar_For(sizeof(MyContract::State)) +
                       (Env::Cost::Cycle * 200);

    Env::GenerateKernel(&cid, MyContract::Method::Increment::s_iMethod,
                        &args, sizeof(args), nullptr, 0, nullptr, 0,
                        "increment", nCharge);
}

void On_set_owner(const ContractID& cid) {
    MyContract::Method::SetOwner args;
    Env::DocGet("owner_pk", args.m_OwnerPk);

    OwnerKey ok;
    Env::KeyID kid(&ok, sizeof(ok));

    uint32_t nCharge = Env::Cost::LoadVar_For(sizeof(MyContract::State)) +
                       Env::Cost::SaveVar_For(sizeof(MyContract::State)) +
                       Env::Cost::AddSig +
                       (Env::Cost::Cycle * 200);

    Env::GenerateKernel(&cid, MyContract::Method::SetOwner::s_iMethod,
                        &args, sizeof(args), nullptr, 0, &kid, 1,
                        "set owner", nCharge);
}

BEAM_EXPORT void Method_1() {
    Env::DocGroup root("");
    char szRole[16], szAction[32];

    if (!Env::DocGetText("role", szRole, sizeof(szRole)))
        return OnError("Role not specified");
    if (!Env::DocGetText("action", szAction, sizeof(szAction)))
        return OnError("Action not specified");

    ContractID cid;
    Env::DocGet("cid", cid);

    if (!Env::Strcmp(szRole, "manager")) {
        if (!Env::Strcmp(szAction, "create_contract")) return On_create_contract(cid);
        if (!Env::Strcmp(szAction, "set_owner")) return On_set_owner(cid);
    }
    if (!Env::Strcmp(szRole, "user")) {
        if (!Env::Strcmp(szAction, "increment")) return On_increment(cid);
    }
    OnError("Invalid action");
}
```

---

## Quick Reference

### Compilation Commands

```bash
# Contract
"$WASI_SDK_PREFIX/bin/clang++.exe" -O3 -std=c++17 -fno-rtti -fno-exceptions \
    -nostartfiles -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -isystem "$WASI_SDK_PREFIX/share/wasi-sysroot" -I. \
    -I"C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders" \
    -Wl,--export=Ctor -Wl,--export=Dtor -Wl,--export=Method_2 \
    -o contract.wasm contract.cpp

# Generate ShaderID
"C:/beam-shader-sdk/shader-sdk/bin/generate-sid.exe" contract.wasm

# App
"$WASI_SDK_PREFIX/bin/clang++.exe" -O3 -std=c++17 -fno-rtti -fno-exceptions \
    -nostartfiles -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -isystem "$WASI_SDK_PREFIX/share/wasi-sysroot" -I. \
    -I"C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders" \
    -o app.wasm app.cpp
```

### Testing Commands

```bash
# View schema
./beam-wallet.exe shader --shader_app_file app.wasm

# Deploy
./beam-wallet.exe shader --shader_app_file app.wasm \
    --shader_contract_file contract.wasm \
    --shader_args "role=manager,action=create_contract"

# Call method
./beam-wallet.exe shader --shader_app_file app.wasm \
    --shader_args "role=user,action=increment,cid=YOUR_CID,amount=100"
```

---
---

## Security Checklist

Before deploying any shader:

- [ ] All owner actions require `Env::AddSig(s.m_OwnerPk)`
- [ ] No hardcoded private keys or seeds
- [ ] All user inputs validated
- [ ] Integer overflow checks on arithmetic
- [ ] State always saved after modifications
- [ ] `Env::Halt()` called on invalid conditions
- [ ] Funds balance invariant maintained
- [ ] No infinite loops or unbounded iteration
- [ ] Error messages don't leak internal state
- [ ] Charge calculation includes all operations

---