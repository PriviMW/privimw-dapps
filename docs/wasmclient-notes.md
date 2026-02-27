# Beam wasmclient Research Notes

**Source**: Sparse clone of https://github.com/BeamMW/beam (commit 7c84a61)
**Date researched**: 2026-02-27
**Files studied**:
- `wasmclient/wasmclient.cpp` — main WASM client, Emscripten JS bindings
- `wasmclient/wasm_beamapi.cpp / .h` — WasmAppApi class (per-DApp API instance)
- `wallet/client/apps_api/apps_api.h` — AppsApi template, the core DApp API logic
- `wallet/api/v6_0/v6_api_defs.h` — InvokeContract + ProcessInvokeData struct defs
- `wallet/api/v6_0/v6_api_parse.cpp` — How process_invoke_data parses its `data` param
- `wallet/api/v6_0/v6_api_handle.cpp` — How invoke_contract and process_invoke_data are executed
- `wallet/api/v6_1/v6_1_api_handle.cpp` — v6.1 version of same (adds priority/unique params)
- `wallet/api/v6_1/v6_1_api_parse.cpp` — v6.1 parse, confirms raw_data is ByteBuffer serialized as JSON array
- `wallet/api/base/contracts.rest` — REST example showing process_invoke_data format
- `android/dao/apps_api_ui.cpp` — Android DApp bridge (confirms callWalletApi pattern)
- `dex-app/src/app/core/utils.js` — **Official DApp reference** (on local disk)

---

## 1. Architecture Overview

Beam DApps communicate with the wallet through a **JSON-RPC API**. There are three transport mechanisms depending on platform:

```
Desktop Wallet (Qt)    →  QWebChannel (qt.webChannelTransport)
                           channel.objects.BEAM.api.callWalletApi(jsonString)
                           channel.objects.BEAM.api.callWalletApiResult.connect(cb)

Mobile (Android/iOS)   →  window.BEAM.callWalletApi(jsonString)
                           window.BEAM.callWalletApiResult(cb)

Web (Browser Extension) → window.BeamApi.callWalletApi(callId, method, params)
                           window.BeamApi.callWalletApiResult(cb)

WASM (headless browser) → WasmWalletClient + AppAPI objects
                           api.callWalletApi(jsonString)
                           api.setHandler(cb)
```

All platforms share the same JSON-RPC protocol. The only difference is how the JSON string is transported.

---

## 2. The Official Two-Step Contract Invocation Pattern

This is the **definitive, confirmed-from-source** pattern for DApp contract calls that modify state:

### Step 1: `invoke_contract` with `create_tx: false`

```json
{
  "jsonrpc": "2.0",
  "id": "call-0-invoke_contract",
  "method": "invoke_contract",
  "params": {
    "contract": [/* byte array of app.wasm, sent ONLY on first call */],
    "args": "role=user,action=place_bet,cid=CID,...",
    "create_tx": false
  }
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "id": "call-0-invoke_contract",
  "result": {
    "output": "{\"some\": \"json from shader\"}",
    "raw_data": [123, 45, 67, ...]
  }
}
```

- `output` is a JSON string produced by the app shader (Env::DocAddText etc.)
- `raw_data` is a **byte array** (JSON array of uint8 values) representing a serialized `beam::bvm2::ContractInvokeData` struct

**Critical enforcement**: When called from a DApp (via AppsApi), `create_tx: true` is **blocked** by the wallet with error: `"Applications must set create_tx to false and use process_contract_data"`. This is enforced in both v6.0 and v6.1 parse code.

### Step 2: `process_invoke_data` with the raw bytes

```json
{
  "jsonrpc": "2.0",
  "id": "call-1-process_invoke_data",
  "method": "process_invoke_data",
  "params": {
    "data": [123, 45, 67, ...]
  }
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "id": "call-1-process_invoke_data",
  "result": {
    "txid": "abc123..."
  }
}
```

This call triggers the wallet's user consent UI (the confirmation dialog). The user must read and approve before the response arrives. This is why a **5-minute timeout** is needed for this call specifically.

---

## 3. What `raw_data` Actually Is

Source: `wallet/api/v6_0/v6_api_parse.cpp` lines 905-920 and `v6_api_parse.cpp` lines 1051-1054.

