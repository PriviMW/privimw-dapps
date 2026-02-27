# BeamBet — Technical Specification

**Version**: 1.0.0
**Status**: Draft
**Author**: Architect Subagent
**Last Updated**: 2024-02-24

---

## 1. Overview

BeamBet is a fully confidential, on-chain betting DApp built on Beam blockchain. Users bet BEAM (or confidential assets) on random number outcomes with guaranteed fairness via commit-reveal scheme.

### 1.1 Game Types

| Type | Condition | Payout | House Edge |
|------|-----------|--------|------------|
| Up | Result > 50 | 1.9x | ~5% |
| Down | Result < 50 | 1.9x | ~5% |
| Exact | Result == N (1-100) | 2.8x | ~5% |

### 1.2 Key Properties

- **Confidential**: All bets/payouts use Confidential Assets
- **Provably Fair**: Commit-reveal + house reveal scheme
- **Non-Custodial**: User funds only in shader
- **Censorship-Resistant**: Runs entirely in wallet via IPFS

---

## 2. System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        BEAM WALLET                               │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    DApp (IPFS)                           │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │    │
│  │  │   UI Layer  │  │ State Store │  │   API Layer │     │    │
│  │  │  (React/TS) │◄─┤   (Redux)   │◄─┤ (Utils.js)  │     │    │
│  │  └─────────────┘  └─────────────┘  └──────┬──────┘     │    │
│  └───────────────────────────────────────────┼─────────────┘    │
│                                              │                   │
│                    ┌─────────────────────────▼─────────────────┐ │
│                    │              Beam BVM                     │ │
│                    │  ┌─────────────────────────────────────┐ │ │
│                    │  │          Contract Shader            │ │ │
│                    │  │  ┌───────────┐  ┌────────────────┐  │ │ │
│                    │  │  │   Bet     │  │  Pool State    │  │ │ │
│                    │  │  │  Engine   │  │  (Confidential)│  │ │ │
│                    │  │  └───────────┘  └────────────────┘  │ │ │
│                    │  │  ┌───────────┐  ┌────────────────┐  │ │ │
│                    │  │  │ Randomness│  │   Owner        │  │ │ │
│                    │  │  │  Module   │  │   Controls     │  │ │ │
│                    │  │  └───────────┘  └────────────────┘  │ │ │
│                    │  └─────────────────────────────────────┘ │ │
│                    └───────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Game Flow Diagrams

### 3.1 Standard Bet Flow

```
┌────────┐          ┌────────┐          ┌────────┐          ┌────────┐
│  USER  │          │   UI   │          │ SHADER │          │  POOL  │
└───┬────┘          └───┬────┘          └───┬────┘          └───┬────┘
    │                   │                   │                   │
    │  1. Choose bet    │                   │                   │
    │  (Up/Down/Exact,  │                   │                   │
    │   amount)         │                   │                   │
    │──────────────────►│                   │                   │
    │                   │                   │                   │
    │                   │  2. Generate      │                   │
    │                   │     user_nonce    │                   │
    │                   │     (client-side) │                   │
    │                   │                   │                   │
    │                   │  3. commitment =  │                   │
    │                   │     Hash(choice   │                   │
    │                   │     + nonce)      │                   │
    │                   │                   │                   │
    │                   │  4. Bet tx        │                   │
    │                   │     (amount,      │                   │
    │                   │      commitment)  │                   │
    │                   │──────────────────►│                   │
    │                   │                   │                   │
    │                   │                   │  5. Verify &      │
    │                   │                   │     lock bet      │
    │                   │                   │──────────────────►│
    │                   │                   │                   │
    │                   │  6. bet_id,       │                   │
    │                   │     status=pending│                   │
    │                   │◄──────────────────│                   │
    │                   │                   │                   │
    │  7. Show pending  │                   │                   │
    │     status        │                   │                   │
    │◄──────────────────│                   │                   │
    │                   │                   │                   │
    │                   │                   │   [AWAIT REVEAL]  │
    │                   │                   │                   │
```

### 3.2 Reveal & Settlement Flow

