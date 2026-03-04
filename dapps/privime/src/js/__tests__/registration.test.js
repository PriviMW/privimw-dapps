import { describe, it, expect, vi } from 'vitest';

// Mock dependencies to isolate registration logic
vi.mock('../state.js', () => ({
    shaderBytes: null,
}));
vi.mock('../helpers.js', () => ({
    showToast: vi.fn(),
    extractError: vi.fn((r) => r?.error || 'error'),
    formatFee: vi.fn(() => '1 BEAM'),
}));
vi.mock('../shader.js', () => ({
    privimeInvoke: vi.fn(),
    privimeTx: vi.fn(),
}));
vi.mock('../wallet-api.js', () => ({
    callApi: vi.fn(),
}));

import { normalizeWalletId, isValidWalletId } from '../registration.js';

describe('normalizeWalletId', () => {
    it('returns 68-char hex for valid 68-char input', () => {
        var wid = '02' + 'a'.repeat(66);
        expect(normalizeWalletId(wid)).toBe(wid);
        expect(normalizeWalletId(wid).length).toBe(68);
    });

    it('pads 67-char (odd length) to 68 by prepending 0', () => {
        var wid = 'a'.repeat(67);
        var result = normalizeWalletId(wid);
        expect(result.length).toBe(68);
        expect(result).toBe('0' + wid);
    });

    it('pads 66-char to 68 by left-padding with zeros', () => {
        var wid = 'a'.repeat(66);
        var result = normalizeWalletId(wid);
        expect(result.length).toBe(68);
        expect(result).toBe('00' + wid);
    });

    it('handles 65-char (odd) -> prepend 0 -> 66 -> pad to 68', () => {
        var wid = 'b'.repeat(65);
        var result = normalizeWalletId(wid);
        expect(result.length).toBe(68);
        expect(result).toBe('000' + wid);
    });

    it('handles 62-char input (minimum accepted)', () => {
        var wid = 'c'.repeat(62);
        var result = normalizeWalletId(wid);
        expect(result.length).toBe(68);
        expect(result).toBe('000000' + wid);
    });

    it('rejects too short input (< 62 chars)', () => {
        expect(normalizeWalletId('abc')).toBeNull();
        expect(normalizeWalletId('a'.repeat(60))).toBeNull();
    });

    it('rejects too long input (> 68 chars)', () => {
        expect(normalizeWalletId('a'.repeat(70))).toBeNull();
    });

    it('rejects non-hex characters', () => {
        expect(normalizeWalletId('g'.repeat(68))).toBeNull();
        expect(normalizeWalletId('zz' + 'a'.repeat(66))).toBeNull();
    });

    it('strips 0x prefix', () => {
        var wid = '0x' + 'a'.repeat(66);
        var result = normalizeWalletId(wid);
        expect(result.length).toBe(68);
        expect(result).toBe('00' + 'a'.repeat(66));
    });

    it('strips whitespace', () => {
        var wid = ' ' + 'a'.repeat(68) + ' ';
        expect(normalizeWalletId(wid)).toBe('a'.repeat(68));
    });

    it('handles mixed case hex', () => {
        var wid = 'aAbBcC' + 'd'.repeat(62);
        expect(normalizeWalletId(wid)).toBe(wid);
    });
});

describe('isValidWalletId', () => {
    it('returns true for valid 68-char hex', () => {
        expect(isValidWalletId('a'.repeat(68))).toBe(true);
    });

    it('returns true for valid 67-char hex (normalizable)', () => {
        expect(isValidWalletId('a'.repeat(67))).toBe(true);
    });

    it('returns false for too short', () => {
        expect(isValidWalletId('abc')).toBe(false);
    });

    it('returns false for non-hex', () => {
        expect(isValidWalletId('x'.repeat(68))).toBe(false);
    });
});
