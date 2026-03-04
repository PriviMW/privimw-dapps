'use strict';

import { PRIVIME_CID } from './config.js';
import { conversations, contacts, deletedConvs, unreadCounts,
         myHandle, setConversations, setContacts,
         setDeletedConvs, setUnreadCounts, setMyHandle, setMyWalletId } from './state.js';
import { normalizeWalletId } from './registration.js';

// ================================================================
// LOCAL STORAGE PERSISTENCE
// Storage is scoped per-handle so different wallets never share data.
// Base key: privime_<cid16>
// Handle key: privime_<cid16>_<handle>
// lasthandle: remembers which handle to load on next cold start (for instant UI)
// ================================================================
export var STORAGE_KEY = 'privime_' + PRIVIME_CID.slice(0, 16);
export var userStorageKey = STORAGE_KEY; // updated to per-handle key once identity is known

// Bump this when conversation keying logic changes -- forces a one-time rebuild from read_messages.
export var CONV_VERSION = 5;

// Contract start timestamp -- filters out SBBS messages from previous contract instances.
// Set once on first run with a new CID (STORAGE_KEY changes when CID changes).
export var contractStartTs = (function() {
    var key = STORAGE_KEY + '_startts';
    var stored = localStorage.getItem(key);
    if (stored) return parseInt(stored);
    var now = Math.floor(Date.now() / 1000);
    localStorage.setItem(key, now);
    return now;
})();

export function setUserStorageScope(handle) {
    userStorageKey = STORAGE_KEY + '_' + handle.toLowerCase();
}

export function saveToStorage() {
    try {
        localStorage.setItem(userStorageKey + '_conv', JSON.stringify(conversations));
        localStorage.setItem(userStorageKey + '_contacts', JSON.stringify(contacts));
        localStorage.setItem(userStorageKey + '_deleted', JSON.stringify(deletedConvs));
        localStorage.setItem(userStorageKey + '_unread', JSON.stringify(unreadCounts));
        localStorage.setItem(userStorageKey + '_convver', CONV_VERSION);
        if (myHandle) {
            localStorage.setItem(userStorageKey + '_handle', JSON.stringify(myHandle));
            // Remember last handle for instant startup next time
            localStorage.setItem(STORAGE_KEY + '_lasthandle', myHandle.handle.toLowerCase());
        }
    } catch(e) {}
}

// loadBlockedUsers callback -- set by block-user.js to break circular dependency.
// storage.js never imports block-user.js; instead block-user.js registers its loader here.
var _loadBlockedUsersFn = null;
export function registerLoadBlockedUsers(fn) { _loadBlockedUsersFn = fn; }

export function loadFromStorage() {
    try {
        // Use last known handle to scope the key (enables instant startup)
        var lastHandle = localStorage.getItem(STORAGE_KEY + '_lasthandle');
        if (lastHandle) setUserStorageScope(lastHandle);

        // If conversation keying changed (CONV_VERSION bump), discard old conversation
        // cache so it's rebuilt correctly from read_messages(all=true) on startup.
        // Contacts, handle info, and tombstones are preserved.
        var storedVersion = parseInt(localStorage.getItem(userStorageKey + '_convver') || '1');
        if (storedVersion < CONV_VERSION) {
            localStorage.removeItem(userStorageKey + '_conv');
            localStorage.removeItem(userStorageKey + '_contacts'); // clear stale wallet_id mappings
            localStorage.removeItem(userStorageKey + '_unread');
            localStorage.setItem(userStorageKey + '_convver', CONV_VERSION);
        }

        var saved = localStorage.getItem(userStorageKey + '_conv');
        if (saved) setConversations(JSON.parse(saved));
        saved = localStorage.getItem(userStorageKey + '_contacts');
        if (saved) setContacts(JSON.parse(saved));
        saved = localStorage.getItem(userStorageKey + '_deleted');
        if (saved) try { setDeletedConvs(JSON.parse(saved)); } catch(e) {}
        saved = localStorage.getItem(userStorageKey + '_unread');
        if (saved) try { setUnreadCounts(JSON.parse(saved)); } catch(e) {}
        saved = localStorage.getItem(userStorageKey + '_handle');
        if (saved) {
            var h = JSON.parse(saved);
            if (h && h.handle) {
                setMyHandle(h);
                setMyWalletId(normalizeWalletId(h.wallet_id) || h.wallet_id);
            }
        }
        if (_loadBlockedUsersFn) _loadBlockedUsersFn();
    } catch(e) {}
}