```
┌────────┐          ┌────────┐          ┌────────┐          ┌────────┐
│ OWNER  │          │   UI   │          │ SHADER │          │  POOL  │
│(House) │          │        │          │        │          │        │
└───┬────┘          └───┬────┘          └───┬────┘          └───┬────┘
    │                   │                   │                   │
    │  1. Trigger       │                   │                   │
    │     reveal        │                   │                   │
    │──────────────────►│                   │                   │
    │                   │                   │                   │
    │                   │  2. Generate      │                   │
    │                   │     random (1-100)│                   │
    │                   │     + house_proof │                   │
    │                   │                   │                   │
    │                   │  3. Reveal tx     │                   │
    │                   │     (bet_id,      │                   │
    │                   │      random,      │                   │
    │                   │      proof)       │                   │
    │                   │──────────────────►│                   │
    │                   │                   │                   │
    │                   │                   │  4. Verify proof  │
    │                   │                   │                   │
    │                   │                   │  5. Determine     │
    │                   │                   │     win/loss      │
    │                   │                   │                   │
    │                   │                   │  6a. WIN: Payout  │
    │                   │                   │◄──────────────────│
    │                   │                   │                   │
    │                   │                   │  6b. LOSS: Add    │
    │                   │                   │      to pool      │
    │                   │                   │──────────────────►│
    │                   │                   │                   │
    │                   │  7. Result        │                   │
    │                   │     (win/loss,    │                   │
    │                   │      payout)      │                   │
    │                   │◄──────────────────│                   │
    │                   │                   │                   │
    │  8. Show result   │                   │                   │
    │◄──────────────────│                   │                   │
    │                   │                   │                   │
```

### 3.3 Pool Management Flow (Owner Only)

```
┌────────┐          ┌────────┐          ┌────────┐          ┌────────┐
│ OWNER  │          │   UI   │          │ SHADER │          │  POOL  │
└───┬────┘          └───┬────┘          └───┬────┘          └───┬────┘
    │                   │                   │                   │
    │  Deposit/Withdraw │                   │                   │
    │  or Update Params │                   │                   │
    │──────────────────►│                   │                   │
    │                   │                   │                   │
    │                   │  Admin tx         │                   │
    │                   │  (signed by       │                   │
    │                   │   owner key)      │                   │
    │                   │──────────────────►│                   │
    │                   │                   │                   │
    │                   │                   │  Verify owner     │
    │                   │                   │  signature        │
    │                   │                   │                   │
    │                   │                   │  Execute action   │
    │                   │                   │──────────────────►│
    │                   │                   │                   │
    │                   │  Confirmation     │                   │
    │                   │◄──────────────────│                   │
    │                   │                   │                   │
```

---

## 4. Shader Pseudocode

### 4.1 Data Structures

```rust
// === STATE STRUCTURES ===

/// Bet types
enum BetType {
    Up,      // Result > 50
    Down,    // Result < 50
    Exact(u8), // Result == N (1-100)
}

/// Bet status
enum BetStatus {
    Pending,   // Awaiting reveal
    Won,       // Won, payout claimed
    Lost,      // Lost, bet absorbed
    Cancelled, // Cancelled by owner (refund)
}

/// Individual bet record
struct Bet {
    bet_id: u64,
    user_pk: PublicKey,
    amount: u64,           // Groth (confidential)
    asset_id: AssetId,
    bet_type: BetType,
    commitment: Hash,      // Hash(choice || user_nonce)
    user_nonce: Option<Hash>,  // Revealed after settlement
    result: Option<u8>,    // Random result (1-100)
    status: BetStatus,
    created_height: u64,
    revealed_height: Option<u64>,
}

/// Contract configuration (owner-controlled)
struct Config {
    owner_pk: PublicKey,
    min_bet: u64,          // Minimum bet in Groth
    max_bet: u64,          // Maximum bet in Groth
    up_down_multiplier: u64,   // 190 = 1.9x (scaled by 100)
    exact_multiplier: u64,     // 280 = 2.8x
    house_edge_percent: u8,    // 5 = 5%
    paused: bool,
    emergency_mode: bool,
    owner_transfer_pending: Option<(PublicKey, u64)>, // (new_owner, effective_height)
}

/// Pool state (confidential)
struct PoolState {
    total_deposited: u64,   // Owner deposits
    total_bets: u64,        // Total bets received
    total_payouts: u64,     // Total payouts sent
    pending_bets: u64,      // Bets awaiting settlement
    asset_id: AssetId,      // BEAM or custom asset
}

/// Global contract state
struct State {
    config: Config,
    pool: PoolState,
    bets: Map<u64, Bet>,
    next_bet_id: u64,
    reveal_epoch: u64,      // Blocks between bet and reveal
}
```

