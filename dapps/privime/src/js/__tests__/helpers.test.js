import { describe, it, expect, vi, beforeEach } from 'vitest';

// Mock state.js to avoid importing the full state tree
vi.mock('../state.js', () => ({
    registrationFee: 1,
}));

import { extractError, shortWalletId, escHtml, escAttr, formatTs, formatTime, formatDateSep, formatFee } from '../helpers.js';

describe('extractError', () => {
    it('returns string directly', () => {
        expect(extractError('some error')).toBe('some error');
    });

    it('extracts nested error.message', () => {
        expect(extractError({ error: { message: 'bad thing' } })).toBe('bad thing');
    });

    it('extracts string error', () => {
        expect(extractError({ error: 'failed' })).toBe('failed');
    });

    it('falls back to message property', () => {
        expect(extractError({ message: 'fallback' })).toBe('fallback');
    });

    it('returns Unknown error for null', () => {
        expect(extractError(null)).toBe('Unknown error');
    });

    it('returns Unknown error for empty object', () => {
        expect(extractError({})).toBe('Unknown error');
    });
});

describe('shortWalletId', () => {
    it('shortens long wallet ID', () => {
        var wid = '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef01234567';
        expect(shortWalletId(wid)).toBe('01234567...01234567');
    });

    it('returns empty string for null', () => {
        expect(shortWalletId(null)).toBe('');
    });

    it('returns short string as-is', () => {
        expect(shortWalletId('abc')).toBe('abc');
    });
});

describe('escHtml', () => {
    it('escapes HTML entities', () => {
        expect(escHtml('<script>alert("xss")</script>')).toBe('&lt;script&gt;alert(&quot;xss&quot;)&lt;/script&gt;');
    });

    it('escapes ampersand', () => {
        expect(escHtml('a & b')).toBe('a &amp; b');
    });

    it('escapes single quotes', () => {
        expect(escHtml("it's")).toBe('it&#39;s');
    });

    it('returns empty string for null/undefined', () => {
        expect(escHtml(null)).toBe('');
        expect(escHtml(undefined)).toBe('');
        expect(escHtml('')).toBe('');
    });
});

describe('escAttr', () => {
    it('escapes single quotes as HTML entities', () => {
        expect(escAttr("@user's")).toBe("@user&#39;s");
    });

    it('escapes double quotes', () => {
        expect(escAttr('a"b')).toBe('a&quot;b');
    });

    it('escapes backslashes', () => {
        expect(escAttr('a\\b')).toBe('a\\\\b');
    });

    it('escapes angle brackets', () => {
        expect(escAttr('<script>')).toBe('&lt;script&gt;');
    });

    it('escapes ampersands', () => {
        expect(escAttr('a&b')).toBe('a&amp;b');
    });

    it('handles normal handle keys', () => {
        expect(escAttr('@alice')).toBe('@alice');
    });

    it('handles crafted XSS attempt in handle', () => {
        // A malicious handle like: '); alert('xss
        expect(escAttr("'); alert('xss")).toBe("&#39;); alert(&#39;xss");
    });

    it('handles attribute breakout attempt', () => {
        // Attacker tries to break out of data-name="..." attribute
        expect(escAttr('" onload="alert(1)')).toBe('&quot; onload=&quot;alert(1)');
    });
});

describe('formatTs', () => {
    it('returns "now" for recent timestamps', () => {
        var now = Math.floor(Date.now() / 1000);
        expect(formatTs(now)).toBe('now');
        expect(formatTs(now - 30)).toBe('now');
    });

    it('returns minutes for 1-59 min ago', () => {
        var now = Math.floor(Date.now() / 1000);
        expect(formatTs(now - 120)).toBe('2m');
        expect(formatTs(now - 3540)).toBe('59m');
    });

    it('returns hours for 1-23 hours ago', () => {
        var now = Math.floor(Date.now() / 1000);
        expect(formatTs(now - 7200)).toBe('2h');
    });

    it('returns days for 1+ days ago', () => {
        var now = Math.floor(Date.now() / 1000);
        expect(formatTs(now - 172800)).toBe('2d');
    });

    it('returns empty for falsy values', () => {
        expect(formatTs(0)).toBe('');
        expect(formatTs(null)).toBe('');
    });
});

describe('formatTime', () => {
    it('formats timestamp to HH:MM', () => {
        // Create a timestamp for a known time
        var d = new Date(2024, 0, 15, 14, 30, 0);
        var ts = Math.floor(d.getTime() / 1000);
        expect(formatTime(ts)).toBe('14:30');
    });

    it('pads single digits', () => {
        var d = new Date(2024, 0, 15, 9, 5, 0);
        var ts = Math.floor(d.getTime() / 1000);
        expect(formatTime(ts)).toBe('09:05');
    });

    it('returns empty for falsy', () => {
        expect(formatTime(0)).toBe('');
        expect(formatTime(null)).toBe('');
    });
});

describe('formatFee', () => {
    it('formats integer fee without decimals', () => {
        // registrationFee is mocked as 1
        expect(formatFee()).toBe('1 BEAM');
    });
});
