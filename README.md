# Beam DApp Development Framework

A comprehensive framework for building **any type** of privacy-focused DApp on the Beam blockchain. This project includes a complete reference implementation (BeamBet betting game) and all the patterns needed to create your own confidential decentralized applications.

## What You Can Build

This framework supports creating any Beam DApp, including:

| DApp Type | Description | Complexity |
|-----------|-------------|------------|
| **Token/Vault** | Simple fund storage with owner controls | Beginner |
| **Escrow** | Two-party escrow with timeout | Beginner |
| **Timelock** | Time-locked funds | Beginner |
| **NFT** | Non-fungible tokens with mint/transfer/burn | Intermediate |
| **Auction** | Timed auctions with bidding | Intermediate |
| **Governance** | Voting and proposal systems | Intermediate |
| **Multisig** | N-of-M signature wallets | Intermediate |
| **Betting** | Gambling games with commit-reveal | Advanced |
| **Exchange** | Token swaps and order books | Advanced |
| **Lottery** | Random drawing mechanisms | Advanced |

## Reference Implementation: BeamBet

The `shaders/beambet/` directory contains a complete betting DApp that demonstrates all key patterns:

- Owner authentication with deterministic key derivation
- Fund management with lock/unlock patterns
- State persistence with LoadVar/SaveVar
- Multi-method dispatch in app shader
- React UI with wallet API integration

### BeamBet Features

- **Confidential Betting**: All bets use Beam's Confidential Assets
- **Provably Fair**: Commit-reveal scheme ensures transparent randomness
- **Three Bet Types**: UP (>50), DOWN (<50), EXACT (1-100)
- **House Edge**: ~5% on all bets
- **Non-Custodial**: User funds stay in shader, not a server
- **Admin Controls**: Owner can manage pool, pause game, update settings

## Quick Start

### Prerequisites

- Node.js 18+ and Yarn
- Beam Wallet (DAPPNET version) installed
- WASI SDK 14.0 for shader compilation
- Beam Shader SDK

### Development

```bash
# Start development server
cd ui
yarn start
# Opens at http://localhost:3000

# Wallet debug mode (for Chrome DevTools)
"C:\Program Files\Beam Wallet\DAPPNET\Beam Wallet.exe" --remote-debugging-port=20000
# Then open chrome://inspect
```

## Project Structure

```
beam-privacy-dapp/
├── shaders/
│   ├── beambet/              # Reference implementation
│   │   ├── contract.h        # Shared structures, ShaderID
│   │   ├── contract.cpp      # Contract shader (on-chain logic)
│   │   ├── app.cpp           # App shader (wallet helper)
│   │   ├── common.h          # Beam SDK types
│   │   └── build.bat         # Build script
│   ├── contract/             # Alternative shader location
│   └── app/                  # Standalone app shader
├── ui/
│   ├── src/
│   │   ├── api/
│   │   │   ├── beam.ts       # Core wallet API integration
│   │   │   └── beambet.ts    # DApp-specific API functions
│   │   ├── components/       # React components
│   │   ├── pages/            # Page components (UserPage, AdminPage)
│   │   ├── App.tsx           # Main app
│   │   └── index.tsx         # Entry point
│   └── package.json
├── MEMORY.md                 # Comprehensive shader development guide
├── config.md                 # Development instructions & patterns
├── README.md                 # This file
└── manifest.json             # DApp manifest for wallet
```

## Creating Your Own DApp

### Step 1: Create Directory

```bash
mkdir shaders/your-dapp
cd shaders/your-dapp
```

### Step 2: Copy SDK Files

```bash
cp ../beambet/common.h .
cp ../beambet/bvm2_cost.h .
cp ../beambet/app_common_impl.h .
```

### Step 3: Write Your Shader

See `MEMORY.md,SHADER.md,UI_DAPP.md` for complete patterns and examples. Key files:

1. **contract.h** — Define state, methods, ShaderID
2. **contract.cpp** — Implement on-chain logic
3. **app.cpp** — Implement wallet helper