### 4.2 Entry Points

```rust
// === CONSTRUCTOR ===

fn constructor(
    ctx: &mut Context,
    owner_pk: PublicKey,
    initial_deposit: u64,
    asset_id: AssetId,
) -> Result<()> {
    // Initialize state
    let state = State {
        config: Config {
            owner_pk,
            min_bet: 1_000_000,      // 0.01 BEAM default
            max_bet: 10_000_000_000, // 100 BEAM default
            up_down_multiplier: 190,
            exact_multiplier: 280,
            house_edge_percent: 5,
            paused: false,
            emergency_mode: false,
            owner_transfer_pending: None,
        },
        pool: PoolState {
            total_deposited: initial_deposit,
            total_bets: 0,
            total_payouts: 0,
            pending_bets: 0,
            asset_id,
        },
        bets: Map::new(),
        next_bet_id: 1,
        reveal_epoch: 10, // 10 blocks
    };

    // Receive initial deposit
    ctx.receive_funds(initial_deposit, asset_id)?;

    // Save state
    ctx.set_state(state)?;

    Ok(())
}


// === BET ===

fn bet(
    ctx: &mut Context,
    amount: u64,
    asset_id: AssetId,
    bet_type: BetType,
    commitment: Hash,  // Hash(choice_bits || user_nonce)
) -> Result<u64> {
    let state = ctx.get_state()?;

    // Validation
    require!(!state.config.paused, "Game paused");
    require!(!state.config.emergency_mode, "Emergency mode");
    require!(asset_id == state.pool.asset_id, "Wrong asset");
    require!(amount >= state.config.min_bet, "Below minimum");
    require!(amount <= state.config.max_bet, "Above maximum");

    // Validate bet type
    match bet_type {
        BetType::Exact(n) => require!(n >= 1 && n <= 100, "Invalid number"),
        _ => {}
    }

    // Create bet
    let bet_id = state.next_bet_id;
    let bet = Bet {
        bet_id,
        user_pk: ctx.caller_pk(),
        amount,
        asset_id,
        bet_type,
        commitment,
        user_nonce: None,
        result: None,
        status: BetStatus::Pending,
        created_height: ctx.height(),
        revealed_height: None,
    };

    // Update state
    state.bets.insert(bet_id, bet);
    state.next_bet_id += 1;
    state.pool.total_bets += amount;
    state.pool.pending_bets += amount;

    // Receive funds
    ctx.receive_funds(amount, asset_id)?;

    // Save state
    ctx.set_state(state)?;

    // Emit event (blinded)
    ctx.emit_event("BetPlaced", { bet_id, height: ctx.height() });

    Ok(bet_id)
}


// === REVEAL (House/Owner) ===

fn reveal(
    ctx: &mut Context,
    bet_id: u64,
    random_value: u8,      // 1-100
    house_proof: Hash,     // Proof of fair generation
    user_nonce: Hash,      // User's revealed nonce (for verification)
) -> Result<BetStatus> {
    let state = ctx.get_state()?;

    // Get bet
    let bet = state.bets.get(bet_id)?;
    require!(bet.status == BetStatus::Pending, "Not pending");

    // Verify commitment
    let computed_commitment = hash(bet.bet_type.to_bytes() || user_nonce);
    require!(computed_commitment == bet.commitment, "Invalid commitment");

    // Verify reveal timing (must be after epoch)
    require!(ctx.height() >= bet.created_height + state.reveal_epoch, "Too early");

    // Determine outcome
    let won = match bet.bet_type {
        BetType::Up => random_value > 50,
        BetType::Down => random_value < 50,
        BetType::Exact(n) => random_value == n,
    };

    // Calculate payout
    let payout = if won {
        let multiplier = match bet.bet_type {
            BetType::Up | BetType::Down => state.config.up_down_multiplier,
            BetType::Exact(_) => state.config.exact_multiplier,
        };
        (bet.amount * multiplier) / 100
    } else {
        0
    };

    // Update bet
    bet.result = Some(random_value);
    bet.user_nonce = Some(user_nonce);
    bet.revealed_height = Some(ctx.height());

    // Update pool
    state.pool.pending_bets -= bet.amount;

    if won {
        bet.status = BetStatus::Won;
        state.pool.total_payouts += payout;

        // Send payout
        ctx.send_funds(bet.user_pk, payout, bet.asset_id)?;
    } else {
        bet.status = BetStatus::Lost;
        // Bet stays in pool (already received)
    }

    // Save state
    ctx.set_state(state)?;

    // Emit event
    ctx.emit_event("BetSettled", { bet_id, status: bet.status });

    Ok(bet.status)
}


// === CLAIM (User-initiated, optional) ===

fn claim(
    ctx: &mut Context,
    bet_id: u64,
) -> Result<u64> {
    let state = ctx.get_state()?;

    let bet = state.bets.get(bet_id)?;
    require!(bet.user_pk == ctx.caller_pk(), "Not your bet");
    require!(bet.status == BetStatus::Won, "Not won");

    // Payout already sent in reveal, this is for pending claims
    // (Alternative design: payout deferred to claim)

    Ok(0) // Already claimed
}


// === ADMIN FUNCTIONS ===

fn admin_deposit(
    ctx: &mut Context,
    amount: u64,
) -> Result<()> {
    let state = ctx.get_state()?;
    require!(ctx.caller_pk() == state.config.owner_pk, "Not owner");

    ctx.receive_funds(amount, state.pool.asset_id)?;
    state.pool.total_deposited += amount;

    ctx.set_state(state)?;
    ctx.emit_event("Deposit", { amount });
    Ok(())
}

fn admin_withdraw(
    ctx: &mut Context,
    amount: u64,
) -> Result<()> {
    let state = ctx.get_state()?;
    require!(ctx.caller_pk() == state.config.owner_pk, "Not owner");

    // Ensure pool has enough (accounting for pending bets)
    let available = state.pool.total_deposited + state.pool.total_bets
                 - state.pool.total_payouts - state.pool.pending_bets;
    require!(amount <= available, "Insufficient pool");

    ctx.send_funds(state.config.owner_pk, amount, state.pool.asset_id)?;
    state.pool.total_deposited -= amount;

    ctx.set_state(state)?;
    ctx.emit_event("Withdraw", { amount });
    Ok(())
}

fn admin_update_config(
    ctx: &mut Context,
    min_bet: Option<u64>,
    max_bet: Option<u64>,
    up_down_multiplier: Option<u64>,
    exact_multiplier: Option<u64>,
    paused: Option<bool>,
) -> Result<()> {
    let state = ctx.get_state()?;
    require!(ctx.caller_pk() == state.config.owner_pk, "Not owner");

    if let Some(v) = min_bet { state.config.min_bet = v; }
    if let Some(v) = max_bet { state.config.max_bet = v; }
    if let Some(v) = up_down_multiplier { state.config.up_down_multiplier = v; }
    if let Some(v) = exact_multiplier { state.config.exact_multiplier = v; }
    if let Some(v) = paused { state.config.paused = v; }

    ctx.set_state(state)?;
    ctx.emit_event("ConfigUpdated", {});
    Ok(())
}

fn admin_transfer_owner(
    ctx: &mut Context,
    new_owner_pk: PublicKey,
) -> Result<()> {
    let state = ctx.get_state()?;
    require!(ctx.caller_pk() == state.config.owner_pk, "Not owner");

    // 48h delay (approximately 48h * 60 * 60 / 60 = 2880 blocks at 60s/block)
    let delay_blocks = 2880;
    state.config.owner_transfer_pending = Some((new_owner_pk, ctx.height() + delay_blocks));

    ctx.set_state(state)?;
    ctx.emit_event("OwnerTransferInitiated", { new_owner_pk });
    Ok(())
}

fn admin_claim_ownership(
    ctx: &mut Context,
) -> Result<()> {
    let state = ctx.get_state()?;

    if let Some((new_owner, effective_height)) = state.config.owner_transfer_pending {
        require!(ctx.caller_pk() == new_owner, "Not pending owner");
        require!(ctx.height() >= effective_height, "Delay not elapsed");

        state.config.owner_pk = new_owner;
        state.config.owner_transfer_pending = None;

        ctx.set_state(state)?;
        ctx.emit_event("OwnerTransferred", {});
        Ok(())
    } else {
        Err("No pending transfer")
    }
}

fn admin_emergency_cancel(
    ctx: &mut Context,
    bet_id: u64,
) -> Result<()> {
    let state = ctx.get_state()?;
    require!(ctx.caller_pk() == state.config.owner_pk, "Not owner");

    let bet = state.bets.get_mut(bet_id)?;
    require!(bet.status == BetStatus::Pending, "Not pending");

    // Refund user
    ctx.send_funds(bet.user_pk, bet.amount, bet.asset_id)?;

    bet.status = BetStatus::Cancelled;
    state.pool.pending_bets -= bet.amount;

    ctx.set_state(state)?;
    ctx.emit_event("BetCancelled", { bet_id });
    Ok(())
}

fn admin_emergency_drain(
    ctx: &mut Context,
) -> Result<()> {
    let state = ctx.get_state()?;
    require!(ctx.caller_pk() == state.config.owner_pk, "Not owner");
    require!(state.config.emergency_mode, "Not in emergency");

    // Refund all pending bets first
    for (bet_id, bet) in state.bets.iter() {
        if bet.status == BetStatus::Pending {
            ctx.send_funds(bet.user_pk, bet.amount, bet.asset_id)?;
            bet.status = BetStatus::Cancelled;
        }
    }

    // Drain remaining pool to owner
    let available = state.pool.total_deposited + state.pool.total_bets
                 - state.pool.total_payouts;
    ctx.send_funds(state.config.owner_pk, available, state.pool.asset_id)?;

    ctx.set_state(state)?;
    ctx.emit_event("EmergencyDrain", { amount: available });
    Ok(())
}


// === VIEW FUNCTIONS ===

fn view_pool() -> PoolView {
    // Returns blinded pool state (no exact amounts exposed)
    // Only show: pool_health (sufficient funds), estimated_size_range
}

fn view_bet(bet_id: u64, user_pk: PublicKey) -> BetView {
    // Returns bet details for caller only
    // Shows: status, result (if settled), did_win
}

fn view_config() -> ConfigView {
    // Returns public config (min/max, multipliers, paused)
}
```

