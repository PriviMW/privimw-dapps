'use strict';

import { shaderBytes } from './state.js';
import { showToast, extractError, formatFee } from './helpers.js';
import { privimeInvoke, privimeTx } from './shader.js';
import { callApi } from './wallet-api.js';

// ================================================================
// HANDLE REGISTRATION
// ================================================================
var handleValid   = false;
var walletIdValid = false;

// applyMyHandle callback -- set by startup.js to break circular dependency.
var _applyMyHandleFn = null;
export function registerApplyMyHandle(fn) { _applyMyHandleFn = fn; }

export function onHandleInput(val) {
    var wrap = document.getElementById('handleInputWrap');
    var hint = document.getElementById('handleHint');
    wrap.className = 'form-input-wrap';

    if (handleCheckTimerLocal) clearTimeout(handleCheckTimerLocal);

    var h = val.toLowerCase().replace(/[^a-z0-9_]/g, '');
    if (val !== h) document.getElementById('handleInput').value = h;
    val = h;

    handleValid = false;
    updateRegisterBtn();

    if (val.length < 3) {
        hint.textContent = val.length === 0 ? '3\u201332 chars \u00B7 a-z, 0-9, underscore' : 'Too short (min 3 chars)';
        hint.className = 'form-hint';
        return;
    }
    if (val.length > 32) {
        hint.textContent = 'Too long (max 32 chars)';
        hint.className = 'form-hint';
        return;
    }

    if (!shaderBytes) {
        hint.textContent = 'Still connecting \u2014 try again in a moment';
        hint.className = 'form-hint';
        return;
    }

    hint.textContent = 'Checking availability...';
    hint.className = 'form-hint checking';

    handleCheckTimerLocal = setTimeout(function() {
        privimeInvoke('user', 'resolve_handle', { handle: val }, function(result) {
            // Stale check (user typed something else)
            var currentVal = document.getElementById('handleInput') ? document.getElementById('handleInput').value : '?';
            if (val !== currentVal) return;
            var errStr = result && result.error ? (typeof result.error === 'string' ? result.error : (result.error.message || '')) : '';
            if (errStr === 'handle not found') {
                handleValid = true;
                wrap.className = 'form-input-wrap valid';
                hint.textContent = '\u2713 Available';
                hint.className = 'form-hint available';
            } else if (result && !result.error && result.wallet_id) {
                handleValid = false;
                wrap.className = 'form-input-wrap invalid';
                hint.textContent = '\u2717 Already taken';
                hint.className = 'form-hint taken';
            } else {
                hint.textContent = 'Could not check \u2014 check connection and retry';
                hint.className = 'form-hint';
            }
            updateRegisterBtn();
        });
    }, 600);
}

var handleCheckTimerLocal = null;

// Beam SBBS addresses can display with variable length (leading zeros stripped):
//   65/67 = odd hex  -> prepend '0' to make even
//   66    = 33 bytes -> pad to 68
//   68    = 34 bytes -> exact match (contract format)
// normalizeWalletId always returns a 68-char hex string or null.
export function normalizeWalletId(val) {
    val = val.replace(/\s/g, '');
    if (val.slice(0, 2).toLowerCase() === '0x') val = val.slice(2);
    if (!/^[0-9a-fA-F]*$/.test(val)) return null;
    if (val.length % 2 !== 0) val = '0' + val;      // odd -> prepend 0
    if (val.length < 62 || val.length > 68) return null; // too short/long
    if (val.length < 68) val = val.padStart(68, '0'); // left-pad to 34 bytes
    return val; // always 68 hex chars
}

export function isValidWalletId(val) {
    return normalizeWalletId(val) !== null;
}

export function onWalletIdInput() {
    var val = document.getElementById('walletIdInput').value.trim();
    var hint = document.getElementById('walletIdHint');
    walletIdValid = isValidWalletId(val);
    if (val.length === 0) {
        hint.textContent = 'Your wallet address for receiving messages.';
        hint.className = 'form-hint';
    } else if (walletIdValid) {
        var normalized = normalizeWalletId(val);
        var paddedNote = (val.replace(/\s/g, '').replace(/^0x/i, '').length < 68) ? ' (padded to 34 bytes)' : '';
        hint.textContent = '\u2713 Valid address' + paddedNote;
        hint.className = 'form-hint available';
    } else if (/[^0-9a-fA-F\s]/.test(val.replace(/^0x/i, ''))) {
        hint.textContent = 'Must be hex characters only';
        hint.className = 'form-hint taken';
    } else {
        hint.textContent = val.replace(/\s/g, '').length + ' chars \u2014 expected 66\u201368 (paste from wallet Receive tab)';
        hint.className = 'form-hint taken';
    }
    updateRegisterBtn();
}

function updateRegisterBtn() {
    document.getElementById('registerBtn').disabled = !(handleValid && walletIdValid);
}

export function tryAutoFillWalletId() {
    // ALWAYS call create_address to generate a fresh address for THIS wallet instance.
    // Never reuse myWalletId -- it may be stale from a different wallet's session
    // (localStorage is shared across wallet instances in the same Beam Desktop app).
    callApi('create_address', { type: 'regular', expiration: 'never' }, function(result) {
        var addr = typeof result === 'string' ? result : (result && (result.address || result.result || ''));
        var normalized = addr ? normalizeWalletId(addr) : null;
        if (normalized) {
            document.getElementById('walletIdInput').value = normalized;
            onWalletIdInput();
            showToast('Address created for this wallet', 'success');
            return;
        }
        showToast('Auto-fill unavailable \u2014 paste your address from the wallet Receive tab', 'warning');
    });
}

export function doRegisterHandle() {
    if (!handleValid || !walletIdValid) return;
    var handle  = document.getElementById('handleInput').value.trim();
    var walletIdRaw = document.getElementById('walletIdInput').value.trim();
    var walletId = normalizeWalletId(walletIdRaw); // always 68 hex chars (34 bytes)
    var displayName = document.getElementById('displayNameInput').value.trim();

    if (!walletId) { showToast('Invalid wallet address', 'error'); return; }

    var btn = document.getElementById('registerBtn');
    btn.disabled = true;
    btn.textContent = 'Preparing...';

    var extra = { handle: handle, wallet_id: walletId };
    if (displayName) extra.display_name = displayName;

    privimeTx('user', 'register_handle', extra,
        function() { btn.textContent = 'Confirm in wallet...'; },
        function(result) {
            btn.disabled = false;
            btn.innerHTML = 'Register @handle \u2014 <span id="regBtnFee">' + formatFee() + '</span>';
            if (result && result.error) {
                showToast('Registration failed: ' + extractError(result), 'error');
                return;
            }
            showToast('@' + handle + ' registered!', 'success');
            // Apply identity immediately from form data -- TX is submitted but not yet confirmed.
            // Chain confirmation arrives later via ev_txs_changed; myHandle is already set at
            // that point so only loadMessages() fires, not a full re-query of the contract.
            if (_applyMyHandleFn) {
                _applyMyHandleFn({ registered: 1, handle: handle, wallet_id: walletId, display_name: displayName || '' });
            }
        }
    );
}
