// Core Beam Wallet API — Qt WebChannel + JSON-RPC 2.0
// This is the communication bridge between the DApp UI and the Beam Desktop Wallet.

declare global {
  interface Window {
    qt: { webChannelTransport: any };
    QWebChannel: any;
    BEAM: { api: any };
  }
}

// --- State ---
let BEAM_API: any = null;
let ShaderBytes: number[] | null = null;
let ShaderCached = false;
let CallID = 0;
let InitLock = false;

const API_TIMEOUT = 30000;        // 30s for view/query calls
const TX_CONFIRM_TIMEOUT = 300000; // 5min for process_invoke_data (wallet shows confirm dialog)

export const DEFAULT_CONTRACT_ID = '3e7eb498ec325a748211d5a015477b121e53c8764a9f0c119ae5971d271cbfeb';

interface PendingCall {
  resolve: (value: any) => void;
  reject: (reason: any) => void;
  timer: ReturnType<typeof setTimeout>;
}

const Calls: Record<string, PendingCall> = {};

// --- Error extraction ---
export function extractError(err: any): string {
  if (typeof err === 'string') return err;
  return err?.error || err?.message || err?.data?.message || 'Unknown error';
}

// --- Script injection ---
function injectScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const existing = document.querySelector(`script[src="${src}"]`);
    if (existing) { resolve(); return; }
    const script = document.createElement('script');
    script.src = src;
    script.onload = () => resolve();
    script.onerror = () => reject(new Error(`Failed to load script: ${src}`));
    document.head.appendChild(script);
  });
}

// --- Load shader bytes ---
async function loadShaderBytes(): Promise<void> {
  try {
    const response = await fetch('app.wasm');
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const buffer = await response.arrayBuffer();
    ShaderBytes = Array.from(new Uint8Array(buffer));
  } catch (err) {
    console.error('[beam] Failed to load shader:', err);
    throw new Error('Failed to load app.wasm shader');
  }
}

// --- Handle API results ---
function handleApiResult(json: string): void {
  let answer: any;
  try {
    answer = JSON.parse(json);
  } catch {
    console.error('[beam] Failed to parse API result:', json);
    return;
  }

  const callId = answer.id;
  const pending = Calls[callId];
  if (!pending) return;

  clearTimeout(pending.timer);
  delete Calls[callId];

  // JSON-RPC error
  if (answer.error) {
    pending.reject(answer.error);
    return;
  }

  const result = answer.result;

  // CRITICAL: Parse output FIRST — catches shader errors even on TX calls
  if (typeof result?.output === 'string') {
    try {
      const shaderAnswer = JSON.parse(result.output);

      if (shaderAnswer.error) {
        pending.reject({ error: shaderAnswer.error });
        return;
      }

      if (result.raw_data !== undefined) {
        shaderAnswer.raw_data = result.raw_data;
      }

      pending.resolve(shaderAnswer);
      return;
    } catch {
      pending.reject({ error: 'Failed to parse shader response' });
      return;
    }
  }

  // Non-string output (process_invoke_data, wallet_status, etc.)
  pending.resolve(result);
}

// --- Core API call ---
export function callWalletApi(method: string, params: any = {}, timeout?: number): Promise<any> {
  const callId = `call-${CallID++}-${method}`;
  const actualTimeout = timeout ?? (method === 'process_invoke_data' ? TX_CONFIRM_TIMEOUT : API_TIMEOUT);

  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      delete Calls[callId];
      reject({ error: `API call '${method}' timed out after ${actualTimeout / 1000}s` });
    }, actualTimeout);

    Calls[callId] = { resolve, reject, timer };

    const request = {
      jsonrpc: '2.0',
      id: callId,
      method,
      params,
    };

    BEAM_API.callWalletApi(JSON.stringify(request));
  });
}

// --- Initialize API ---
export async function initializeApi(): Promise<void> {
  if (BEAM_API) return;
  if (InitLock) throw new Error('API initialization already in progress');
  InitLock = true;

  try {
    await loadShaderBytes();
    await injectScript('qrc:///qtwebchannel/qwebchannel.js');

    await new Promise<void>((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('WebChannel connection timeout')), 10000);

      new window.QWebChannel(window.qt.webChannelTransport, (channel: any) => {
        clearTimeout(timeout);
        BEAM_API = channel.objects.BEAM.api;
        BEAM_API.callWalletApiResult.connect(handleApiResult);
        resolve();
      });
    });
  } finally {
    InitLock = false;
  }
}

// --- Invoke contract (view or pre-TX) ---
export async function invokeContract(args: string, withShader: boolean = false): Promise<any> {
  const params: any = { args, create_tx: false };

  if (withShader && ShaderBytes && !ShaderCached) {
    params.contract = ShaderBytes;
  }

  return callWalletApi('invoke_contract', params);
}

// --- Process contract data (step 2 of TX) ---
export async function processContractData(rawData: number[]): Promise<any> {
  return callWalletApi('process_invoke_data', { data: rawData });
}

// --- High-level shader call ---
export async function callShader(
  role: string,
  action: string,
  params: Record<string, string | number> = {},
  options: { createTx?: boolean; includeShader?: boolean; skipCid?: boolean; onReadyForConfirm?: () => void } = {}
): Promise<any> {
  const { createTx = false, includeShader = false, skipCid = false, onReadyForConfirm } = options;

  const argsParts: string[] = [`role=${role}`, `action=${action}`];

  if (!skipCid) {
    argsParts.push(`cid=${DEFAULT_CONTRACT_ID}`);
  }

  for (const [key, value] of Object.entries(params)) {
    if (value !== undefined && value !== '') {
      argsParts.push(`${key}=${value}`);
    }
  }

  const fullArgs = argsParts.join(',');
  const result = await invokeContract(fullArgs, includeShader);

  if (createTx && result?.raw_data) {
    // Fire callback exactly when wallet confirmation dialog is about to appear
    onReadyForConfirm?.();
    return processContractData(result.raw_data);
  }

  return result;
}

// --- Shader cache management ---
export function markShaderCached(): void {
  ShaderCached = true;
}

export function isApiReady(): boolean {
  return BEAM_API !== null;
}

// --- Wallet status ---
export async function getWalletStatus(): Promise<any> {
  return callWalletApi('wallet_status');
}