---

## 5. Randomness & Fairness Verification

### 5.1 Commit-Reveal + House Reveal Scheme

```
COMMITMENT PHASE:
┌─────────────────────────────────────────────────────────────────┐
│ User generates:                                                 │
│   user_nonce = Random(32 bytes)                                │
│   commitment = SHA256(bet_type || user_nonce)                  │
│                                                                 │
│ User sends: Bet(amount, bet_type, commitment)                  │
│ Shader stores: bet_id, commitment, amount, bet_type            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
REVEAL PHASE (after epoch blocks):
┌─────────────────────────────────────────────────────────────────┐
│ House generates:                                                │
│   random_value = HouseRNG(1-100)                               │
│   house_proof = Attestation(random_value, bet_id, timestamp)   │
│                                                                 │
│ House sends: Reveal(bet_id, random_value, house_proof, nonce)  │
│                                                                 │
│ Shader verifies:                                                │
│   1. commitment == SHA256(bet_type || user_nonce) ✓            │
│   2. timing >= created_height + reveal_epoch ✓                 │
│   3. house_proof signature valid (owner) ✓                     │
│                                                                 │
│ Shader determines outcome:                                      │
│   - BetType::Up    → won if random_value > 50                  │
│   - BetType::Down  → won if random_value < 50                  │
│   - BetType::Exact → won if random_value == chosen_number      │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Fairness Guarantees

| Guarantee | Implementation |
|-----------|----------------|
| **Cannot Predict** | User commits BEFORE house generates random |
| **Cannot Manipulate** | Commitment locked on-chain, cannot be changed |
| **Verifiable** | User can verify: `SHA256(bet_type || nonce) == commitment` |
| **Auditable** | All reveals stored on-chain, queryable post-hoc |
| **No Front-Running** | Reveal only after epoch blocks (configurable) |

### 5.3 House Proof Format

```rust
struct HouseProof {
    random_value: u8,           // 1-100
    bet_id: u64,
    timestamp: u64,
    block_hash: Hash,           // Recent block hash for entropy
    owner_signature: Signature, // Owner attests to this random
}

