import React, { useState, useEffect, useRef } from 'react';
import { RevealResult } from '../hooks/useBeamBet';
import { formatBeam, betTypeLabel } from '../utils/format';

interface Props {
  result: RevealResult;
  onClose: () => void;
}

export function ResultReveal({ result, onClose }: Props) {
  const [phase, setPhase] = useState<'spinning' | 'revealed'>('spinning');
  const [displayNumber, setDisplayNumber] = useState(0);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // Spinning number animation
  useEffect(() => {
    let count = 0;
    intervalRef.current = setInterval(() => {
      setDisplayNumber(Math.floor(Math.random() * 100) + 1);
      count++;
      if (count >= 20) {
        if (intervalRef.current) clearInterval(intervalRef.current);
        setDisplayNumber(result.result);
        setPhase('revealed');
      }
    }, 80);

    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
    };
  }, [result.result]);

  // Close on Escape
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [onClose]);

  const betInfo = `${betTypeLabel(result.betType)}${result.betType === 2 ? ` #${result.exactNumber}` : ''} \u2022 ${formatBeam(result.amount)} BEAM`;

  return (
    <div className="reveal-overlay" onClick={onClose}>
      <div
        className={`reveal-card ${result.won ? 'won' : 'lost'}`}
        onClick={e => e.stopPropagation()}
        style={{ position: 'relative' }}
      >
        <div className="reveal-label">
          {phase === 'spinning' ? 'Rolling...' : 'Result'}
        </div>

        <div className={`reveal-number ${phase === 'revealed' ? (result.won ? 'won' : 'lost') : ''}`}>
          {displayNumber || '?'}
        </div>

        {phase === 'revealed' && (
          <>
            <div className={`reveal-outcome ${result.won ? 'won' : 'lost'}`}>
              {result.won ? 'YOU WIN!' : 'Better luck next time'}
            </div>

            {result.won && result.payout > 0 && (
              <div className="reveal-payout">
                +{formatBeam(result.payout)} BEAM
              </div>
            )}

            <div className="reveal-bet-info">{betInfo}</div>

            <button className="reveal-close-btn" onClick={onClose}>
              Continue
            </button>
          </>
        )}
      </div>
    </div>
  );
}
