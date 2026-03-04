'use strict';

import { activeChat, contacts, conversations, deletedConvs,
         unreadCounts } from './state.js';
import { showToast } from './helpers.js';
import { saveToStorage, userStorageKey, registerLoadBlockedUsers } from './storage.js';
import { renderChatMessages } from './render-chat.js';
import { showHome } from './navigation.js';
import { hideKebabMenu } from './kebab.js';
import { showConfirmModal } from './profile-actions.js';

// ================================================================
// BLOCK USER (client-side, localStorage)
// ================================================================
export var blockedUsers = {}; // { handle: true }

export function loadBlockedUsers() {
    try {
        var saved = localStorage.getItem(userStorageKey + '_blocked');
        if (saved) blockedUsers = JSON.parse(saved);
    } catch(e) {}
}

// Register with storage.js so loadFromStorage can call loadBlockedUsers
registerLoadBlockedUsers(loadBlockedUsers);

function saveBlockedUsers() {
    try {
        localStorage.setItem(userStorageKey + '_blocked', JSON.stringify(blockedUsers));
    } catch(e) {}
}

export function toggleBlockUser() {
    hideKebabMenu();
    var handle = activeChat && activeChat.startsWith('@') ? activeChat.slice(1) : null;
    if (!handle) return;
    if (blockedUsers[handle]) {
        delete blockedUsers[handle];
        saveBlockedUsers();
        showToast('@' + handle + ' unblocked', 'success');
    } else {
        showConfirmModal(
            'Block @' + handle + '?',
            'Messages from this user will be hidden. You can unblock from the menu.',
            'Block',
            function() {
                blockedUsers[handle] = true;
                saveBlockedUsers();
                showToast('@' + handle + ' blocked', 'success');
                renderChatMessages(activeChat);
            }
        );
    }
}

export function isBlocked(handle) {
    return handle && blockedUsers[handle.toLowerCase()] === true;
}

export function clearChatHistory() {
    hideKebabMenu();
    if (!activeChat) return;
    showConfirmModal(
        'Clear history?',
        'All messages in this chat will be deleted locally. This cannot be undone.',
        'Clear',
        function() {
            conversations[activeChat] = [];
            saveToStorage();
            renderChatMessages(activeChat);
            showToast('History cleared', 'success');
        }
    );
}

export function deleteChatFromKebab() {
    hideKebabMenu();
    if (!activeChat) return;
    showConfirmModal(
        'Delete chat?',
        'This conversation will be removed from your list.',
        'Delete',
        function() {
            var wid = activeChat;
            var msgs = conversations[wid] || [];
            var maxTs = msgs.reduce(function(m, x) { return Math.max(m, x.ts); }, 0) || Math.floor(Date.now() / 1000);
            deletedConvs[wid] = maxTs;
            var contact = contacts[wid] || {};
            if (contact.handle) deletedConvs['@' + contact.handle] = maxTs;
            delete conversations[wid];
            delete unreadCounts[wid];
            saveToStorage();
            showHome();
            showToast('Chat deleted', 'success');
        }
    );
}
