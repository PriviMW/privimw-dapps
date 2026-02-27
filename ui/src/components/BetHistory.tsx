import React from 'react';
import { BeamBetState } from '../hooks/useBeamBet';
import { formatBeam, betTypeLabel, betTypeClass } from '../utils/format';
import { grothToBeam } from '../utils/format';

interface Props {
  state: BeamBetState;
  actions: {
    refreshHistory: () => Promise<void>;
  };
}

export function BetHistory({ state, actions }: Props) {
  const { history } = state;

  if (history.length === 0) {
    return (
      <div className="card">
        <div className="card-title">History</div>
        <div className="empty-state">
          No bets yet
          <div className="empty-state-sub">Place your first bet above!</div>
        </div>
      </div>
    );
  }

  // Stats
  const wins = history.filter(h => h.status === 1 || h.status === 3);
  const losses = history.filter(h => h.status === 2);
  const totalWon = wins.reduce((sum, h) => sum + h.payout, 0);
  const totalWagered = history.reduce((sum, h) => sum + h.amount, 0);
  const netProfit = totalWon - totalWagered;
  const winRate = history.length > 0 ? ((wins.length / history.length) * 100).toFixed(0) : '0';

  return (
    <div className="card">
      <div className="card-title">History</div>

      {/* Stats Row */}
      <div className="history-stats">
        <div className="stat-item">
          <div className={`stat-value ${netProfit >= 0 ? 'positive' : 'negative'}`}>
            {netProfit >= 0 ? '+' : ''}{grothToBeam(netProfit).toFixed(2)}
          </div>
          <div className="stat-label">Net P&L</div>
        </div>
        <div className="stat-item">
          <div className="stat-value neutral">{winRate}%</div>
          <div className="stat-label">Win Rate</div>
        </div>
        <div className="stat-item">
          <div className="stat-value neutral">{history.length}</div>
          <div className="stat-label">Total Bets</div>
        </div>
      </div>

      {/* History List */}
      <div className="history-list">
        {history.map(bet => {
          const isWon = bet.status === 1 || bet.status === 3;
          const statusClass = isWon ? 'won' : bet.status === 2 ? 'lost' : 'claimed';

          return (
            <div key={bet.bet_id} className={`history-row ${statusClass}`}>
              <span className={`history-type ${betTypeClass(bet.type)}`}>
                {betTypeLabel(bet.type)}
                {bet.type === 2 ? ` ${bet.exact_number}` : ''}
              </span>
              <span className="history-amount">
                {formatBeam(bet.amount)}
              </span>
              <span className="history-result">{bet.result}</span>
              <span className={`history-payout ${isWon ? 'won' : 'lost'}`}>
                {isWon ? `+${formatBeam(bet.payout)}` : '-'}
              </span>
            </div>
          );
        })}
      </div>
    </div>
  );
}
