import { useReducer, useEffect, useCallback, useRef } from 'react';
import { initializeApi, extractError } from '../api/beam';
import {
  viewParams, viewPool, getMyBets, getResultHistory,
  placeBet as apiPlaceBet, checkResults as apiCheckResults,
  checkResult as apiCheckResult, deposit as apiDeposit,
  withdraw as apiWithdraw, setConfig as apiSetConfig,
  revealBet as apiRevealBet, resolveBets as apiResolveBets,
  viewAllBets as apiViewAllBets,
  ContractParams, PoolInfo, PendingBet, HistoryBet, BetType,
} from '../api/beambet';
import { beamToGroth, grothToBeam } from '../utils/format';

// --- State ---

export interface OptimisticBet {
  betTypeNum: number; // 0=up, 1=down, 2=exact
  amount: string;     // BEAM string
  exactNumber: number;
}

export interface RevealResult {
  betId: number;
  betType: number;
  exactNumber: number;
  result: number;
  won: boolean;
  payout: number;
  amount: number;
}

export interface BeamBetState {
  initStatus: 'connecting' | 'priming' | 'ready' | 'error';
  initError: string | null;

  params: ContractParams | null;
  pool: PoolInfo | null;
  pendingBets: PendingBet[];
  history: HistoryBet[];

  selectedBetType: BetType;
  betAmount: string;
  exactNumber: number;

  txStatus: 'idle' | 'placing' | 'confirming' | 'revealing' | 'success' | 'error';
  txError: string | null;

  revealResult: RevealResult | null;
  showReveal: boolean;

  optimisticBet: OptimisticBet | null;

  isAdmin: boolean;
}

const initialState: BeamBetState = {
  initStatus: 'connecting',
  initError: null,
  params: null,
  pool: null,
  pendingBets: [],
  history: [],
  selectedBetType: 'up',
  betAmount: '1',
  exactNumber: 50,
  txStatus: 'idle',
  txError: null,
  revealResult: null,
  showReveal: false,
  optimisticBet: null,
  isAdmin: false,
};

// --- Actions ---

type Action =
  | { type: 'INIT_PRIMING' }
  | { type: 'INIT_READY' }
  | { type: 'INIT_ERROR'; payload: string }
  | { type: 'SET_PARAMS'; payload: ContractParams }
  | { type: 'SET_POOL'; payload: PoolInfo }
  | { type: 'SET_PENDING_BETS'; payload: PendingBet[] }
  | { type: 'SET_HISTORY'; payload: HistoryBet[] }
  | { type: 'SET_BET_TYPE'; payload: BetType }
  | { type: 'SET_BET_AMOUNT'; payload: string }
  | { type: 'SET_EXACT_NUMBER'; payload: number }
  | { type: 'TX_START'; payload?: string }
  | { type: 'TX_CONFIRMING' }
  | { type: 'TX_SUCCESS' }
  | { type: 'TX_ERROR'; payload: string }
  | { type: 'TX_RESET' }
  | { type: 'SHOW_REVEAL'; payload: RevealResult }
  | { type: 'HIDE_REVEAL' }
  | { type: 'SET_OPTIMISTIC_BET'; payload: OptimisticBet }
  | { type: 'CLEAR_OPTIMISTIC_BET' }
  | { type: 'TOGGLE_ADMIN' };

