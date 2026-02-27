const GROTH_PER_BEAM = 100_000_000;

export function grothToBeam(groth: number): number {
  return groth / GROTH_PER_BEAM;
}

export function beamToGroth(beam: number): number {
  return Math.round(beam * GROTH_PER_BEAM);
}

export function formatBeam(groth: number, decimals: number = 4): string {
  const beam = grothToBeam(groth);
  if (beam >= 1000) return beam.toFixed(2);
  if (beam >= 1) return beam.toFixed(decimals);
  return beam.toFixed(Math.min(8, decimals + 4));
}

export function formatMultiplier(mult: number): string {
  return (mult / 100).toFixed(2) + 'x';
}

export function betTypeLabel(type: number): string {
  switch (type) {
    case 0: return 'UP';
    case 1: return 'DOWN';
    case 2: return 'EXACT';
    default: return '?';
  }
}

export function betTypeIcon(type: number): string {
  switch (type) {
    case 0: return '\u2191'; // ↑
    case 1: return '\u2193'; // ↓
    case 2: return '\u25CE'; // ◎
    default: return '?';
  }
}

export function statusLabel(status: number): string {
  switch (status) {
    case 0: return 'Pending';
    case 1: return 'Won';
    case 2: return 'Lost';
    case 3: return 'Claimed';
    default: return 'Unknown';
  }
}

export function betTypeClass(type: number): string {
  switch (type) {
    case 0: return 'bet-up';
    case 1: return 'bet-down';
    case 2: return 'bet-exact';
    default: return '';
  }
}
