## DApp Patterns & Templates

### Pattern 1: Token/Vault Contract

```cpp
// Simple vault that holds funds for a user
namespace Vault {
    struct State {
        PubKey m_OwnerPk;
        uint64_t m_Balance;
        AssetID m_AssetId;
        uint8_t m_Initialized;
    };

    namespace Method {
        struct Deposit {
            static const uint32_t s_iMethod = 2;
            uint64_t m_Amount;
        };
        struct Withdraw {
            static const uint32_t s_iMethod = 3;
            uint64_t m_Amount;
        };
    }
}
```

### Pattern 2: Escrow Contract

```cpp
// Two-party escrow with timeout
namespace Escrow {
    struct State {
        PubKey m_SellerPk;
        PubKey m_BuyerPk;
        uint64_t m_Amount;
        AssetID m_AssetId;
        Height m_TimeoutHeight;
        uint8_t m_Status; // 0=pending, 1=released, 2=refunded
    };

    namespace Method {
        struct Create { static const uint32_t s_iMethod = 2; /* ... */ };
        struct Release { static const uint32_t s_iMethod = 3; /* buyer releases */ };
        struct Refund { static const uint32_t s_iMethod = 4; /* seller claims after timeout */ };
    }
}
```

---

## CRITICAL: Beam Wallet API (CORRECT PATTERNS)

### Desktop Wallet Connection (Qt WebChannel) - CORRECT WAY

The Beam Desktop Wallet uses Qt WebChannel. **Use JSON-RPC string format:**

```typescript
// 1. Inject Qt WebChannel script
await injectScript('qrc:///qtwebchannel/qwebchannel.js');

// 2. Connect to channel
new QWebChannel(qt.webChannelTransport, (channel) => {
  const api = channel.objects.BEAM.api;

  // Connect to result signal
  api.callWalletApiResult.connect((result) => {
    handleApiResult(result);
  });
});

// 3. Call API - MUST use JSON-RPC string format
const request = {
  jsonrpc: '2.0',
  id: 'call-id',
  method: 'invoke_contract',
  params: { args: 'role=user,action=view_params', create_tx: false }
};
api.callWalletApi(JSON.stringify(request));  // <-- MUST stringify!
```

### API Response Handling

```typescript
// Handle API results
function handleApiResult(json: string) {
  const answer = JSON.parse(json);

  // For invoke_contract with raw_data, return full result
  if (answer.result?.raw_data !== undefined) {
    return answer.result;  // Returns { output, raw_data, txid }
  }

  // For view calls, parse output
  if (answer.result?.output !== undefined) {
    return JSON.parse(answer.result.output);
  }

  return answer.result;
}
```

### Two-Step Transaction Pattern

**CRITICAL: Transactions require two API calls!**

```typescript
// Step 1: Call invoke_contract with create_tx: false
const result = await callWalletApi('invoke_contract', {
  args: 'role=user,action=place_bet,cid=CONTRACT_ID,amount=1000000,...',
  create_tx: false,  // ALWAYS false!
  contract: shaderBytes  // Only needed for first call
});

// Step 2: Process the raw_data to create transaction
if (result.raw_data) {
  const txResult = await callWalletApi('process_invoke_data', {
    data: result.raw_data  // Array of bytes
  });
  // User will be prompted to sign transaction
}
```

### API Methods Reference

| Method | Description |
|--------|-------------|
| `invoke_contract` | Call shader method (always use create_tx: false) |
| `process_invoke_data` | Create transaction from raw_data (for state changes) |
| `ev_subunsub` | Subscribe to wallet events |
| `wallet_status` | Get wallet status |

### invoke_contract Parameters

```typescript
{
  args: 'role=user,action=view_params,cid=CONTRACT_ID',
  create_tx: false,  // ALWAYS false - use process_invoke_data for transactions
  contract: [...],   // Shader bytes (first call only)
}
```

### process_invoke_data Parameters

```typescript
{
  data: [129, 130, 176, ...]  // raw_data array from invoke_contract
}
```

---

## Complete API Implementation

### Core Wallet API (beam.ts)

```typescript
// Call Wallet API - CORRECT implementation
export async function callWalletApi(method: string, params: any = {}): Promise<any> {
  const callId = `call-${CallID++}-${method}`;

  return new Promise((resolve, reject) => {
    Calls[callId] = { resolve, reject };

    // MUST use JSON-RPC string format for desktop wallet
    const request = {
      jsonrpc: '2.0',
      id: callId,
      method,
      params,
    };

    console.log('API Request:', request);
    BEAM.api.callWalletApi(JSON.stringify(request));
  });
}

// Invoke contract for VIEW calls
export async function invokeContract(args: string, withShader: boolean = false): Promise<any> {
  return callWalletApi('invoke_contract', {
    args,
    create_tx: false,  // Always false
    ...(withShader && { contract: ShaderBytes })
  });
}

// Invoke contract for STATE-CHANGING calls (creates transaction)
export async function invokeContractWithTx(args: string, withShader: boolean = false): Promise<any> {
  // Step 1: Get contract data
  const result = await invokeContract(args, withShader);

  // Step 2: Process raw_data to create transaction
  if (result?.raw_data) {
    return callWalletApi('process_invoke_data', { data: result.raw_data });
  }

  return result;
}

// Shader helper
export async function callShader(
  role: string,
  action: string,
  params: Record<string, string | number>,
  includeShader: boolean = false,
  skipContractId: boolean = false,
  createTx: boolean = false
): Promise<any> {
  const argsParts: string[] = [];

  if (CurrentContractId && !skipContractId) {
    argsParts.push(`cid=${CurrentContractId}`);
  }

  for (const [key, value] of Object.entries(params)) {
    if (value !== undefined && value !== '') {
      argsParts.push(`${key}=${value}`);
    }
  }

  const fullArgs = `role=${role},action=${action},${argsParts.join(',')}`;

  if (createTx) {
    return invokeContractWithTx(fullArgs, includeShader);
  } else {
    return invokeContract(fullArgs, includeShader);
  }
}
```