function reducer(state: BeamBetState, action: Action): BeamBetState {
  switch (action.type) {
    case 'INIT_PRIMING':
      return { ...state, initStatus: 'priming' };
    case 'INIT_READY':
      return { ...state, initStatus: 'ready' };
    case 'INIT_ERROR':
      return { ...state, initStatus: 'error', initError: action.payload };
    case 'SET_PARAMS':
      return { ...state, params: action.payload };
    case 'SET_POOL':
      return { ...state, pool: action.payload };
    case 'SET_PENDING_BETS':
      return { ...state, pendingBets: action.payload };
    case 'SET_HISTORY':
      return { ...state, history: action.payload };
    case 'SET_BET_TYPE':
      return { ...state, selectedBetType: action.payload, txError: null };
    case 'SET_BET_AMOUNT':
      return { ...state, betAmount: action.payload, txError: null };
    case 'SET_EXACT_NUMBER':
      return { ...state, exactNumber: action.payload, txError: null };
    case 'TX_START':
      return { ...state, txStatus: action.payload === 'reveal' ? 'revealing' : 'placing', txError: null };
    case 'TX_CONFIRMING':
      return { ...state, txStatus: 'confirming' };
    case 'TX_SUCCESS':
      return { ...state, txStatus: 'success' };
    case 'TX_ERROR':
      return { ...state, txStatus: 'error', txError: action.payload };
    case 'TX_RESET':
      return { ...state, txStatus: 'idle', txError: null };
    case 'SHOW_REVEAL':
      return { ...state, revealResult: action.payload, showReveal: true };
    case 'HIDE_REVEAL':
      return { ...state, showReveal: false, revealResult: null };
    case 'SET_OPTIMISTIC_BET':
      return { ...state, optimisticBet: action.payload };
    case 'CLEAR_OPTIMISTIC_BET':
      return { ...state, optimisticBet: null };
    case 'TOGGLE_ADMIN':
      return { ...state, isAdmin: !state.isAdmin };
    default:
      return state;
  }
}

// --- Hook ---

