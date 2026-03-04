'use strict';

import { myHandle } from './state.js';
import { escHtml, escAttr, shortWalletId } from './helpers.js';

// ================================================================
// MY PROFILE RENDERING
// ================================================================
export function renderMyProfileContent() {
    var el = document.getElementById('myProfileContent');
    if (!myHandle) {
        el.innerHTML = '<div style="padding:24px;text-align:center;color:var(--text-muted);">No @handle registered.</div>' +
            '<div style="padding:0 16px;"><button class="btn btn-primary btn-full" onclick="showRegisterPage()">Claim @handle</button></div>';
        return;
    }

    el.innerHTML =
        '<div style="padding:32px 24px 16px;text-align:center;display:flex;flex-direction:column;align-items:center;gap:8px;">' +
        '<div class="profile-avatar-lg">' + myHandle.handle.charAt(0).toUpperCase() + '</div>' +
        '<div class="profile-handle">@' + escHtml(myHandle.handle) + '</div>' +
        (myHandle.display_name ? '<div class="profile-name">' + escHtml(myHandle.display_name) + '</div>' : '') +
        '<div class="profile-height">Registered block #' + myHandle.registered_height + '</div>' +
        '</div>' +
        '<div style="padding:0 16px;display:flex;flex-direction:column;gap:12px;">' +
        '<div class="detail-row">' +
            '<span class="detail-label">Display Name</span>' +
            '<span class="detail-value" style="font-family:inherit;">' + escHtml(myHandle.display_name || 'Not set') + '</span>' +
            '<button class="copy-btn" onclick="showEditDisplayName()">Edit</button>' +
        '</div>' +
        '<div id="editDisplayNameForm" style="display:none;">' +
        '<div style="display:flex;gap:8px;align-items:center;">' +
        '<input id="editDisplayNameInput" class="form-input-full" type="text" maxlength="64" placeholder="Your preferred name" value="' + escHtml(myHandle.display_name || '') + '">' +
        '</div>' +
        '<div style="display:flex;gap:8px;margin-top:8px;">' +
        '<button id="saveDisplayNameBtn" class="btn btn-primary" style="flex:1;" onclick="doSaveDisplayName()">Save</button>' +
        '<button class="btn btn-secondary" style="flex:1;" onclick="hideEditDisplayName()">Cancel</button>' +
        '</div></div>' +
        '<div class="detail-row">' +
            '<span class="detail-label">Wallet Address</span>' +
            '<span class="detail-value">' + shortWalletId(myHandle.wallet_id) + '</span>' +
            '<button class="copy-btn" onclick="copyToClipboard(\'' + escAttr(myHandle.wallet_id) + '\')">Copy</button>' +
        '</div>' +
        '<div id="updateAddrSection">' +
        '<button class="btn btn-secondary btn-full mt-8" onclick="showUpdateAddr()">Update Address</button>' +
        '<div class="form-hint">If your registered address expires, update it here.</div>' +
        '</div>' +
        '<div id="updateAddrForm" style="display:none;margin-top:12px;">' +
        '<div class="walletid-auto-wrap" style="margin-bottom:6px;">' +
        '<input id="updateWalletIdInput" class="form-input-full monospace" maxlength="70" placeholder="New wallet address (hex)" oninput="onUpdateWalletIdInput()">' +
        '<button class="auto-btn" onclick="tryAutoFillUpdateAddr()">Auto-fill</button>' +
        '</div>' +
        '<div id="updateWalletIdHint" class="form-hint">Paste your new permanent address.</div>' +
        '<div style="display:flex;gap:8px;margin-top:8px;">' +
        '<button id="updateAddrBtn" class="btn btn-primary" style="flex:1;" onclick="doUpdateAddress()" disabled>Update</button>' +
        '<button class="btn btn-secondary" style="flex:1;" onclick="hideUpdateAddr()">Cancel</button>' +
        '</div></div>' +
        '<div class="danger-zone">' +
        '<div class="section-title text-danger">Danger Zone</div>' +
        '<button class="btn btn-danger btn-full mt-8" onclick="confirmReleaseHandle()">Release @' + escHtml(myHandle.handle) + '</button>' +
        '<div class="form-hint mt-8">Permanently releases your @handle. No refund.</div>' +
        '</div>' +
        '</div>';
}