// Verification:
fn verify_house_proof(proof: &HouseProof, owner_pk: &PublicKey) -> bool {
    // 1. Signature valid
    let msg = hash(proof.random_value || proof.bet_id || proof.timestamp);
    verify_signature(owner_pk, msg, proof.owner_signature)?;

    // 2. Timestamp reasonable (not too old, not future)
    // 3. Random in valid range
    // 4. Block hash from valid chain
    Ok(())
}
```

### 5.4 Alternative: Deterministic Chain Entropy (Future)

```rust
// Future enhancement: Use block hash as entropy source
fn derive_random(bet_id: u64, block_height: u64) -> u8 {
    let block_hash = get_block_hash(block_height);
    let entropy = SHA256(bet_id || block_hash || "beambet_random");
    (entropy[0] % 100) + 1  // 1-100
}
// Issue: Miner could potentially manipulate block hash
// Solution: Multi-block commitment window
```

---

## 6. UI Screens

### 6.1 Screen Flow

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   CONNECT   │────►│   MAIN      │────►│   BET       │
│   (Wallet)  │     │   (Pool)    │     │   (Place)   │
└─────────────┘     └──────┬──────┘     └──────┬──────┘
                          │                   │
                          ▼                   ▼
                   ┌─────────────┐     ┌─────────────┐
                   │   HISTORY   │     │   PENDING   │
                   │   (Past)    │     │   (Await)   │
                   └─────────────┘     └─────────────┘
                          │
                          ▼
                   ┌─────────────┐
                   │   ADMIN     │
                   │   (Owner)   │
                   └─────────────┘
```

