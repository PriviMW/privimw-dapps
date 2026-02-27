import React from 'react';
import { BeamBetState } from '../hooks/useBeamBet';
import { BetType } from '../api/beambet';
import { formatBeam, formatMultiplier, betTypeIcon, betTypeClass } from '../utils/format';

interface Props {
  state: BeamBetState;
  actions: {
    setBetType: (type: BetType) => void;
    setBetAmount: (amount: string) => void;
    setExactNumber: (n: number) => void;
    placeBet: () => Promise<void>;
    txReset: () => void;
  };
}

const AMOUNT_CHIPS = [0.1, 0.5, 1, 5, 10];

export function BetPanel({ state, actions }: Props) {
  const { params, selectedBetType, betAmount, exactNumber, txStatus, txError } = state;
  if (!params) return null;

  const amountNum = parseFloat(betAmount) || 0;
  const multiplier = selectedBetType === 'exact' ? params.exact_mult : params.up_down_mult;
  const potentialPayout = amountNum * (multiplier / 100);

  const isDisabled = txStatus !== 'idle' && txStatus !== 'error' && txStatus !== 'success';
  const isPaused = params.paused === 1;

  const buttonText = () => {
    if (isPaused) return 'BETTING PAUSED';
    switch (txStatus) {
      case 'placing': return 'PREPARING...';
      case 'confirming': return 'CONFIRM IN WALLET...';
      case 'success': return 'BET PLACED!';
      case 'error': return 'PLACE BET';
      default: return 'PLACE BET';
    }
  };

  const buttonClass = () => {
    const base = `place-bet-btn type-${selectedBetType}`;
    if (txStatus === 'confirming') return `${base} confirming`;
    if (txStatus === 'success') return `${base} success`;
    return base;
  };

  return (
    <div className="card">
      {/* Bet Type Selector */}
      <div className="bet-type-selector">
        {(['up', 'down', 'exact'] as BetType[]).map(type => {
          const typeNum = type === 'up' ? 0 : type === 'down' ? 1 : 2;
          const mult = type === 'exact' ? params.exact_mult : params.up_down_mult;
          return (
            <button
              key={type}
              className={`bet-type-btn ${betTypeClass(typeNum)} ${selectedBetType === type ? 'active' : ''}`}
              onClick={() => actions.setBetType(type)}
            >
              <span className="bet-type-icon">{betTypeIcon(typeNum)}</span>
              <span>{type.toUpperCase()}</span>
              <span className="bet-type-mult">{formatMultiplier(mult)}</span>
            </button>
          );
        })}
      </div>

      {/* Amount Input */}
      <div className="amount-section">
        <label className="amount-label">Bet Amount</label>
        <div className="amount-input-wrap">
          <input
            className="amount-input"
            type="number"
            placeholder="0.00"
            value={betAmount}
            onChange={e => actions.setBetAmount(e.target.value)}
            min={0}
            step={0.01}
          />
          <span className="amount-suffix">BEAM</span>
        </div>
        <div className="amount-chips">
          {AMOUNT_CHIPS.map(v => (
            <button
              key={v}
              className={`amount-chip ${betAmount === String(v) ? 'active' : ''}`}
              onClick={() => actions.setBetAmount(String(v))}
            >
              {v}
            </button>
          ))}
          <button
            className="amount-chip"
            onClick={() => actions.setBetAmount(String(params.max_bet / 100_000_000))}
          >
            MAX
          </button>
        </div>
      </div>

      {/* Exact Number Picker */}
      {selectedBetType === 'exact' && (
        <div className="exact-section">
          <label className="amount-label">Pick Your Number (1-100)</label>
          <div className="exact-grid">
            {Array.from({ length: 100 }, (_, i) => i + 1).map(n => (
              <button
                key={n}
                className={`exact-cell ${exactNumber === n ? 'selected' : ''}`}
                onClick={() => actions.setExactNumber(n)}
              >
                {n}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Payout Preview */}
      {amountNum > 0 && (
        <div className="payout-preview">
          <div>
            <div className="payout-label">Potential Win</div>
            <div className="payout-range">
              {formatMultiplier(multiplier)} payout
            </div>
          </div>
          <div className={`payout-amount ${selectedBetType === 'exact' ? 'exact' : ''}`}>
            {potentialPayout.toFixed(4)} BEAM
          </div>
        </div>
      )}

      {/* Place Bet Button */}
      <button
        className={buttonClass()}
        onClick={txStatus === 'error' ? () => { actions.txReset(); actions.placeBet(); } : actions.placeBet}
        disabled={isDisabled || isPaused || amountNum <= 0}
      >
        {buttonText()}
      </button>

      {/* Error Display */}
      {txError && txStatus === 'error' && (
        <div className="tx-error">{txError}</div>
      )}
    </div>
  );
}
