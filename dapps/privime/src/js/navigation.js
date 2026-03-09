'use strict';

import { currentPage, setCurrentPage, activeChat, setActiveChat,
         profileFrom, setProfileFrom, contacts, unreadCounts,
         myHandle } from './state.js';
import { escHtml, shortWalletId, showToast, copyToClipboard, fixBvmUtf8 } from './helpers.js';
import { saveToStorage } from './storage.js';
import { renderHome, renderContactList } from './render-home.js';
import { renderChatMessages } from './render-chat.js';
import { renderMyProfileContent } from './render-profile.js';
import { privimeInvoke } from './shader.js';
import { sendReadReceipts } from './sbbs.js';
import { resolveHandleIntoContact } from './contacts.js';
import { hideKebabMenu } from './kebab.js';
import { closeChatSearch } from './chat-search.js';
import { closeSlashPopup } from './slash-commands.js';
import { startMsgPoll } from './startup.js';
import { clearSearch } from './search-bar.js';

// ================================================================
// PAGE NAVIGATION
// ================================================================
export function showPage(id) {
    setCurrentPage(id.replace('Page', ''));
    document.querySelectorAll('.page').forEach(function(p) { p.classList.remove('active'); });
    document.getElementById(id).classList.add('active');
}

export function showHome() {
    showPage('homePage');
    clearSearch();
    renderHome();
}

export function showRegisterPage() {
    showPage('registerPage');
}

export function showChatPage(id) {
    // Normalize wallet_id -> '@handle' -- '@handle' is the permanent conversation key.
    // Search results and profile "send message" pass a wallet_id; convert to '@handle' here.
    var key = id;
    if (key && !key.startsWith('@')) {
        var c = contacts[key];
        if (c && c.handle) {
            key = '@' + c.handle;
            // Ensure the '@handle' contact entry exists with wallet_id for sending
            if (!contacts[key]) contacts[key] = {};
            contacts[key].handle = c.handle;
            contacts[key].wallet_id = c.wallet_id || id;
            contacts[key].display_name = c.display_name || '';
        }
    }
    setActiveChat(key);
    if (unreadCounts[key]) {
        unreadCounts[key] = 0;
        saveToStorage();
    }
    var contact = contacts[key] || {};
    var handle  = contact.handle || (key.startsWith('@') ? key.slice(1) : shortWalletId(key));
    var name    = contact.display_name || '';

    document.getElementById('chatAvatarLg').textContent = handle.charAt(0).toUpperCase();
    document.getElementById('chatHeaderHandle').textContent = name || '@' + handle;
    document.getElementById('chatHeaderName').textContent = '';

    // Reset chat UI state
    hideKebabMenu();
    closeChatSearch();
    closeSlashPopup();

    showPage('chatPage');
    renderChatMessages(key, true);
    startMsgPoll();
    document.getElementById('chatInput').focus();
    sendReadReceipts(key);

    // Always re-resolve handle -> wallet_id from contract to catch address updates
    if (key.startsWith('@')) {
        var h = key.slice(1);
        resolveHandleIntoContact(h, key);
    }
}

export function showContactProfile(id) {
    setProfileFrom(currentPage);
    var contact = contacts[id] || {};
    var handle  = contact.handle || (id.startsWith('@') ? id.slice(1) : shortWalletId(id));
    var name    = contact.display_name || '';
    // wallet_id for display and sending -- prefer contacts entry, fall back to id if it's a hex
    var walletId = contact.wallet_id || (id.startsWith('@') ? '' : id);

    document.getElementById('profileAvatar').textContent = handle.charAt(0).toUpperCase();
    document.getElementById('profileHandle').textContent = name || '@' + handle;
    document.getElementById('profileDisplayName').textContent = name ? '@' + handle : '';
    document.getElementById('profileHeight').textContent = '';
    document.getElementById('profileWalletId').textContent = shortWalletId(walletId || id);
    document.getElementById('profilePage').dataset.walletId = walletId || id;
    document.getElementById('profilePage').dataset.fullWalletId = walletId || id;
    document.getElementById('profilePage').dataset.handle = handle;

    privimeInvoke('user', 'resolve_handle', { handle: handle }, function(r) {
        if (r && !r.error) {
            document.getElementById('profileHeight').textContent = 'Registered block #' + r.registered_height;
            if (r.wallet_id) {
                document.getElementById('profileWalletId').textContent = shortWalletId(r.wallet_id);
                document.getElementById('profilePage').dataset.fullWalletId = r.wallet_id;
            }
            // Update display name from on-chain in case it changed
            var dn = fixBvmUtf8(r.display_name);
            if (dn && dn !== name) {
                contacts[id] = contacts[id] || {};
                contacts[id].display_name = dn;
                document.getElementById('profileHandle').textContent = dn;
                document.getElementById('profileDisplayName').textContent = '@' + handle;
                saveToStorage();
            }
        }
    });
    showPage('profilePage');
}

export function profileBack() {
    if (profileFrom === 'chat') {
        showChatPage(activeChat);
    } else {
        showHome();
    }
}

export function openChatFromProfile() {
    var handle = document.getElementById('profilePage').dataset.handle;
    if (handle) {
        showChatPage('@' + handle);
    } else {
        var walletId = document.getElementById('profilePage').dataset.walletId;
        showChatPage(walletId);
    }
}

export function copyProfileWalletId() {
    var wid = document.getElementById('profilePage').dataset.fullWalletId ||
              document.getElementById('profilePage').dataset.walletId || '';
    if (wid) copyToClipboard(wid);
    else showToast('No wallet address available', 'warning');
}

export function showMyProfile() {
    renderMyProfileContent();
    showPage('myProfilePage');
}