**`raw_data` is a serialized `beam::bvm2::ContractInvokeData` struct**, sent as a JSON array of uint8 byte values.

On the server side:
```cpp
// From getResponse() for InvokeContract:
if (res.invokeData)
{
    msg["result"]["raw_data"] = *res.invokeData;  // ByteBuffer serialized as uint8 array
}

// From onParseProcessInvokeData():
const json bytes = getMandatoryParam<NonEmptyJsonArray>(params, "data");
message.invokeData = bytes.get<std::vector<uint8_t>>();

beam::bvm2::ContractInvokeData realData;
if (!beam::wallet::fromByteBuffer(message.invokeData, realData))
{
    throw jsonrpc_exception(ApiError::InvalidParamsJsonRpc, "Failed to parse invoke data");
}
```

This means:
- The wallet serializes `ContractInvokeData` into a `ByteBuffer` (raw bytes)
- JSON serialization of `ByteBuffer` produces a JSON array of unsigned integers `[0..255]`
- The DApp takes `result.raw_data` (this array) and passes it **directly** to `process_invoke_data.data`
- There is NO re-encoding, transformation, or base64 — it is a direct passthrough of the byte array

The `ContractInvokeData` struct contains, per each invocation entry:
- `m_Cid` — Contract ID
- `m_iMethod` — Method number
- `m_Charge` — BVM compute charge
- `m_sComment` — Human-readable description shown in wallet dialog
- `m_Spend` — Asset amounts spent/received

---

## 4. The "Apps Allowed" Enforcement

From `wallet/client/apps_api/apps_api.h`, the `AnyThread_callWalletApiChecked()` method:

```cpp
if (acinfo.method == "tx_send")
{
    // Requires user consent (spend consent)
    CheckInfo info{true, *pres};
    return info;
}

if (acinfo.method == "process_invoke_data")
{
    // Requires user consent (contract consent)
    CheckInfo info{false, *pres};
    return info;
}

if (pres->minfo.fee > 0 || !pres->minfo.spend.empty())
{
    // Any other method spending funds: blocked
    AnyThread_sendApiError(request, ApiError::NotAllowedError, std::string());
    return {};
}

// Safe read-only methods: execute directly
_walletAPI->executeAPIRequest(request.c_str(), request.size());
```

DApps can ONLY call:
- `invoke_contract` (with `create_tx: false`) — view calls and shader execution
- `process_invoke_data` — submits a transaction (triggers consent dialog)
- `tx_send` — plain BEAM sends (triggers consent dialog)
- All read-only API methods (`wallet_status`, `tx_list`, `get_version`, `ev_subunsub`, etc.)

DApps CANNOT call:
- `invoke_contract` with `create_tx: true` (explicitly blocked)
- Any method that spends funds without going through the consent flow

---

## 5. The "Prime Once" Shader Bytes Pattern

Source: `dex-app/src/app/core/utils.js` `invokeContract()` function and MEMORY.md critical lesson #1.

The `contract` parameter (the app.wasm bytes as uint8 array) should be sent on the **first `invoke_contract` call only**. Once the wallet has processed the shader, it caches it. Subsequent calls omit the `contract` parameter.

**Official dex-app pattern**:
```js
static invokeContract(args, cback, bytes) {
  let params = { create_tx: false };
  if (args) { params = { args: assign, ...params }; }
  if (bytes) { params = { contract: bytes, ...params }; }  // Only when bytes provided
  return Utils.callApi('invoke_contract', params, cback);
}
```

The dex-app downloads the wasm file once via XHR as an arraybuffer, converts to `Array.from(new Uint8Array(buffer))`, and passes that array as `contract`.

**Why "prime once"**: Multiple concurrent calls with shader bytes cause the wallet to hang silently. The wallet's internal shader loading is not concurrent-safe. Always send bytes on exactly the first call, then omit.

---

## 6. The `callWalletApi` JSON-RPC Format

All platforms use standard JSON-RPC 2.0:

```js
const request = {
  jsonrpc: '2.0',
  id: 'call-0-method_name',   // Unique string ID
  method: 'invoke_contract',  // Method name
  params: {                   // Method params
    args: 'role=user,action=view,cid=...',
    create_tx: false,
  }
};
api.callWalletApi(JSON.stringify(request));
```

