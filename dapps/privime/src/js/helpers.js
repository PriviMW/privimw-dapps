'use strict';

import { registrationFee } from './state.js';

// ================================================================
// HELPERS
// ================================================================
export function extractError(r) {
    if (typeof r === 'string') return r;
    if (r && r.error && typeof r.error === 'object') return r.error.message || JSON.stringify(r.error);
    if (r && typeof r.error === 'string') return r.error;
    return (r && r.message) || 'Unknown error';
}

export function shortWalletId(wid) {
    if (!wid || wid.length < 16) return wid || '';
    return wid.substring(0, 8) + '...' + wid.substring(wid.length - 8);
}

export function escHtml(str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

// Escape for use inside onclick/oncontextmenu attribute single-quoted strings
export function escAttr(str) {
    return String(str).replace(/\\/g, '\\\\').replace(/'/g, "\\'");
}

export function formatTs(ts) {
    if (!ts) return '';
    var now  = Math.floor(Date.now() / 1000);
    var diff = now - ts;
    if (diff < 60)   return 'now';
    if (diff < 3600) return Math.floor(diff / 60) + 'm';
    if (diff < 86400) return Math.floor(diff / 3600) + 'h';
    return Math.floor(diff / 86400) + 'd';
}

export function formatTime(ts) {
    if (!ts) return '';
    var d = new Date(ts * 1000);
    return d.getHours().toString().padStart(2, '0') + ':' + d.getMinutes().toString().padStart(2, '0');
}

export function formatDateSep(ts) {
    if (!ts) return '';
    var now = new Date(), d = new Date(ts * 1000);
    var today  = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    var msgDay = new Date(d.getFullYear(), d.getMonth(), d.getDate());
    var diff   = Math.round((today - msgDay) / 86400000);
    if (diff === 0) return 'Today';
    if (diff === 1) return 'Yesterday';
    return d.toLocaleDateString(undefined, {
        month: 'short', day: 'numeric',
        year: d.getFullYear() !== now.getFullYear() ? 'numeric' : undefined
    });
}

export function copyToClipboard(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(function() {
            showToast('Copied to clipboard', 'success');
        }, function() {
            copyToClipboardFallback(text);
        });
    } else {
        copyToClipboardFallback(text);
    }
}

function copyToClipboardFallback(text) {
    try {
        var ta = document.createElement('textarea');
        ta.value = text;
        ta.style.cssText = 'position:fixed;top:0;left:0;opacity:0;';
        document.body.appendChild(ta);
        ta.select();
        document.execCommand('copy');
        document.body.removeChild(ta);
        showToast('Copied to clipboard', 'success');
    } catch (e) {
        showToast('Copy failed', 'error');
    }
}

export function formatFee() {
    var f = registrationFee;
    return (f % 1 === 0 ? f.toFixed(0) : f.toFixed(2)) + ' BEAM';
}

export function updateFeeDisplay() {
    var fee = formatFee();
    var el;
    el = document.getElementById('heroFee');
    if (el) el.textContent = 'Registration fee: ' + fee + ' · Permanent';
    el = document.getElementById('regFeeInfo');
    if (el) el.innerHTML = 'Registration fee: <strong style="color:var(--text-primary);">' + fee + '</strong> · Non-refundable · Permanent until released';
    el = document.getElementById('regBtnFee');
    if (el) el.textContent = fee;
}

export function showToast(msg, type) {
    var container = document.getElementById('toastContainer');
    var toast = document.createElement('div');
    toast.className = 'toast ' + (type || 'info');
    toast.textContent = msg;
    container.appendChild(toast);
    setTimeout(function() {
        toast.style.opacity = '0';
        toast.style.transition = 'opacity 0.3s ease';
        setTimeout(function() { if (toast.parentNode) toast.parentNode.removeChild(toast); }, 300);
    }, 3000);
}

export function setSplashStatus(msg) {
    document.getElementById('splashStatus').textContent = msg;
}
