'use strict';

import { GROTH_PER_BEAM } from './config.js';
import { myHandle, activeChat, contacts } from './state.js';
import { showToast, extractError, escHtml } from './helpers.js';
import { saveToStorage } from './storage.js';
import { privimeTx } from './shader.js';
import { callApi } from './wallet-api.js';
import { sendSbbsWithRetry } from './sbbs.js';
import { isValidWalletId } from './registration.js';
import { renderChatMessages } from './render-chat.js';
import { closeEmojiPanel } from './emoji.js';
import { onChatInput } from './chat-input.js';
import { conversations } from './state.js';

// ================================================================
// SLASH COMMANDS
// ================================================================
export var slashPopupOpen = false;
export var slashSelectedIdx = 0;
export function setSlashSelectedIdx(v) { slashSelectedIdx = v; }

export var SLASH_COMMANDS = [
    { name: '/rename', desc: 'Change your display name', enabled: true },
    { name: '/tip', desc: 'Send BEAM to this user', enabled: true }
];

export function toggleSlashPopup() {
    if (slashPopupOpen) { closeSlashPopup(); return; }
    openSlashPopup();
}

export function openSlashPopup(filter) {
    var popup = document.getElementById('slashCmdPopup');
    var q = (filter || '').toLowerCase().replace(/^\//, '');
    var items = SLASH_COMMANDS.filter(function(c) {
        return q.length === 0 || c.name.toLowerCase().indexOf(q) !== -1;
    });
    if (items.length === 0) { closeSlashPopup(); return; }
    slashSelectedIdx = 0;
    popup.innerHTML = items.map(function(c, i) {
        var cls = 'slash-cmd-item' + (c.enabled ? '' : ' disabled') + (i === 0 ? ' selected' : '');
        return '<div class="' + cls + '" data-cmd="' + c.name + '" onclick="selectSlashCommand(\'' + c.name + '\',' + c.enabled + ')">' +
            '<div><div class="slash-cmd-name">' + escHtml(c.name) + '</div>' +
            '<div class="slash-cmd-desc">' + escHtml(c.desc) + '</div></div></div>';
    }).join('');
    popup.classList.add('active');
    slashPopupOpen = true;
    closeEmojiPanel();
}

export function closeSlashPopup() {
    document.getElementById('slashCmdPopup').classList.remove('active');
    slashPopupOpen = false;
}

export function selectSlashCommand(cmd, enabled) {
    closeSlashPopup();
    if (!enabled) { showToast(cmd + ' is coming soon', 'info'); return; }
    var input = document.getElementById('chatInput');
    input.value = cmd + ' ';
    input.focus();
    onChatInput();
}

export function handleSlashCommand(text) {
    var parts = text.trim().split(/\s+/);
    var cmd = parts[0].toLowerCase();

    if (cmd === '/rename') {
        var newName = text.substring(parts[0].length).trim();
        if (!newName) {
            showToast('Usage: /rename YourNewName', 'warning');
            return true;
        }
        if (newName.length > 64) {
            showToast('Display name too long (max 64 chars)', 'warning');
            return true;
        }
        if (!myHandle) {
            showToast('Register a @handle first', 'warning');
            return true;
        }
        doSlashRename(newName);
        return true;
    }

    if (cmd === '/tip') {
        var amountStr = text.substring(parts[0].length).trim();
        if (!amountStr) {
            showToast('Usage: /tip <amount>  (e.g. /tip 5)', 'warning');
            return true;
        }
        var tipAmt = parseFloat(amountStr);
        if (isNaN(tipAmt) || tipAmt <= 0) {
            showToast('Invalid amount. Usage: /tip 5', 'warning');
            return true;
        }
        if (tipAmt < 0.001) {
            showToast('Minimum tip is 0.001 BEAM', 'warning');
            return true;
        }
        if (!myHandle) {
            showToast('Register a @handle first', 'warning');
            return true;
        }
        if (!activeChat || !activeChat.startsWith('@')) {
            showToast('Open a chat with a @handle user first', 'warning');
            return true;
        }
        var recipientHandle = activeChat.slice(1);
        if (myHandle.handle.toLowerCase() === recipientHandle.toLowerCase()) {
            showToast('You cannot tip yourself', 'warning');
            return true;
        }
        var recipientContact = contacts[activeChat];
        if (!recipientContact || !recipientContact.wallet_id || !isValidWalletId(recipientContact.wallet_id)) {
            showToast('Recipient address not resolved yet. Try again in a moment.', 'warning');
            return true;
        }
        doSlashTip(tipAmt, recipientHandle, recipientContact.wallet_id);
        return true;
    }

    return false; // not a slash command
}

function doSlashRename(newName) {
    showToast('Updating display name...', 'info');
    var extra = { wallet_id: myHandle.wallet_id };
    if (newName) extra.display_name = newName;

    privimeTx('user', 'update_profile', extra,
        function() { showToast('Confirm in wallet...', 'info'); },
        function(result) {
            if (result && result.error) {
                showToast('Rename failed: ' + extractError(result), 'error');
                return;
            }
            showToast('Display name changed to "' + newName + '"', 'success');
            myHandle.display_name = newName;
            saveToStorage();
        }
    );
}

function doSlashTip(beamAmount, recipientHandle, recipientWalletId) {
    var groth = Math.round(beamAmount * GROTH_PER_BEAM);
    showToast('Sending ' + beamAmount + ' BEAM to @' + recipientHandle + '...', 'info');

    callApi('tx_send', { value: groth, address: recipientWalletId, asset_id: 0 }, function(result) {
        if (result && result.error) {
            showToast('Tip failed: ' + (result.error.message || result.error), 'error');
            return;
        }
        showToast('Tip sent! ' + beamAmount + ' BEAM to @' + recipientHandle, 'success');

        // Send SBBS notification so recipient sees the tip in chat
        var tipMsg = {
            v: 1,
            t: 'tip',
            msg: 'Sent you ' + beamAmount + ' BEAM',
            ts: Math.floor(Date.now() / 1000),
            amount: groth,
            from: myHandle.handle,
            dn: myHandle.display_name || '',
            to: recipientHandle
        };
        sendSbbsWithRetry(recipientWalletId, tipMsg, 2);

        // Add to local conversation
        var convKey = '@' + recipientHandle;
        if (!conversations[convKey]) conversations[convKey] = [];
        conversations[convKey].push({
            text: 'Sent ' + beamAmount + ' BEAM',
            ts: tipMsg.ts,
            sent: true,
            isTip: true,
            tipAmount: groth
        });
        saveToStorage();
        renderChatMessages(convKey);
    });
}