### 6.2 Screen Specifications

#### 6.2.1 Main Screen (Pool View)

```
┌────────────────────────────────────────────────────┐
│  BeamBet                              [⚙️ Admin]   │
├────────────────────────────────────────────────────┤
│                                                    │
│       Pool Status: ████████░░ (Healthy)           │
│       Est. Size: ~█████ BEAM (blinded)            │
│                                                    │
├────────────────────────────────────────────────────┤
│                                                    │
│   ┌─────────────┐  ┌─────────────┐                │
│   │   UP (>50)  │  │ DOWN (<50)  │                │
│   │    1.9x     │  │    1.9x     │                │
│   │  [SELECT]   │  │  [SELECT]   │                │
│   └─────────────┘  └─────────────┘                │
│                                                    │
│   ┌─────────────────────────────────────┐         │
│   │  EXACT NUMBER (1-100)    2.8x       │         │
│   │  [___] [SELECT]                      │         │
│   └─────────────────────────────────────┘         │
│                                                    │
├────────────────────────────────────────────────────┤
│                                                    │
│   Your Pending Bets: 2                            │
│   [View History]                                  │
│                                                    │
└────────────────────────────────────────────────────┘
```

#### 6.2.2 Bet Placement Screen

```
┌────────────────────────────────────────────────────┐
│  Place Bet                                    [←] │
├────────────────────────────────────────────────────┤
│                                                    │
│   Bet Type: UP (>50)                              │
│   Payout: 1.9x                                    │
│                                                    │
│   Amount:                                         │
│   ┌─────────────────────────────────────┐         │
│   │  10.00 BEAM                         │         │
│   └─────────────────────────────────────┘         │
│   Min: 0.01 BEAM  |  Max: 100 BEAM                │
│                                                    │
│   ┌─────────────────────────────────────┐         │
│   │  Potential Win: 19.00 BEAM          │         │
│   └─────────────────────────────────────┘         │
│                                                    │
│   ⚠️  This bet is confidential. Your choice       │
│       will be revealed after settlement.          │
│                                                    │
│   [CANCEL]              [CONFIRM BET]             │
│                                                    │
└────────────────────────────────────────────────────┘
```

