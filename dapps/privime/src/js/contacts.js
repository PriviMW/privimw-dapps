'use strict';

import { contacts, conversations, activeChat, setActiveChat,
         unreadCounts, deletedConvs, currentPage } from './state.js';
import { privimeInvoke } from './shader.js';
import { saveToStorage } from './storage.js';
import { extractError, fixBvmUtf8 } from './helpers.js';
import { normalizeWalletId } from './registration.js';
import { renderContactList } from './render-home.js';
import { renderChatMessages } from './render-chat.js';

// ================================================================
// CONTACT RESOLUTION
// ================================================================
export function resolveWalletIdToContact(walletId) {
    if (contacts[walletId] && contacts[walletId].handle) return;
    var norm = normalizeWalletId(walletId) || walletId;
    privimeInvoke('user', 'resolve_walletid', { walletid: norm }, function(result) {
        if (contacts[walletId]) contacts[walletId]._resolving = false;
        if (result && !result.error && result.handle) {
            contacts[walletId] = contacts[walletId] || {};
            contacts[walletId].handle = result.handle;
            contacts[walletId].display_name = fixBvmUtf8(result.display_name) || '';
            if (currentPage === 'home') renderContactList();
            if (currentPage === 'chat' && activeChat === walletId) renderChatMessages(walletId);
        }
    });
}

// Scan '@handle' contacts to find one with a matching wallet_id (for legacy sent messages).
// Returns null if not found or if multiple handles share the same address (ambiguous).
export function findHandleByWalletId(walletId) {
    for (var key in contacts) {
        if (key.charAt(0) === '@' && contacts[key].wallet_id === walletId) {
            return contacts[key].handle;
        }
    }
    return null;
}

// Resolve @handle -> receive address, populating contacts[convKey].wallet_id for sending.
// convKey is always '@handle'. The handle stays as the permanent conversation key -- we do NOT
// merge into receiveAddr, because multiple handles can share the same wallet_id on the contract.
export function resolveHandleIntoContact(handle, convKey) {
    privimeInvoke('user', 'resolve_handle', { handle: handle }, function(result) {
        if (contacts[convKey]) contacts[convKey]._resolving = false;
        if (result && !result.error && result.wallet_id) {
            var receiveAddr = result.wallet_id;

            // Populate '@handle' contact with wallet_id (needed for sending replies)
            if (!contacts[convKey]) contacts[convKey] = {};
            contacts[convKey].handle = handle;
            contacts[convKey].wallet_id = receiveAddr;
            contacts[convKey].display_name = fixBvmUtf8(result.display_name) || '';
            // Do NOT index contacts[receiveAddr] -- multiple handles can share the same
            // wallet_id (accounts on the same Beam wallet). Indexing by wallet_id causes
            // the last-resolved handle to overwrite all others.

            // Propagate tombstone from '@handle' to wallet_id (for tombstone checks on sent msgs)
            if (deletedConvs[convKey] && !deletedConvs[receiveAddr]) {
                deletedConvs[receiveAddr] = deletedConvs[convKey];
            }

            // Migrate legacy wallet_id-keyed conversations -> '@handle' key.
            // Old code stored conversations at receiveAddr; new code uses '@handle'.
            if (receiveAddr !== convKey && conversations[receiveAddr]) {
                if (!conversations[convKey]) conversations[convKey] = [];
                conversations[receiveAddr].forEach(function(msg) {
                    var dup = conversations[convKey].some(function(x) {
                        return x.ts === msg.ts && x.text === msg.text && x.sent === msg.sent;
                    });
                    if (!dup) conversations[convKey].push(msg);
                });
                conversations[convKey].sort(function(a, b) { return a.ts - b.ts; });
                delete conversations[receiveAddr];
                delete unreadCounts[receiveAddr];
                if (activeChat === receiveAddr) {
                    setActiveChat(convKey);
                    unreadCounts[convKey] = 0;
                }
            }

            saveToStorage();
            if (currentPage === 'home') renderContactList();
            if (currentPage === 'chat' && (activeChat === convKey || activeChat === receiveAddr)) {
                renderChatMessages(activeChat);
            }
        }
    });
}

export function resolveHandleToContact(handle, callback) {
    privimeInvoke('user', 'resolve_handle', { handle: handle.toLowerCase() }, function(result) {
        if (result && !result.error && result.wallet_id) {
            var wid = result.wallet_id;
            var hKey = '@' + handle.toLowerCase();
            contacts[hKey] = { handle: handle.toLowerCase(), wallet_id: wid, display_name: fixBvmUtf8(result.display_name) || '' };
            callback(null, { wallet_id: wid, handle: handle.toLowerCase(), display_name: fixBvmUtf8(result.display_name) || '', registered_height: result.registered_height });
        } else {
            callback(result && result.error ? extractError(result) : 'Not found');
        }
    });
}
