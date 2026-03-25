# PriviMW — Privacy-First DApps on Beam

Open-source smart contracts (Beam Shaders) for the PriviMW ecosystem — confidential DApps built on [Beam](https://beam.mw)'s MimbleWimble blockchain.

Beam is a privacy-focused cryptocurrency using MimbleWimble protocol and the Beam Virtual Machine (BVM) for confidential smart contracts. All transactions are private by default.

## DApps

| DApp | Description | Status |
|------|-------------|--------|
| **PriviMe** | Encrypted messaging + group chats with on-chain identity | Live on Mainnet |
| **PriviBets** | On-chain casino — Dice 100 and American Roulette | Live on Mainnet |

**PriviMW Wallet** (Android) — [PriviMW/PriviMW-Wallet](https://github.com/PriviMW/PriviMW-Wallet)

## Repository Structure

```
privimw-dapps/
├── dapps/
│   ├── privibets/shaders/     # PriviBets — Dice + Roulette (C++)
│   └── privime/shaders/       # PriviMe — Messaging + Groups (C++)
├── shared/
│   ├── branding/              # PriviMW logo assets
│   └── shader-headers/        # Beam SDK headers (shared)
└── tools/scripts/             # Debug utilities (CDP)
```

## Building Shaders

### Prerequisites

- [WASI SDK](https://github.com/BeamMW/wasi-sdk)
- [Beam Shader SDK](https://github.com/BeamMW/shader-sdk)

### Compile

```bash
WASI="/path/to/wasi-sdk"
SDK="/path/to/shader-sdk/beam/bvm/Shaders"
SHARED="../../shared/shader-headers"

# Contract shader
"$WASI/bin/clang++" -O3 -std=c++17 -fno-rtti -fno-exceptions \
    -nostartfiles -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -I. -I"$SHARED" -I"$SDK" \
    -Wl,--export=Ctor -Wl,--export=Dtor -Wl,--export=Method_2 \
    -o contract.wasm contract.cpp

# App shader
"$WASI/bin/clang++" -O3 -std=c++17 -fno-rtti -fno-exceptions \
    -nostartfiles -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -I. -I"$SHARED" -I"$SDK" \
    -Wl,--export=Method_0 -Wl,--export=Method_1 \
    -o app.wasm app.cpp
```

### Verify Shader ID

After compiling, generate the Shader ID to verify contract integrity:

```bash
generate-sid.exe contract.wasm
```

## Contract Methods

### PriviMe — Messaging & Groups

| Method | Action | Auth |
|--------|--------|------|
| Method_3 | RegisterHandle | Any (pays registration fee) |
| Method_4 | UpdateProfile | Handle owner |
| Method_5 | ReleaseHandle | Handle owner (must not be a group creator) |
| Method_10 | CreateGroup | Any registered handle |
| Method_11 | JoinGroup | Password required for private groups |
| Method_12 | RemoveMember / Ban / Unban | Admin or creator |
| Method_13 | SetMemberRole | Creator only |
| Method_14 | UpdateGroupInfo | Admin or creator |
| Method_15 | LeaveGroup | Any member (creator cannot leave) |
| Method_16 | TransferOwnership | Creator only |
| Method_17 | DeleteGroup | Creator only |

Uses Upgradable3 for in-place contract upgrades (Method_2 reserved for upgrade dispatch).

### PriviBets — Dice 100 & Roulette

| Method | Action | Auth |
|--------|--------|------|
| Method_2 | PlaceBet | Any user |
| Method_3 | RevealBet | House (auto-resolves expired bets) |
| Method_4 | CheckResult / Claim | Bet owner |
| Method_5 | Deposit | House owner |
| Method_6 | Withdraw | House owner |
| Method_7 | SetConfig | House owner |

Provably fair — results derived from on-chain block hash + bet nonce via SHA-256. Users can verify independently.

## Security

- All funds use Beam Confidential Assets (private amounts)
- Owner actions verified via BVM `AddSig` (signature verification)
- Group join passwords stored as SHA-256 hash (not plaintext)
- Handle deletion blocked if user is a group creator
- Banned users permanently blocked from rejoining until admin unbans
- PriviBets results are deterministic and verifiable on-chain

## Ecosystem

- [Beam](https://beam.mw) — Privacy blockchain
- [Beam Explorer](https://explorer.beam.mw) — Block explorer
- [Beam Documentation](https://documentation.beam.mw) — Developer docs
- [PriviMW Wallet](https://github.com/PriviMW/PriviMW-Wallet) — Android app

## License

Apache License 2.0 — see [LICENSE](LICENSE)