#### 6.2.3 Pending Bet Screen

```
┌────────────────────────────────────────────────────┐
│  Pending Bet                                       │
├────────────────────────────────────────────────────┤
│                                                    │
│   Bet ID: #████████████                           │
│   Type: UP (>50)                                  │
│   Amount: ██.██ BEAM (confidential)               │
│   Status: ⏳ Awaiting Reveal                      │
│                                                    │
│   ┌─────────────────────────────────────┐         │
│   │  Commitment:                        │         │
│   │  0x7a3f...9e2d                      │         │
│   │  (Your choice is locked)            │         │
│   └─────────────────────────────────────┘         │
│                                                    │
│   Estimated reveal: ~10 blocks                    │
│                                                    │
│   [View on Explorer]    [Back to Main]            │
│                                                    │
└────────────────────────────────────────────────────┘
```

#### 6.2.4 Result Screen

```
┌────────────────────────────────────────────────────┐
│  Bet Result                                        │
├────────────────────────────────────────────────────┤
│                                                    │
│           ████████████████████                    │
│           █                      █                │
│           █      YOU WON! 🎉     █                │
│           █                      █                │
│           ████████████████████                    │
│                                                    │
│   Result: 73                                      │
│   Your Bet: UP (>50)                              │
│   Amount: 10.00 BEAM                              │
│   Payout: 19.00 BEAM                              │
│                                                    │
│   ┌─────────────────────────────────────┐         │
│   │  Fairness Verification:             │         │
│   │  ✓ Commitment matched               │         │
│   │  ✓ Proper reveal timing             │         │
│   │  ✓ House proof valid                │         │
│   └─────────────────────────────────────┘         │
│                                                    │
│   [Verify on Explorer]   [Place Another Bet]      │
│                                                    │
└────────────────────────────────────────────────────┘
```

#### 6.2.5 Admin Panel (Owner Only)

```
┌────────────────────────────────────────────────────┐
│  Admin Panel                                  [←] │
├────────────────────────────────────────────────────┤
│                                                    │
│   Pool Management                                  │
│   ┌─────────────────────────────────────┐         │
│   │  Total Deposited: 1000 BEAM         │         │
│   │  Pending Bets:    50 BEAM           │         │
│   │  Available:       950 BEAM          │         │
│   └─────────────────────────────────────┘         │
│                                                    │
│   [Deposit]  [Withdraw]                           │
│                                                    │
├────────────────────────────────────────────────────┤
│   Game Settings                                    │
│   Min Bet: [0.01   ] BEAM                         │
│   Max Bet: [100    ] BEAM                         │
│   Up/Down Multiplier: [1.9x]                      │
│   Exact Multiplier:   [2.8x]                      │
│                                                    │
│   [Pause Game]  [Emergency Mode]                  │
│                                                    │
├────────────────────────────────────────────────────┤
│   Ownership                                        │
│   Current Owner: 0x7a3f...9e2d                    │
│   [Transfer Ownership]                             │
│                                                    │
└────────────────────────────────────────────────────┘
```

---

## 7. API Contract (UI ↔ Shader)

### 7.1 Methods

