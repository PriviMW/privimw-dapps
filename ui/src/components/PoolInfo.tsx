import React from 'react';
import { PoolInfo as PoolInfoType } from '../api/beambet';
import { formatBeam } from '../utils/format';

interface Props {
  pool: PoolInfoType | null;
  onRefresh: () => void;
}

export function PoolInfo({ pool, onRefresh }: Props) {
  if (!pool) return null;

  return (
    <div className="card">
      <div className="card-title">Pool Stats</div>
      <div className="pool-stats">
        <div className="pool-stat">
          <div className="pool-stat-label">Available Balance</div>
          <div className="pool-stat-value highlight">{formatBeam(pool.available_balance)} BEAM</div>
        </div>
        <div className="pool-stat">
          <div className="pool-stat-label">Total Deposited</div>
          <div className="pool-stat-value">{formatBeam(pool.total_deposited)} BEAM</div>
        </div>
        <div className="pool-stat">
          <div className="pool-stat-label">Total Payouts</div>
          <div className="pool-stat-value">{formatBeam(pool.total_payouts)} BEAM</div>
        </div>
        <div className="pool-stat">
          <div className="pool-stat-label">Pending Bets</div>
          <div className="pool-stat-value">{formatBeam(pool.pending_bets)} BEAM</div>
        </div>
      </div>
      <button className="pool-refresh-btn" onClick={onRefresh}>
        Refresh
      </button>
    </div>
  );
}
