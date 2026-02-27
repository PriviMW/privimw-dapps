import React, { useRef } from 'react';
import { PoolInfo } from '../api/beambet';
import { formatBeam } from '../utils/format';

interface Props {
  pool: PoolInfo | null;
  isAdmin: boolean;
  onToggleAdmin: () => void;
}

export function Header({ pool, isAdmin, onToggleAdmin }: Props) {
  const clickCount = useRef(0);
  const clickTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  // Secret admin toggle: click logo 5 times
  const handleLogoClick = () => {
    clickCount.current++;
    if (clickTimer.current) clearTimeout(clickTimer.current);

    if (clickCount.current >= 5) {
      clickCount.current = 0;
      onToggleAdmin();
    } else {
      clickTimer.current = setTimeout(() => {
        clickCount.current = 0;
      }, 2000);
    }
  };

  return (
    <div className="header">
      <div className="header-logo" onClick={handleLogoClick}>
        BeamBet
      </div>

      <div className="header-pool">
        {isAdmin && <span className="header-admin-badge">ADMIN</span>}
        {pool && (
          <>
            <span className="header-pool-label">Pool:</span>
            <span className="header-pool-amount">
              {formatBeam(pool.available_balance)}
            </span>
          </>
        )}
        <div className="header-status" />
      </div>
    </div>
  );
}