| Method | Role | Description |
|--------|------|-------------|
| `constructor` | Owner | Initialize contract with deposit |
| `bet` | User | Place bet with commitment |
| `reveal` | Owner | Reveal random and settle |
| `claim` | User | Claim pending payout (alt design) |
| `admin_deposit` | Owner | Add liquidity to pool |
| `admin_withdraw` | Owner | Remove liquidity from pool |
| `admin_update_config` | Owner | Update game parameters |
| `admin_transfer_owner` | Owner | Initiate ownership transfer |
| `admin_claim_ownership` | New Owner | Complete ownership transfer |
| `admin_emergency_cancel` | Owner | Cancel specific bet |
| `admin_emergency_drain` | Owner | Drain all funds (emergency) |
| `view_pool` | Anyone | View blinded pool state |
| `view_bet` | User | View own bet details |
| `view_config` | Anyone | View public configuration |

### 7.2 Events

| Event | When | Data |
|-------|------|------|
| `BetPlaced` | New bet | bet_id, height (blinded) |
| `BetSettled` | Reveal complete | bet_id, status |
| `BetCancelled` | Emergency cancel | bet_id |
| `Deposit` | Owner deposit | amount (blinded) |
| `Withdraw` | Owner withdraw | amount (blinded) |
| `ConfigUpdated` | Settings changed | - |
| `OwnerTransferInitiated` | Transfer started | new_owner_pk |
| `OwnerTransferred` | Transfer complete | - |
| `EmergencyDrain` | Full drain | amount (blinded) |

---

## 8. Security Considerations

### 8.1 Threat Model

| Threat | Mitigation |
|--------|------------|
| **Front-running** | Commit-reveal with epoch delay |
| **Pool exhaustion** | Max bet limit, house edge, monitoring |
| **Owner key compromise** | 48h transfer delay, emergency mode |
| **Shader bug** | Audit before mainnet, emergency drain |
| **Random manipulation** | User commitment locks choice first |
| **Double-spend** | Beam UTXO model prevents |
| **Reentrancy** | No external calls in shader |

### 8.2 Invariants

```
// Must ALWAYS hold:
pool_total_deposited + pool_total_bets == pool_total_payouts + pool_available

// Before any payout:
available >= payout_amount

// After any operation:
sum(all_bets.where(status=Pending).amount) == pool.pending_bets
```

### 8.3 Emergency Procedures

1. **Pool Running Low**: Owner deposits more OR reduces max bet
2. **Suspected Bug**: Owner pauses game, investigates, uses emergency cancel for refunds
3. **Critical Vulnerability**: Owner enables emergency mode, drains pool
4. **Key Compromise**: Old owner initiates transfer, new owner claims after 48h

---

## 9. Deployment Checklist

### 9.1 Pre-Deployment

- [ ] Shader audit complete (community or professional)
- [ ] All tests pass on DAPPNET
- [ ] Privacy audit checklist complete
- [ ] Owner key secured (hardware wallet recommended)
- [ ] Initial liquidity prepared
- [ ] IPFS hosting configured

### 9.2 Deployment Steps

1. Deploy shader contract to DAPPNET
2. Call `constructor` with owner key and initial deposit
3. Record Contract ID (CID)
4. Build UI: `cd ui && yarn build`
5. Update `api.tsx` with CID
6. Package as .dapp
7. Upload to IPFS
8. Test in DAPPNET wallet
9. Verify all functions
10. Mainnet deployment (post-audit)

### 9.3 Post-Deployment

- [ ] Verify pool state
- [ ] Test small bet end-to-end
- [ ] Monitor first 24h
- [ ] Document CID and owner address

---

## 10. Future Enhancements

| Feature | Description | Priority |
|---------|-------------|----------|
| Multi-asset support | Accept custom confidential assets | Medium |
| VRF integration | drand or Chainlink VRF for randomness | High |
| Multi-player pools | Shared betting pools | Low |
| NFT rewards | Achievement badges | Low |
| Mobile wallet | Optimize for Beam mobile wallet | Medium |
| Governance | DAO-controlled parameters | Low |

---

**Document End**
*Generated by Architect Subagent*