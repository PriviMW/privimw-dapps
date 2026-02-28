import React from 'react';
import { useBeamBet } from './hooks/useBeamBet';
import { Header } from './components/Header';
import { BetPanel } from './components/BetPanel';
import { PendingBets } from './components/PendingBets';
import { BetHistory } from './components/BetHistory';
import { PoolInfo } from './components/PoolInfo';
import { ResultReveal } from './components/ResultReveal';
import { AdminPanel } from './components/AdminPanel';
import { Footer } from './components/Footer';

function App() {
  const { state, actions } = useBeamBet();

  // Loading states
  if (state.initStatus === 'connecting') {
    return (
      <div className="app">
        <div className="loading-screen">
          <div className="loading-spinner" />
          <p className="loading-text">Connecting to wallet...</p>
        </div>
      </div>
    );
  }

  if (state.initStatus === 'priming') {
    return (
      <div className="app">
        <div className="loading-screen">
          <div className="loading-spinner" />
          <p className="loading-text">Loading BeamBet shader...</p>
        </div>
      </div>
    );
  }

  if (state.initStatus === 'error') {
    return (
      <div className="app">
        <div className="loading-screen">
          <div className="error-icon">!</div>
          <p className="loading-text">Failed to initialize</p>
          <p className="error-detail">{state.initError}</p>
        </div>
      </div>
    );
  }

  return (
    <div className="app">
      <Header
        pool={state.pool}
        isAdmin={state.isAdmin}
        onToggleAdmin={actions.toggleAdmin}
      />

      {state.isAdmin ? (
        <AdminPanel state={state} actions={actions} />
      ) : (
        <div className="main-content">
          <BetPanel state={state} actions={actions} />
          <PendingBets state={state} actions={actions} />
          <BetHistory state={state} actions={actions} />
          <PoolInfo pool={state.pool} onRefresh={actions.refreshPool} />
        </div>
      )}

      {state.showReveal && state.revealResult && (
        <ResultReveal
          result={state.revealResult}
          onClose={actions.hideReveal}
        />
      )}

      <Footer />
    </div>
  );
}

export default App;