export function useBeamBet() {
  const [state, dispatch] = useReducer(reducer, initialState);
  const historyRef = useRef<HistoryBet[]>([]);
  const pendingBetsRef = useRef<PendingBet[]>([]);

  // Keep refs in sync
  useEffect(() => {
    historyRef.current = state.history;
  }, [state.history]);

  useEffect(() => {
    pendingBetsRef.current = state.pendingBets;
  }, [state.pendingBets]);

  // --- Init ---
  useEffect(() => {
    let cancelled = false;

    async function init() {
      try {
        // 1. Connect to wallet
        await initializeApi();
        if (cancelled) return;

        dispatch({ type: 'INIT_PRIMING' });

        // 2. Prime shader (first call sends wasm bytes)
        const params = await viewParams();
        if (cancelled) return;

        dispatch({ type: 'SET_PARAMS', payload: params });
        dispatch({ type: 'INIT_READY' });

        // 3. Load initial data in parallel (shader already cached)
        const [pending, history, pool] = await Promise.all([
          getMyBets().catch(() => [] as PendingBet[]),
          getResultHistory().catch(() => [] as HistoryBet[]),
          viewPool().catch(() => null),
        ]);
        if (cancelled) return;

        dispatch({ type: 'SET_PENDING_BETS', payload: pending });
        dispatch({ type: 'SET_HISTORY', payload: history });
        if (pool) dispatch({ type: 'SET_POOL', payload: pool });

      } catch (err: any) {
        if (!cancelled) {
          dispatch({ type: 'INIT_ERROR', payload: extractError(err) });
        }
      }
    }

    init();
    return () => { cancelled = true; };
  }, []);

  // --- Polling (every 15s for pending bets) ---
  useEffect(() => {
    if (state.initStatus !== 'ready') return;

    const interval = setInterval(async () => {
      try {
        const pending = await getMyBets().catch(() => null);
        if (pending) dispatch({ type: 'SET_PENDING_BETS', payload: pending });
      } catch {
        // Silent fail on poll
      }
    }, 10000);

    return () => clearInterval(interval);
  }, [state.initStatus]);

  // --- Auto-reset TX status after success ---
  useEffect(() => {
    if (state.txStatus === 'success') {
      const timer = setTimeout(() => dispatch({ type: 'TX_RESET' }), 2500);
      return () => clearTimeout(timer);
    }
  }, [state.txStatus]);

  // --- Actions ---

  const doPlaceBet = useCallback(async () => {
    if (!state.params) return;

    const amountBeam = parseFloat(state.betAmount);
    if (isNaN(amountBeam) || amountBeam <= 0) {
      dispatch({ type: 'TX_ERROR', payload: 'Enter a valid amount' });
      return;
    }

    const amountGroth = beamToGroth(amountBeam);
    if (amountGroth < state.params.min_bet) {
      dispatch({ type: 'TX_ERROR', payload: `Minimum bet is ${grothToBeam(state.params.min_bet)} BEAM` });
      return;
    }
    if (amountGroth > state.params.max_bet) {
      dispatch({ type: 'TX_ERROR', payload: `Maximum bet is ${grothToBeam(state.params.max_bet)} BEAM` });
      return;
    }
    if (state.selectedBetType === 'exact' && (state.exactNumber < 1 || state.exactNumber > 100)) {
      dispatch({ type: 'TX_ERROR', payload: 'Pick a number between 1 and 100' });
      return;
    }
    if (state.params.paused) {
      dispatch({ type: 'TX_ERROR', payload: 'Betting is currently paused' });
      return;
    }

    try {
      dispatch({ type: 'TX_START' }); // shows PREPARING...

      // TX_CONFIRMING fires precisely when wallet dialog appears (after invoke_contract)
      await apiPlaceBet(
        amountGroth,
        state.selectedBetType,
        state.selectedBetType === 'exact' ? state.exactNumber : 0,
        undefined,
        () => dispatch({ type: 'TX_CONFIRMING' })
      );

      dispatch({ type: 'TX_SUCCESS' });

      // Show optimistic bet card immediately — real bet takes ~1 block to appear
      const betTypeNum = state.selectedBetType === 'up' ? 0 : state.selectedBetType === 'down' ? 1 : 2;
      dispatch({
        type: 'SET_OPTIMISTIC_BET',
        payload: { betTypeNum, amount: state.betAmount, exactNumber: state.exactNumber },
      });

      // Poll getMyBets every 3s until new bet appears on-chain (max 60s)
      const expectedCount = pendingBetsRef.current.length + 1;
      let attempts = 0;
      const pollForBet = async () => {
        if (++attempts > 20) {
          dispatch({ type: 'CLEAR_OPTIMISTIC_BET' });
          return;
        }
        try {
          const pending = await getMyBets();
          dispatch({ type: 'SET_PENDING_BETS', payload: pending });
          if (pending.length >= expectedCount) {
            dispatch({ type: 'CLEAR_OPTIMISTIC_BET' });
          } else {
            setTimeout(pollForBet, 3000);
          }
        } catch {
          setTimeout(pollForBet, 3000);
        }
      };
      setTimeout(pollForBet, 3000);

    } catch (err: any) {
      dispatch({ type: 'TX_ERROR', payload: extractError(err) });
    }
  }, [state.params, state.betAmount, state.selectedBetType, state.exactNumber]);

  const doCheckResults = useCallback(async () => {
    try {
      dispatch({ type: 'TX_START', payload: 'reveal' });

      const beforeHistory = historyRef.current;
      await apiCheckResults(() => dispatch({ type: 'TX_CONFIRMING' }));

      dispatch({ type: 'TX_SUCCESS' });

      // Refresh data
      const [pending, history] = await Promise.all([
        getMyBets().catch(() => [] as PendingBet[]),
        getResultHistory().catch(() => [] as HistoryBet[]),
      ]);

      dispatch({ type: 'SET_PENDING_BETS', payload: pending });
      dispatch({ type: 'SET_HISTORY', payload: history });

      // Find newly resolved bets for animation
      const newResults = history.filter(
        h => !beforeHistory.find(b => b.bet_id === h.bet_id)
      );

      if (newResults.length > 0) {
        const latest = newResults[0];
        dispatch({
          type: 'SHOW_REVEAL',
          payload: {
            betId: latest.bet_id,
            betType: latest.type,
            exactNumber: latest.exact_number,
            result: latest.result,
            won: latest.status === 1 || latest.status === 3,
            payout: latest.payout,
            amount: latest.amount,
          },
        });
      }
    } catch (err: any) {
      dispatch({ type: 'TX_ERROR', payload: extractError(err) });
    }
  }, []);

  const doCheckResult = useCallback(async (betId: number) => {
    try {
      dispatch({ type: 'TX_START', payload: 'reveal' });

      await apiCheckResult(betId, () => dispatch({ type: 'TX_CONFIRMING' }));

      dispatch({ type: 'TX_SUCCESS' });

      // Refresh data
      const [pending, history] = await Promise.all([
        getMyBets().catch(() => [] as PendingBet[]),
        getResultHistory().catch(() => [] as HistoryBet[]),
      ]);

      dispatch({ type: 'SET_PENDING_BETS', payload: pending });
      dispatch({ type: 'SET_HISTORY', payload: history });

      // Find the revealed bet
      const revealed = history.find(h => h.bet_id === betId);
      if (revealed) {
        dispatch({
          type: 'SHOW_REVEAL',
          payload: {
            betId: revealed.bet_id,
            betType: revealed.type,
            exactNumber: revealed.exact_number,
            result: revealed.result,
            won: revealed.status === 1 || revealed.status === 3,
            payout: revealed.payout,
            amount: revealed.amount,
          },
        });
      }
    } catch (err: any) {
      dispatch({ type: 'TX_ERROR', payload: extractError(err) });
    }
  }, []);

  const refreshPool = useCallback(async () => {
    try {
      const pool = await viewPool();
      dispatch({ type: 'SET_POOL', payload: pool });
    } catch {}
  }, []);

  const refreshHistory = useCallback(async () => {
    try {
      const history = await getResultHistory();
      dispatch({ type: 'SET_HISTORY', payload: history });
    } catch {}
  }, []);

  const refreshPending = useCallback(async () => {
    try {
      const pending = await getMyBets();
      dispatch({ type: 'SET_PENDING_BETS', payload: pending });
    } catch {}
  }, []);

  return {
    state,
    actions: {
      placeBet: doPlaceBet,
      checkResults: doCheckResults,
      checkResult: doCheckResult,
      refreshPool,
      refreshHistory,
      refreshPending,
      setBetType: (type: BetType) => dispatch({ type: 'SET_BET_TYPE', payload: type }),
      setBetAmount: (amount: string) => dispatch({ type: 'SET_BET_AMOUNT', payload: amount }),
      setExactNumber: (n: number) => dispatch({ type: 'SET_EXACT_NUMBER', payload: n }),
      hideReveal: () => dispatch({ type: 'HIDE_REVEAL' }),
      toggleAdmin: () => dispatch({ type: 'TOGGLE_ADMIN' }),
      txReset: () => dispatch({ type: 'TX_RESET' }),
      // Admin
      adminDeposit: async (amount: number) => {
        dispatch({ type: 'TX_START' });
        dispatch({ type: 'TX_CONFIRMING' });
        try {
          await apiDeposit(beamToGroth(amount));
          dispatch({ type: 'TX_SUCCESS' });
          refreshPool();
        } catch (err: any) {
          dispatch({ type: 'TX_ERROR', payload: extractError(err) });
        }
      },
      adminWithdraw: async (amount: number) => {
        dispatch({ type: 'TX_START' });
        dispatch({ type: 'TX_CONFIRMING' });
        try {
          await apiWithdraw(beamToGroth(amount));
          dispatch({ type: 'TX_SUCCESS' });
          refreshPool();
        } catch (err: any) {
          dispatch({ type: 'TX_ERROR', payload: extractError(err) });
        }
      },
      adminSetConfig: async (config: any) => {
        dispatch({ type: 'TX_START' });
        dispatch({ type: 'TX_CONFIRMING' });
        try {
          await apiSetConfig(config);
          dispatch({ type: 'TX_SUCCESS' });
          viewParams().then(params => dispatch({ type: 'SET_PARAMS', payload: params })).catch(() => {});
        } catch (err: any) {
          dispatch({ type: 'TX_ERROR', payload: extractError(err) });
        }
      },
      adminRevealBet: async (betId: number) => {
        dispatch({ type: 'TX_START' });
        dispatch({ type: 'TX_CONFIRMING' });
        try {
          await apiRevealBet(betId);
          dispatch({ type: 'TX_SUCCESS' });
        } catch (err: any) {
          dispatch({ type: 'TX_ERROR', payload: extractError(err) });
        }
      },
      adminResolveBets: async (count?: number) => {
        dispatch({ type: 'TX_START' });
        dispatch({ type: 'TX_CONFIRMING' });
        try {
          await apiResolveBets(count);
          dispatch({ type: 'TX_SUCCESS' });
        } catch (err: any) {
          dispatch({ type: 'TX_ERROR', payload: extractError(err) });
        }
      },
      adminViewAllBets: apiViewAllBets,
    },
  };
}
