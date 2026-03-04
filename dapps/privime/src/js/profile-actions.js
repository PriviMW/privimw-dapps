'use strict';

import { myHandle, setMyHandle, setMyWalletId, conversations, contacts,
         unreadCounts, setConversations, setContacts, setUnreadCounts } from './state.js';
import { showToast, extractError, escHtml } from './helpers.js';
import { saveToStorage, STORAGE_KEY } from './storage.js';
import { privimeTx } from './shader.js';
import { callApi } from './wallet-api.js';
import { normalizeWalletId, isValidWalletId } from './registration.js';
import { renderMyProfileContent } from './render-profile.js';
import { showHome } from './navigation.js';

// ================================================================
// CONFIRM MODAL
// ================================================================
export function showConfirmModal(title, body, actionLabel, onConfirm) {
    var modal = document.getElementById('confirmModal');
    document.getElementById('confirmModalTitle').textContent = title;
    document.getElementById('confirmModalBody').textContent = body;
    var btn = document.getElementById('confirmModalAction');
    btn.textContent = actionLabel;
    btn.onclick = function() { modal.classList.remove('active'); onConfirm(); };
    modal.classList.add('active');
}

// ================================================================
// UPDATE ADDRESS
// ================================================================
var updateAddrValid = false;

export function showUpdateAddr() {
    var form = document.getElementById('updateAddrForm');
    if (form) { form.style.display = 'block'; }
    document.getElementById('updateWalletIdInput').value = '';
    document.getElementById('updateWalletIdHint').textContent = 'Paste your new permanent address.';
    document.getElementById('updateWalletIdHint').className = 'form-hint';
    updateAddrValid = false;
    document.getElementById('updateAddrBtn').disabled = true;
}

export function hideUpdateAddr() {
    var form = document.getElementById('updateAddrForm');
    if (form) form.style.display = 'none';
}

export function onUpdateWalletIdInput() {
    var val = document.getElementById('updateWalletIdInput').value.trim();
    var hint = document.getElementById('updateWalletIdHint');
    updateAddrValid = isValidWalletId(val);
    if (val.length === 0) {
        hint.textContent = 'Paste your new permanent address.';
        hint.className = 'form-hint';
    } else if (updateAddrValid) {
        hint.textContent = '\u2713 Valid address';
        hint.className = 'form-hint available';
    } else {
        hint.textContent = 'Invalid \u2014 must be 66\u201368 hex chars';
        hint.className = 'form-hint taken';
    }
    document.getElementById('updateAddrBtn').disabled = !updateAddrValid;
}

export function tryAutoFillUpdateAddr() {
    // Always generate a fresh address for THIS wallet (same rationale as tryAutoFillWalletId)
    callApi('create_address', { type: 'regular', expiration: 'never' }, function(result) {
        var addr = typeof result === 'string' ? result : (result && (result.address || result.result || ''));
        var norm = addr ? normalizeWalletId(addr) : null;
        if (norm) {
            document.getElementById('updateWalletIdInput').value = norm;
            onUpdateWalletIdInput();
            return;
        }
        showToast('Auto-fill unavailable \u2014 paste your address manually', 'warning');
    });
}

export function showEditDisplayName() {
    var form = document.getElementById('editDisplayNameForm');
    if (form) form.style.display = 'block';
}

export function hideEditDisplayName() {
    var form = document.getElementById('editDisplayNameForm');
    if (form) form.style.display = 'none';
}

export function doSaveDisplayName() {
    var newName = document.getElementById('editDisplayNameInput').value.trim();
    var btn = document.getElementById('saveDisplayNameBtn');
    btn.disabled = true;
    btn.textContent = 'Preparing...';

    var extra = { wallet_id: myHandle.wallet_id };
    if (newName) extra.display_name = newName;

    privimeTx('user', 'update_profile', extra,
        function() { btn.textContent = 'Confirm in wallet...'; },
        function(result) {
            btn.disabled = false;
            btn.textContent = 'Save';
            if (result && result.error) {
                showToast('Update failed: ' + extractError(result), 'error');
                return;
            }
            showToast('Display name updated!', 'success');
            myHandle.display_name = newName;
            saveToStorage();
            hideEditDisplayName();
            renderMyProfileContent();
        }
    );
}

export function doUpdateAddress() {
    if (!updateAddrValid) return;
    var walletIdRaw = document.getElementById('updateWalletIdInput').value.trim();
    var walletId = normalizeWalletId(walletIdRaw);
    if (!walletId) { showToast('Invalid wallet address', 'error'); return; }

    var btn = document.getElementById('updateAddrBtn');
    btn.disabled = true;
    btn.textContent = 'Preparing...';

    var updateExtra = { wallet_id: walletId };
    if (myHandle && myHandle.display_name) updateExtra.display_name = myHandle.display_name;
    privimeTx('user', 'update_profile', updateExtra,
        function() { btn.textContent = 'Confirm in wallet...'; },
        function(result) {
            btn.disabled = false;
            btn.textContent = 'Update';
            if (result && result.error) {
                showToast('Update failed: ' + extractError(result), 'error');
                return;
            }
            showToast('Address updated!', 'success');
            setMyWalletId(walletId);
            if (myHandle) myHandle.wallet_id = walletId;
            hideUpdateAddr();
            renderMyProfileContent();
        }
    );
}

export function confirmReleaseHandle() {
    showConfirmModal(
        'Release @' + myHandle.handle + '?',
        'This is permanent and there is no refund.',
        'Release',
        function() {
            privimeTx('user', 'release_handle', {},
                function() { showToast('Confirm in wallet...', 'info'); },
                function(result) {
                    if (result && result.error) {
                        showToast('Failed: ' + extractError(result), 'error');
                        return;
                    }
                    showToast('@' + myHandle.handle + ' released', 'success');
                    try { localStorage.removeItem(STORAGE_KEY + '_lasthandle'); } catch(e) {}
                    setMyHandle(null);
                    setMyWalletId(null);
                    setConversations({});
                    setContacts({});
                    setUnreadCounts({});
                    showHome();
                }
            );
        }
    );
}
