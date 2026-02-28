// BeamBet DApp-specific API — typed wrappers over beam.ts callShader
import { callShader, markShaderCached } from './beam';

// --- Types ---

export interface ContractParams {
  min_bet: number;
  max_bet: number;
  up_down_mult: number;
  exact_mult: number;
  reveal_epoch: number;
  paused: number;
  asset_id: number;
}

export interface PoolInfo {
  total_deposited: number;
  total_bets: number;
  total_payouts: number;
  pending_bets: number;
  pending_max_payout: number;
  pending_payouts: number;
  available_balance: number;
  asset_id: number;
  paused: number;
}

export interface PendingBet {
  bet_id: number;
  amount: number;
  type: number;
  exact_number: number;
  status: number;
  created_height: number;
  blocks_remaining: number;
  can_reveal: number;
}

export interface HistoryBet {
  bet_id: number;
  amount: number;
  type: number;
  exact_number: number;
  result: number;
  status: number;
  payout: number;
  created_height: number;
  revealed_height: number;
  status_text: string;
}

export type BetType = 'up' | 'down' | 'exact';

// --- Helpers ---

function betTypeToNum(type: BetType): number {
  switch (type) {
    case 'up': return 0;
    case 'down': return 1;
    case 'exact': return 2;
  }
}

export function generateCommitment(): string {
  const bytes = new Uint8Array(32);
  crypto.getRandomValues(bytes);
  return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
}

// --- View calls (no TX) ---

export async function viewParams(): Promise<ContractParams> {
  const result = await callShader('user', 'view_params', {}, { includeShader: true });
  markShaderCached();
  return result.params || result;
}

export async function viewPool(): Promise<PoolInfo> {
  const result = await callShader('manager', 'view_pool');
  return result.pool || result;
}

export async function getMyBets(): Promise<PendingBet[]> {
  const result = await callShader('user', 'my_bets');
  return result.bets || [];
}

export async function getResultHistory(): Promise<HistoryBet[]> {
  const result = await callShader('user', 'result_history');
  return result.history || [];
}

export async function viewUserPk(): Promise<string> {
  const result = await callShader('user', 'view_user_pk');
  return result.user_pk;
}

// --- TX calls (two-step: invoke_contract -> process_invoke_data) ---

export async function placeBet(
  amount: number,
  betType: BetType,
  exactNumber: number = 0,
  commitment?: string,
  onReadyForConfirm?: () => void
): Promise<any> {
  const comm = commitment || generateCommitment();
  return callShader('user', 'place_bet', {
    amount,
    asset_id: 0,
    bet_type: betTypeToNum(betType),
    exact_number: exactNumber,
    commitment: comm,
  }, { createTx: true, onReadyForConfirm });
}

export async function checkResults(onReadyForConfirm?: () => void): Promise<any> {
  return callShader('user', 'check_results', {}, { createTx: true, onReadyForConfirm });
}

export async function checkResult(betId: number, onReadyForConfirm?: () => void): Promise<any> {
  return callShader('user', 'check_result', { bet_id: betId }, { createTx: true, onReadyForConfirm });
}

// --- Manager TX calls ---

export async function deposit(amount: number): Promise<any> {
  return callShader('manager', 'deposit', { amount }, { createTx: true });
}

export async function withdraw(amount: number): Promise<any> {
  return callShader('manager', 'withdraw', { amount }, { createTx: true });
}

export async function setConfig(config: {
  min_bet: number;
  max_bet: number;
  up_down_mult: number;
  exact_mult: number;
  paused: number;
  asset_id: number;
}): Promise<any> {
  return callShader('manager', 'set_config', config, { createTx: true });
}

export async function revealBet(betId: number): Promise<any> {
  return callShader('manager', 'reveal_bet', { bet_id: betId }, { createTx: true });
}

export async function resolveBets(count: number = 50): Promise<any> {
  return callShader('manager', 'resolve_bets', { count }, { createTx: true });
}

export async function viewAllBets(): Promise<any> {
  const result = await callShader('manager', 'view_all_bets');
  return result;
}

