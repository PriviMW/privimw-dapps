## Troubleshooting Guide

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Low fee" | Insufficient charge in GenerateKernel | Increase charge calculation |
| "Halt" | Contract rejected operation | Check validation logic |
| "Key not found" | Wrong key derivation | Verify OwnerKey/UserKey structure |
| "Contract not found" | Invalid CID | Verify contract is deployed |
| "Invalid signature" | AddSig failed | Ensure correct pubkey is used |
| "Funds insufficient" | Pool balance too low | Check FundsLock/Unlock balance |
| "Failed to read state" | LoadVar failed | Verify key structure matches |
| "get_HdrInfo Error" | m_Height not set before call | Set `hdr.m_Height = Env::get_Height()` before `Env::get_HdrInfo(hdr)` |

### Debug Steps

1. **Check shader ID matches**: Verify `s_SID` in contract.h matches compiled wasm
2. **Verify key derivation**: Ensure same seed used in app and contract
3. **Check charge calculation**: Add more cycles if transaction fails
4. **Verify state keys**: Ensure key structures match between save/load
5. **Test schema first**: Run `beam-wallet shader --shader_app_file app.wasm` to check schema

---

## Build Script Templates

### Windows Build (Current)

```bash
export WASI_SDK_PREFIX="C:/beam-shader-sdk/shader-sdk/wasi-sdk-14.0"
cd shaders/beambet

# Contract shader (note: must export Method_10 for resolve_bets)
"$WASI_SDK_PREFIX/bin/clang++.exe" \
    -O3 -std=c++17 -fno-rtti -fno-exceptions -nostartfiles \
    -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -isystem "$WASI_SDK_PREFIX/share/wasi-sysroot" \
    -I. -I"C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders" \
    -Wl,--export=Ctor -Wl,--export=Dtor \
    -Wl,--export=Method_2 -Wl,--export=Method_3 -Wl,--export=Method_4 \
    -Wl,--export=Method_5 -Wl,--export=Method_6 -Wl,--export=Method_7 \
    -Wl,--export=Method_8 -Wl,--export=Method_9 -Wl,--export=Method_10 \
    -o contract.wasm contract.cpp

# App shader
"$WASI_SDK_PREFIX/bin/clang++.exe" \
    -O3 -std=c++17 -fno-rtti -fno-exceptions -nostartfiles \
    -Wl,--no-entry,--allow-undefined,--export-dynamic \
    -isystem "$WASI_SDK_PREFIX/share/wasi-sysroot" \
    -I. -I"C:/beam-shader-sdk/shader-sdk/beam/bvm/Shaders" \
    -o app.wasm app.cpp
```

---

## Current BeamBet Configuration (v2 - Security Redesign)

### ShaderID
```
039a14fc3a720ba18693cf6f42279ce600538d48cfa0c84ec49f48ea0c1cb0e0
```

### Contract ID (DAppNet)
```
e28c2b241a6871f4290d7e0c80b701273bbb48ee79ce5712f5873cdae3330c16
```
Deployed at block 7312467.

### Security Fixes in v2
1. **Randomness fix**: Uses `get_HdrInfo` block hash at bet placement time as entropy (user can't precompute)
2. **Pool solvency**: `m_PendingMaxPayout` tracks worst-case reserves (uses ExactMult for all bets)
3. **Won-payout protection**: `m_PendingPayouts` prevents owner withdrawing funds owed to winners
4. **Deterministic reveal_bet**: Owner emergency reveal uses same formula as user (no manipulation)
5. **Input validation**: asset_id, bet_type (0-2), exact_number (1-100 for type=2)
6. **SetConfig protection**: Can't change AssetID while bets are pending
7. **Underflow protection**: Safe subtraction in GetAvailableBalance()
8. **Dead code removed**: m_UserNonce, m_NonceRevealed, s_PublicRevealDelay, ProcessBetPayout

### Key API Discovery: get_HdrInfo
- `hdr.m_Height` MUST be set as INPUT before calling `Env::get_HdrInfo(hdr)`
- Returns block hash in `hdr.m_Hash` (HashValue, 32 bytes)
- Available in both contract and app shader contexts (BVMOpsAll_Common)
- Struct defined in bvm2_shared.h: `BlockHeader::Info` (112 bytes)

### Methods

| Method | ID | Description |
|--------|-----|-------------|
| Ctor | 0 | Constructor |
| Dtor | 1 | Destructor |
| PlaceBet | 2 | Place a bet (with block hash entropy) |
| CheckResults | 3 | User reveals AND claims ALL pending bets |
| Deposit | 4 | Owner deposits to pool |
| Withdraw | 5 | Owner withdraws from pool |
| SetOwner | 6 | Transfer ownership |
| SetConfig | 7 | Update game config (can't change AssetID with pending bets) |
| RevealBet | 8 | Owner emergency reveal (deterministic, same formula) |
| CheckSingleResult | 9 | User reveals AND claims ONE bet by ID |
| ResolveExpiredBets | 10 | Anyone resolves expired bets (reveal only, no payout) |

### Randomness Formula
```
result = SHA256(commitment || placementHash || revealHeight || betId)
  - commitment: user-provided at bet time (personalization)
  - placementHash: block hash at PlaceBet time (entropy, unknowable to user pre-submission)
  - revealHeight: createdHeight + revealEpoch (10 blocks)
  - betId: sequential ID assigned by contract
Result = (first_2_bytes_of_hash % 100) + 1  (range: 1-100)
```

### Pool Balance Formula
```
total_in_contract = m_TotalDeposited + m_TotalBets - m_TotalPayouts
reserved = m_PendingMaxPayout + m_PendingPayouts
available = max(total_in_contract - reserved, 0)
```

### App Shader Actions

**Manager Actions:**
- `create_contract` - Deploy new contract
- `view_contracts` - List deployed contracts
- `view_pool` - View pool statistics (includes pending_max_payout, pending_payouts)
- `view_all_bets` - Full bet history from all users
- `deposit` - Add funds to pool
- `withdraw` - Remove funds from pool
- `set_config` - Update game parameters
- `set_owner` - Transfer ownership
- `reveal_bet` - Deterministic emergency reveal (bet_id only, no random_value)
- `resolve_bets` - Batch resolve expired pending bets (count parameter)

**User Actions:**
- `view_params` - View game parameters
- `place_bet` - Place a bet
- `check_results` - Reveal and claim ALL pending bets
- `check_result` - Reveal and claim ONE bet by bet_id
- `my_bets` - View user's pending bets
- `result_history` - View user's betting history

---

## Quick Reference Commands

```bash
# Wallet CLI path
WALLET_CLI="/path/to/wallet"

# View contracts
$WALLET_CLI shader --shader_app_file app.wasm --shader_args "role=manager,action=view_contracts"

# View pool
$WALLET_CLI shader --shader_app_file app.wasm --shader_args "role=manager,action=view_pool,cid=CID"

# Place bet
$WALLET_CLI shader --shader_app_file app.wasm --shader_args "role=user,action=place_bet,cid=CID,amount=100000000,asset_id=0,bet_type=0,exact_number=0,commitment=HASH"

# Check single result
$WALLET_CLI shader --shader_app_file app.wasm --shader_args "role=user,action=check_result,cid=CID,bet_id=0"

# Resolve expired bets
$WALLET_CLI shader --shader_app_file app.wasm --shader_args "role=manager,action=resolve_bets,cid=CID,count=50"
```

---

*Last updated: February 2026 (v2 security redesign)*