The `id` field is echoed back in the response, allowing the DApp to match responses to requests.

---

## 7. How the Desktop Wallet Bridge Works (Qt WebChannel)

Source: `dex-app/src/app/core/utils.js` `createDesktopAPI()`.

The desktop Beam Wallet exposes a Qt WebChannel object at `qt.webChannelTransport`. The standard Qt WebChannel.js script (served at `qrc:///qtwebchannel/qwebchannel.js`) is used to connect:

```js
await Utils.injectScript('qrc:///qtwebchannel/qwebchannel.js');
new QWebChannel(qt.webChannelTransport, (channel) => {
  channel.objects.BEAM.api.callWalletApiResult.connect(apirescback);
  resolve({
    api: channel.objects.BEAM.api,
    styles: channel.objects.BEAM.style,
  });
});
```

- `channel.objects.BEAM.api.callWalletApi(jsonString)` — sends a request
- `channel.objects.BEAM.api.callWalletApiResult.connect(callback)` — registers response handler

This is a Qt signal/slot connection. The signal fires every time the wallet sends a response.

---

## 8. The Official `handleApiResult` Response Parsing Pattern

Source: `dex-app/src/app/core/utils.js` `handleApiResult()`.

The official pattern always parses `result.output` FIRST (before checking `raw_data`), because shader errors appear in `output` even for transaction-creating calls:

```js
static handleApiResult(json) {
  const answer = JSON.parse(json);
  const { id } = answer;
  const call = Calls[id];

  if (answer.error) {
    return cback(answer);
  }

  if (typeof answer.result === 'undefined') {
    return cback({ error: 'no valid api call result', answer });
  }

  if (typeof answer.result.output === 'string') {
    // ALWAYS parse output first — shader errors come here
    const shaderAnswer = JSON.parse(answer.result.output);
    if (shaderAnswer.error) {
      return cback({ error: shaderAnswer.error, answer, request });
    }
    return cback(null, shaderAnswer, answer, request);  // success
  }

  // Non-shader responses (wallet_status, process_invoke_data, etc.)
  return cback(null, answer.result, answer, request);
}
```

**Critical**: The `raw_data` field is NOT returned as part of the `shaderAnswer`. The official dex-app has a separate helper `invokeContractAsyncAndMakeTx` that explicitly reads `full.result.raw_data`:

```js
static async invokeContractAsyncAndMakeTx(args) {
  const { full } = await Utils.invokeContractAsync(args);
  Utils.ensureField(full.result, 'raw_data', 'array');
  const { res } = await Utils.callApiAsync('process_invoke_data', { data: full.result.raw_data });
  Utils.ensureField(res, 'txid', 'string');
  return res.txid;
}
```

Note: `full` is the complete JSON-RPC response object, and `full.result.raw_data` is the raw byte array. The current project's `beam.ts` handles this correctly by attaching `raw_data` to the parsed shader answer before resolving.

---

## 9. The WASM Client (for Browser/Headless Mode)

Source: `wasmclient/wasmclient.cpp`.

The WASM client exposes a `WasmWalletClient` class to JavaScript via Emscripten bindings. For DApps it creates a per-DApp `AppAPI` object:

```js
// Create a headless wallet client
const client = new WasmWalletClient(nodeUrl, WasmModule.Network.mainnet);
client.startWallet();

// Create the per-DApp API object
const appid = WasmWalletClient.GenerateAppID(appname, window.location.href);
client.createAppAPI(apiver, apivermin, appid, appname, (err, api) => {
  api.setHandler((result) => {
    // result is a JSON string — same format as desktop/mobile
    handleApiResult(result);
  });

  // Now call the API
  api.callWalletApi(JSON.stringify({
    jsonrpc: '2.0',
    id: 'my-call',
    method: 'wallet_status',
    params: {}
  }));
});
```

The WASM client also handles the consent flow internally:
- `setApproveContractInfoHandler` — called when `process_invoke_data` needs user consent
- `setApproveSendHandler` — called when `tx_send` needs user consent
- In headless mode these handlers are required; without them, transactions would be auto-rejected

---

## 10. API Version History Relevant to Shaders