### Example: Place Bet (State-Changing)

```typescript
export async function placeBet(amount: number, betType: string): Promise<void> {
  // This creates a transaction (createTx: true)
  await callShader('user', 'place_bet', {
    amount,
    bet_type: betType === 'up' ? 0 : 1,
    commitment: '...',
  }, false, false, true);  // <-- createTx = true
}

// View calls don't need createTx
export async function viewParams(): Promise<ContractParams> {
  return callShader('user', 'view_params', {});  // <-- createTx = false (default)
}
```

---

## Common Mistakes to Avoid

### ❌ WRONG: Using create_tx: true
```typescript
// This will FAIL with "Applications must set create_tx to false"
await callWalletApi('invoke_contract', {
  args: '...',
  create_tx: true  // ❌ WRONG!
});
```

### ✅ CORRECT: Two-step pattern
```typescript
// Step 1: Get raw_data
const result = await callWalletApi('invoke_contract', {
  args: '...',
  create_tx: false  // ✅ Always false
});

// Step 2: Process raw_data
if (result.raw_data) {
  await callWalletApi('process_invoke_data', { data: result.raw_data });
}
```

### ❌ WRONG: Not stringifying request
```typescript
api.callWalletApi(callId, method, params);  // ❌ WRONG
```

### ✅ CORRECT: JSON-RPC string
```typescript
api.callWalletApi(JSON.stringify({
  jsonrpc: '2.0',
  id: callId,
  method,
  params
}));  // ✅ CORRECT
```

### ❌ WRONG: Wrong method name
```typescript
await callWalletApi('process_contract_data', { data });  // ❌ WRONG
```

### ✅ CORRECT: process_invoke_data
```typescript
await callWalletApi('process_invoke_data', { data });  // ✅ CORRECT
```

---

## Local Storage for Pending Bets

Track bets locally since the shader may not return bet_id:

```typescript
export interface PendingBet {
  id: string;           // Local ID
  betId?: number;       // On-chain bet ID
  amount: number;       // In groth
  amountBeam: number;   // In BEAM
  type: 'up' | 'down' | 'exact';
  exactNumber?: number;
  commitment: string;
  nonce: string;
  createdHeight?: number;
  createdTime: number;
  status: 'pending' | 'won' | 'lost' | 'claimed';
  result?: number;
  payout?: number;
}

// Get bet_id from pool.total_bets BEFORE placing bet
const pool = await viewPool();
const betId = pool.total_bets;  // This will be our bet ID

// Place bet
const result = await placeBet(amount, type);

// Save locally
savePendingBet({
  id: `bet-${Date.now()}`,
  betId,
  ...
});
```

---

## Reveal Timing

```typescript
const REVEAL_EPOCH = 10;  // blocks until reveal available

// Check if bet can be revealed
const blocksRemaining = (betCreatedHeight + REVEAL_EPOCH) - currentHeight;
if (blocksRemaining <= 0) {
  // Can reveal now
}

// Public reveal (trustless - uses block hash)
await publicReveal(betId);

// Result is derived from: SHA256(commitment + revealHeight + betId) % 100 + 1
```

---

## DApp Package Structure

The `.dapp` file is a ZIP archive:

```
beambet.dapp
├── manifest.json     # DApp metadata
└── app/
    ├── index.html    # Entry point
    ├── icon.svg      # DApp icon (SVG only, max 10KB)
    ├── app.wasm      # App shader
    └── static/
        ├── css/
        └── js/
```

### manifest.json Format

```json
{
  "name": "BeamBet",
  "description": "Privacy-focused betting on Beam",
  "url": "localapp/app/index.html",
  "version": "1.0.0",
  "api_version": "7.0",
  "min_api_version": "7.0",
  "icon": "localapp/app/icon.svg",
  "guid": "unique-dapp-identifier-here"
}
```

---

## Build Commands

```bash
# Build shader
cd shaders/beambet && build.bat

# Generate ShaderID (after first compile)
generate-sid.exe contract.wasm

# Build UI
cd ui && yarn build

# Test shader
beam-wallet-dappnet shader --shader_app_file app.wasm

# Deploy contract
beam-wallet-dappnet shader \
    --shader_app_file app.wasm \
    --shader_contract_file contract.wasm \
    --shader_args "role=manager,action=create_contract"
```

---