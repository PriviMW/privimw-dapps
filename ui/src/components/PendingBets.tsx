import React from 'react';
import { BeamBetState } from '../hooks/useBeamBet';
import { formatBeam, betTypeIcon, betTypeLabel, betTypeClass } from '../utils/format';

interface Props {
  state: BeamBetState;
  actions: {
    checkResults: () => Promise<void>;
    checkResult: (betId: number) => Promise<void>;
  };
}

export function PendingBets({ state, actions }: Props) {
  const { pendingBets, txStatus, optimisticBet } = state;

  if (pendingBets.length === 0 && !optimisticBet) return null;

  const hasRevealable = pendingBets.some(b => b.can_reveal === 1);
  const isBusy = txStatus === 'revealing' || txStatus === 'confirming';
  const totalCount = pendingBets.length + (optimisticBet ? 1 : 0);

  return (
    <div className="card">
      <div className="pending-header">
        <span className="card-title" style={{ marginBottom: 0 }}>
          Active Bets ({totalCount})
        </span>
        {hasRevealable && (
          <button
            className="reveal-all-btn"
            onClick={actions.checkResults}
            disabled={isBusy}
          >
            {isBusy ? 'Revealing...' : 'Reveal All'}
          </button>
        )}
      </div>

      <div className="pending-list">
        {/* Optimistic bet — shown while waiting for on-chain confirmation */}
        {optimisticBet && (
          <div className="pending-card optimistic-card">
            <div className={`pending-badge ${betTypeClass(optimisticBet.betTypeNum)}`}>
              {betTypeIcon(optimisticBet.betTypeNum)}
            </div>

            <div className="pending-info">
              <div className="pending-amount">
                {optimisticBet.amount} BEAM
              </div>
              <div className="pending-detail">
                {betTypeLabel(optimisticBet.betTypeNum)}
                {optimisticBet.betTypeNum === 2 ? ` #${optimisticBet.exactNumber}` : ''}
              </div>
              <div className="progress-bar">
                <div className="progress-fill optimistic-fill" />
              </div>
            </div>

            <div className="pending-countdown">
              <div className="optimistic-status">Confirming...</div>
            </div>
          </div>
        )}

        {pendingBets.map(bet => {
          const progressPct = bet.blocks_remaining > 0
            ? Math.max(0, ((10 - bet.blocks_remaining) / 10) * 100)
            : 100;

          return (
            <div
              key={bet.bet_id}
              className={`pending-card ${bet.can_reveal ? 'can-reveal' : ''}`}
            >
              <div className={`pending-badge ${betTypeClass(bet.type)}`}>
                {betTypeIcon(bet.type)}
              </div>

              <div className="pending-info">
                <div className="pending-amount">
                  {formatBeam(bet.amount)} BEAM
                </div>
                <div className="pending-detail">
                  {betTypeLabel(bet.type)}
                  {bet.type === 2 ? ` #${bet.exact_number}` : ''}
                  {' \u2022 '}Bet #{bet.bet_id}
                </div>
                <div className="progress-bar">
                  <div className="progress-fill" style={{ width: `${progressPct}%` }} />
                </div>
              </div>

              <div className="pending-countdown">
                {bet.can_reveal ? (
                  <button
                    className="reveal-btn"
                    onClick={() => actions.checkResult(bet.bet_id)}
                    disabled={isBusy}
                  >
                    REVEAL
                  </button>
                ) : (
                  <>
                    <div className="pending-blocks">{bet.blocks_remaining} blocks</div>
                    <div className="pending-time">~{bet.blocks_remaining} min</div>
                  </>
                )}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
