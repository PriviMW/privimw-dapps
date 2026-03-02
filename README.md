# PriviMW — Privacy-First DApps on Beam

Confidential, provably fair on-chain gaming built on [Beam](https://beam.mw)'s MimbleWimble blockchain. All results are verifiable on-chain, all transactions use Confidential Assets.

## PriviBets

The first PriviMW DApp — a multi-game casino platform running inside the Beam Desktop Wallet.

### Games

**Dice 100** — Roll a d100. Bet Up (51-100), Down (1-50), or Exact (pick a number).
- Up/Down: 1.9x payout (5% house edge)
- Exact: 95x payout (5% house edge)
- Result: `SHA256(block_hash + reveal_height + bet_id) mod 100 + 1`

**American Roulette** — Place up to 10 chip bets per spin on a full roulette board.
- 13 bet types: Straight (35:1), Red/Black/Odd/Even/Low/High (1:1), Dozen 1-3/Column 1-3 (2:1)
- 38 outcomes: 0-36 + 00
- All bets on one spin share the same result (like real roulette)
- Result: `SHA256(placement_hash + reveal_height + spin_id) mod 38`

### How It Works

1. **Place bet** — funds locked into the contract
2. **Wait ~3 blocks** (~3 minutes) for the reveal height
3. **Claim results** — contract computes result from the block hash at reveal height, pays out wins

The reveal block doesn't exist when you place your bet, so the result is unpredictable. No server seeds, no house manipulation — pure blockchain randomness.

### Provably Fair Verification

Every bet shows its Bet ID and reveal block height. You can independently verify any result:
1. Look up the block hash at the reveal height on any Beam explorer
2. Apply the SHA256 formula with your bet parameters
3. Confirm the result matches what the contract computed

## Tech Stack

- **Smart Contracts**: Beam Shaders (C++ compiled to WASM via WASI SDK)
- **Frontend**: Vanilla JS single-file HTML (runs inside Beam Wallet WebView at 440px)
- **Privacy**: Confidential Assets — fund amounts are hidden in wallet-to-wallet transfers
- **No Backend**: Everything runs on-chain + in the wallet. No servers, no databases.

## Repository Structure

```
dapps/privibets/
├── shaders/
│   ├── contract.h / .cpp / .wasm       # Dice 100 contract
│   ├── app.cpp / app.wasm              # Dice 100 app shader
│   ├── roulette.h / .cpp / .wasm       # American Roulette contract
│   └── roulette_app.cpp / .wasm        # American Roulette app shader
├── ui/
│   └── index.html                      # Single-file DApp UI (both games)
├── build/                              # .dapp packaging staging area
├── releases/                           # Mainnet releases
├── manifest.json                       # DApp manifest for Beam Wallet
└── package.sh                          # Build .dapp package script

shared/
├── shader-headers/                     # Beam SDK headers
└── branding/                           # PriviMW brand assets

docs/
├── SHADER.md                           # Shader development reference
├── UI_DAPP.md                          # DApp UI architecture reference
└── SPEC.md                             # Design spec
```

## Architecture

Beam DApps use **two separate shaders** per game:

**Contract Shader** (deployed on-chain) — holds funds, enforces rules, manages state. Exports `Ctor`, `Dtor`, and `Method_2..N`.

**App Shader** (runs in wallet) — reads contract state, derives keys, builds transactions. Exports `Method_0` (schema) and `Method_1` (dispatcher).

Each game has its own independent contract + app shader pair. The UI loads both app shaders and routes calls to the correct one based on which game is active.

## Building

### Prerequisites

- [WASI SDK 14.0](https://github.com/aspect-build/aspect-frameworks/releases) for shader compilation
- [Beam Shader SDK](https://github.com/AthensGroup/beam-shader-sdk) headers
- [Beam Desktop Wallet](https://beam.mw/downloads) (DAPPNET for dev, Mainnet for production)
- 7-Zip for .dapp packaging

### Compile Shaders

```bash
cd dapps/privibets/shaders/

WASI="C:/beam-shader-sdk/shader-sdk/wasi-sdk-14.0"
SDK="C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders"
SHARED="../../shared/shader-headers"

# Contract shader (dice example)
"$WASI/bin/clang++.exe" -O3 -std=c++17 -fno-rtti -fno-exceptions \
    -nostartfiles -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -I. -I"$SHARED" -I"$SDK" \
    -Wl,--export=Ctor -Wl,--export=Dtor -Wl,--export=Method_2 \
    -Wl,--export=Method_3 ... \
    -o contract.wasm contract.cpp

# App shader
"$WASI/bin/clang++.exe" -O3 -std=c++17 -fno-rtti -fno-exceptions \
    -nostartfiles -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -I. -I"$SHARED" -I"$SDK" \
    -Wl,--export=Method_0 -Wl,--export=Method_1 \
    -o app.wasm app.cpp
```

### Deploy Contract

```bash
beam-wallet-dappnet shader \
    --shader_app_file app.wasm \
    --shader_contract_file contract.wasm \
    --shader_args "role=manager,action=create_contract"
```

### Package .dapp

```bash
cd dapps/privibets && bash package.sh
# Produces PriviBets.dapp — install in Beam Wallet via DApps > Install from file
```

## Install

Download the latest `.dapp` file from [Releases](../../releases) and install it in Beam Desktop Wallet:

1. Open Beam Wallet (DAPPNET or Mainnet)
2. Go to **DApp Store**
3. Click **Install from file**
4. Select the `.dapp` file

## Deployment

| Network | Status |
|---------|--------|
| **Mainnet** | Dice 100 live |
| **DAppNet** | Dice 100 + American Roulette |

## Security

- All funds managed by on-chain smart contracts (non-custodial)
- Owner actions require cryptographic signatures
- Pool solvency enforced — worst-case payouts reserved before accepting bets
- Provably fair — deterministic results from block hashes
- Input validation on all contract methods
- Overflow protection on payout calculations

## License

MIT