### Step 4: Build

```bash
build.bat  # Windows
./build.sh # Linux/Mac
```

### Step 5: Deploy

```bash
# Generate ShaderID
generate-sid.exe contract.wasm

# Add to contract.h and rebuild

# Deploy
beam-wallet-dappnet shader \
    --shader_app_file app.wasm \
    --shader_contract_file contract.wasm \
    --shader_args "role=manager,action=create_contract"
```

## Shader Development Quick Reference

### Owner Authentication Pattern

```cpp
// App shader: Derive key
OwnerKey ok;
Env::KeyID kid(&ok, sizeof(ok));
Env::GenerateKernel(&cid, method, &args, sizeof(args), nullptr, 0, &kid, 1, "...", nCharge);

// Contract shader: Verify signature
Env::AddSig(s.m_OwnerPk);
```

### Fund Handling

```cpp
// Lock funds INTO contract
Env::FundsLock(assetId, amount);

// Unlock funds FROM contract
Env::FundsUnlock(assetId, amount);
```

### Charge Calculation

```cpp
uint32_t nCharge =
    Env::Cost::LoadVar_For(sizeof(State)) +
    Env::Cost::SaveVar_For(sizeof(State)) +
    Env::Cost::AddSig +
    (Env::Cost::Cycle * 500);
```

## BeamBet Contract API

The reference implementation (BeamBet) provides this API:

### User Methods

| Params | Description |
|--------|-------------|
| `role=user,action=view_params` | View contract parameters |
| `role=user,action=view_bet,bet_id=<id>` | View bet details |
| `role=user,action=place_bet,amount=<groth>,bet_type=<0\|1\|2>,commitment=<hex>` | Place a bet |
| `role=user,action=claim_winnings,bet_id=<id>` | Claim winnings |

### Manager Methods (Owner Only)

| Params | Description |
|--------|-------------|
| `role=manager,action=create_contract` | Initialize contract |
| `role=manager,action=view_pool` | View pool state |
| `role=manager,action=reveal_bet,bet_id=<id>,random_value=<1-100>` | Settle bet |
| `role=manager,action=deposit,amount=<groth>` | Add liquidity |
| `role=manager,action=withdraw,amount=<groth>` | Remove liquidity |
| `role=manager,action=set_config,min_bet=<groth>,max_bet=<groth>` | Update settings |

## Packaging & Deployment

### Build Package

```bash
# Create package directory
mkdir -p dist/yourdapp

# Copy UI build
cp -r ui/build/* dist/yourdapp/

# Copy shaders
cp shaders/your-dapp/contract.wasm dist/yourdapp/
cp shaders/your-dapp/app.wasm dist/yourdapp/

# Copy manifest
cp manifest.json dist/yourdapp/
```

### Upload to IPFS

```bash
beam-wallet-dappnet ipfs upload yourdapp.dapp
# Returns: ipfs://QmXxx...
```

### Load in Wallet

1. Open Beam Wallet (DAPPNET)
2. Go to **DApps** tab
3. Click **Add DApp** → **From IPFS** or **From File**
4. Enter IPFS URL or select file

## Troubleshooting

| Error | Solution |
|-------|----------|
| "Low fee" | Increase charge in GenerateKernel |
| "Contract not found" | Verify CID matches deployed contract |
| "Invalid signature" | Check OwnerKey derivation matches |
| "Halt" | Contract validation failed — check inputs |
| Schema not showing | Verify Method_0 is exported |

## Documentation

- **MEMORY.md** — Complete shader development guide with patterns, APIs, troubleshooting
- **config.md** — Development workflow and patterns for AI-assisted development
- **Beam SDK** — `C:\beam-shader-sdk\shader-sdk\beam\bvm\Shaders\`

## Security Notes

- All transactions use Confidential Assets
- Owner actions require cryptographic signatures
- Never commit private keys or seeds
- Validate all user inputs in contract
- Use `Env::Halt()` for invalid states
- Maintain pool invariant: funds in == funds out + balance

## License

MIT