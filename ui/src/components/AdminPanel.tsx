import React, { useState, useEffect } from 'react';
import { BeamBetState } from '../hooks/useBeamBet';
import { formatBeam } from '../utils/format';

interface Props {
  state: BeamBetState;
  actions: {
    refreshPool: () => Promise<void>;
    adminDeposit: (amount: number) => Promise<void>;
    adminWithdraw: (amount: number) => Promise<void>;
    adminSetConfig: (config: any) => Promise<void>;
    adminRevealBet: (betId: number) => Promise<void>;
    adminResolveBets: (count?: number) => Promise<void>;
  };
}

type AdminTab = 'pool' | 'config' | 'bets';

export function AdminPanel({ state, actions }: Props) {
  const [tab, setTab] = useState<AdminTab>('pool');
  const [depositAmount, setDepositAmount] = useState('');
  const [withdrawAmount, setWithdrawAmount] = useState('');
  const [revealBetId, setRevealBetId] = useState('');
  const [resolveCount, setResolveCount] = useState('50');

  // Config form state
  const [configMinBet, setConfigMinBet] = useState('');
  const [configMaxBet, setConfigMaxBet] = useState('');
  const [configUpDownMult, setConfigUpDownMult] = useState('');
  const [configExactMult, setConfigExactMult] = useState('');
  const [configPaused, setConfigPaused] = useState(false);

  const { pool, params, txStatus } = state;
  const isBusy = txStatus !== 'idle' && txStatus !== 'error' && txStatus !== 'success';

  // Initialize config form from params
  useEffect(() => {
    if (params) {
      setConfigMinBet(String(params.min_bet));
      setConfigMaxBet(String(params.max_bet));
      setConfigUpDownMult(String(params.up_down_mult));
      setConfigExactMult(String(params.exact_mult));
      setConfigPaused(params.paused === 1);
    }
  }, [params]);

  // Refresh pool on mount
  useEffect(() => {
    actions.refreshPool();
  }, []);

  return (
    <div className="card">
      <div className="card-title">Admin Panel</div>

      <div className="admin-tabs">
        {(['pool', 'config', 'bets'] as AdminTab[]).map(t => (
          <button
            key={t}
            className={`admin-tab ${tab === t ? 'active' : ''}`}
            onClick={() => setTab(t)}
          >
            {t === 'pool' ? 'Pool' : t === 'config' ? 'Config' : 'Bets'}
          </button>
        ))}
      </div>

      {/* Pool Management Tab */}
      {tab === 'pool' && (
        <div className="admin-section">
          {pool && (
            <div className="pool-stats" style={{ marginBottom: 16 }}>
              <div className="pool-stat">
                <div className="pool-stat-label">Available</div>
                <div className="pool-stat-value highlight">{formatBeam(pool.available_balance)} BEAM</div>
              </div>
              <div className="pool-stat">
                <div className="pool-stat-label">Total Deposited</div>
                <div className="pool-stat-value">{formatBeam(pool.total_deposited)} BEAM</div>
              </div>
              <div className="pool-stat">
                <div className="pool-stat-label">Pending Max Payout</div>
                <div className="pool-stat-value">{formatBeam(pool.pending_max_payout)} BEAM</div>
              </div>
              <div className="pool-stat">
                <div className="pool-stat-label">Pending Payouts</div>
                <div className="pool-stat-value">{formatBeam(pool.pending_payouts)} BEAM</div>
              </div>
            </div>
          )}

          <div className="admin-row">
            <div className="admin-input-group">
              <label className="admin-input-label">Deposit (BEAM)</label>
              <input
                className="admin-input"
                type="number"
                value={depositAmount}
                onChange={e => setDepositAmount(e.target.value)}
                placeholder="0"
              />
            </div>
            <button
              className="admin-btn"
              disabled={isBusy || !depositAmount}
              onClick={() => { actions.adminDeposit(parseFloat(depositAmount)); setDepositAmount(''); }}
            >
              Deposit
            </button>
          </div>

          <div className="admin-row">
            <div className="admin-input-group">
              <label className="admin-input-label">Withdraw (BEAM)</label>
              <input
                className="admin-input"
                type="number"
                value={withdrawAmount}
                onChange={e => setWithdrawAmount(e.target.value)}
                placeholder="0"
              />
            </div>
            <button
              className="admin-btn danger"
              disabled={isBusy || !withdrawAmount}
              onClick={() => { actions.adminWithdraw(parseFloat(withdrawAmount)); setWithdrawAmount(''); }}
            >
              Withdraw
            </button>
          </div>
        </div>
      )}

      {/* Config Tab */}
      {tab === 'config' && (
        <div className="admin-section">
          <div className="admin-row">
            <div className="admin-input-group">
              <label className="admin-input-label">Min Bet (groth)</label>
              <input
                className="admin-input"
                type="number"
                value={configMinBet}
                onChange={e => setConfigMinBet(e.target.value)}
              />
            </div>
            <div className="admin-input-group">
              <label className="admin-input-label">Max Bet (groth)</label>
              <input
                className="admin-input"
                type="number"
                value={configMaxBet}
                onChange={e => setConfigMaxBet(e.target.value)}
              />
            </div>
          </div>

          <div className="admin-row">
            <div className="admin-input-group">
              <label className="admin-input-label">Up/Down Mult (x100)</label>
              <input
                className="admin-input"
                type="number"
                value={configUpDownMult}
                onChange={e => setConfigUpDownMult(e.target.value)}
              />
            </div>
            <div className="admin-input-group">
              <label className="admin-input-label">Exact Mult (x100)</label>
              <input
                className="admin-input"
                type="number"
                value={configExactMult}
                onChange={e => setConfigExactMult(e.target.value)}
              />
            </div>
          </div>

          <div className="admin-toggle">
            <button
              className={`admin-toggle-switch ${configPaused ? 'on' : 'off'}`}
              onClick={() => setConfigPaused(!configPaused)}
            />
            <span style={{ fontSize: 13 }}>
              {configPaused ? 'Paused (betting disabled)' : 'Active (betting enabled)'}
            </span>
          </div>

          <button
            className="admin-btn"
            disabled={isBusy}
            onClick={() => actions.adminSetConfig({
              min_bet: parseInt(configMinBet) || 0,
              max_bet: parseInt(configMaxBet) || 0,
              up_down_mult: parseInt(configUpDownMult) || 0,
              exact_mult: parseInt(configExactMult) || 0,
              paused: configPaused ? 1 : 0,
              asset_id: 0,
            })}
          >
            Update Config
          </button>
        </div>
      )}

      {/* Bets Management Tab */}
      {tab === 'bets' && (
        <div className="admin-section">
          <div className="admin-row">
            <div className="admin-input-group">
              <label className="admin-input-label">Reveal Bet by ID</label>
              <input
                className="admin-input"
                type="number"
                value={revealBetId}
                onChange={e => setRevealBetId(e.target.value)}
                placeholder="Bet ID"
              />
            </div>
            <button
              className="admin-btn"
              disabled={isBusy || !revealBetId}
              onClick={() => { actions.adminRevealBet(parseInt(revealBetId)); setRevealBetId(''); }}
            >
              Reveal
            </button>
          </div>

          <div className="admin-row">
            <div className="admin-input-group">
              <label className="admin-input-label">Resolve Expired (count)</label>
              <input
                className="admin-input"
                type="number"
                value={resolveCount}
                onChange={e => setResolveCount(e.target.value)}
                placeholder="50"
              />
            </div>
            <button
              className="admin-btn"
              disabled={isBusy}
              onClick={() => actions.adminResolveBets(parseInt(resolveCount) || 50)}
            >
              Resolve
            </button>
          </div>
        </div>
      )}

      {state.txError && state.txStatus === 'error' && (
        <div className="tx-error" style={{ marginTop: 12 }}>{state.txError}</div>
      )}
    </div>
  );
}
