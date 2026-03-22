# PriviMW — Privacy-First DApps on Beam

Open-source smart contracts (Beam Shaders) for the PriviMW ecosystem — confidential DApps built on Beam's MimbleWimble blockchain.

## DApps

| DApp | Description | Status |
|------|-------------|--------|
| **PriviBets** | On-chain betting (Dice 100, Roulette) | Live on Mainnet |
| **PriviMe** | Encrypted messaging + group chat with on-chain identity | Live on Mainnet |

## Repository Structure

```
privimw-dapps/
├── dapps/
│   ├── privibets/shaders/     # PriviBets contract + app shader (C++)
│   └── privime/shaders/       # PriviMe contract + app shader (C++)
├── shared/
│   └── shader-headers/        # Beam SDK headers (shared by all DApps)
├── docs/                      # Development reference
└── tools/scripts/             # Debug & utility scripts
```

## Building Shaders

### Prerequisites

- [WASI SDK 14.0](https://github.com/aspect-build/aspect-build-wasi-sdk/releases)
- [Beam Shader SDK](https://github.com/AlessandroLongo/beam-shader-sdk)

### Compile

```bash
WASI="/path/to/wasi-sdk-14.0"
SDK="/path/to/beam/bvm/Shaders"
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

## PriviMe Contract Methods

| Method | Action | Auth |
|--------|--------|------|
| Method_3 | RegisterHandle | Any (pays fee) |
| Method_4 | UpdateProfile | Handle owner |
| Method_5 | ReleaseHandle | Handle owner (no groups) |
| Method_10 | CreateGroup | Any registered handle |
| Method_11 | JoinGroup | Password for private groups |
| Method_12 | RemoveMember / Ban / Unban | Admin or creator |
| Method_13 | SetMemberRole | Creator only |
| Method_14 | UpdateGroupInfo | Admin or creator |
| Method_15 | LeaveGroup | Any member (not creator) |
| Method_16 | TransferOwnership | Creator only |
| Method_17 | DeleteGroup | Creator only |

Uses Upgradable3 for in-place contract upgrades (Method_2 reserved).

## Security

- All funds use Confidential Assets
- Owner actions verified with `AddSig`
- Group passwords stored as SHA-256 hash
- Handle deletion blocked if user is a group creator
- Banned users permanently blocked until unbanned

## License

Apache License 2.0 — see [LICENSE](LICENSE)