- **v6.0**: Introduced `invoke_contract` and `process_invoke_data`
- **v6.1**: Added `priority` and `unique` params to `invoke_contract`; added `ev_subunsub` events; added `get_version`
- **v7.0**: Added IPFS methods, `sign_message`, `verify_signature`
- **v7.3**: Added `assets_list`

The Desktop DAPPNET wallet supports up to whatever version was compiled in. Use `get_version` to query. The current project uses `apiver: 'current'` and `apivermin: '7.0'`.

---

## 11. Confirmed: `raw_data` IS the Correct Format for `process_invoke_data`

To directly answer the original research question:

**Yes, the raw byte array is exactly correct for `process_invoke_data`.**

The wallet server-side code:
1. `invoke_contract` (no-TX path) calls `CallShader()` which runs the app shader
2. The app shader calls `Env::GenerateKernel()` which produces a `ContractInvokeData` buffer
3. This buffer is serialized to `ByteBuffer` and returned as `result.raw_data` (JSON uint8 array)
4. `process_invoke_data` receives this same JSON uint8 array in `params.data`
5. It deserializes it back into `ContractInvokeData` using `fromByteBuffer()`
6. Then calls `getContracts()->ProcessTxData(data.invokeData, ...)` to build and submit the transaction

There is no "opcode" or higher-level transaction building API. The raw byte array IS the transaction data (a serialized `ContractInvokeData` struct).

---

## 12. Error Shape from Wallet API

The wallet returns errors in two forms:

**JSON-RPC level error** (network/API issues):
```json
{ "jsonrpc": "2.0", "id": "...", "error": { "code": -32xxx, "message": "..." } }
```

**Shader-level error** (contract Halt() or logic error):
```json
{
  "jsonrpc": "2.0",
  "id": "...",
  "result": {
    "output": "{\"error\": \"some message\"}",
    "raw_data": null
  }
}
```

The shader error is embedded in `result.output` as a JSON string with an `error` key. This is why the response handler MUST parse `output` first.

User-cancelled consent:
```json
{ "error": { "code": -32021, "message": "User rejected" } }
```
Check `err.error && err.error.code == -32021`.

---

## 13. Comparison with Current Project's beam.ts

The current project's `/path/to/project correctly implements:

- Two-step pattern (`invokeContract` + `processContractData`)
- `create_tx: false` enforcement
- Prime-once shader bytes (`ShaderCached` flag)
- Output-first response parsing
- `raw_data` passthrough to `process_invoke_data`
- `TX_CONFIRM_TIMEOUT = 300000` for `process_invoke_data`
- Qt WebChannel connection for desktop

The project correctly reads `result.raw_data` from the full API response before the `handleApiResult` function strips it, by attaching `raw_data` to the parsed shader answer:

```typescript
if (answer.result.raw_data !== undefined) {
  shaderAnswer.raw_data = answer.result.raw_data;
}
```

This matches the intent of the official dex-app's `invokeContractAsyncAndMakeTx` which reads `full.result.raw_data` directly.

---

## 14. Summary: The Complete Flow for a State-Changing Contract Call

```
DApp JavaScript                           Beam Wallet
─────────────────────────────────────────────────────────────────────
1. callWalletApi('invoke_contract', {
     args: 'role=user,action=bet,...',
     create_tx: false,
     contract: [/* wasm bytes, first call only */]
   })
   ──────────────────────────────────────────>

                                          2. Runs app.wasm Method_1 dispatcher
                                             → Calls Env::GenerateKernel(...)
                                             → Produces ContractInvokeData buffer
                                             → Serializes to ByteBuffer

   <──────────────────────────────────────────
3. Response arrives:
   result.output = '{"ok":1}'
   result.raw_data = [1, 2, 3, ...]  ← serialized ContractInvokeData

4. Parse output, check for shader errors
   Extract raw_data

5. callWalletApi('process_invoke_data', {
     data: raw_data   ← same byte array
   })
   ──────────────────────────────────────────>

                                          6. Deserializes ContractInvokeData
                                             Shows consent dialog to user
                                             (user sees: fee, amounts, comment)
                                             User clicks CONFIRM

                                          7. Builds actual blockchain transaction
                                             Signs with wallet keys
                                             Submits to node

   <──────────────────────────────────────────
8. Response: { txid: "abc123..." }
```
