'use strict';

import { activeChat, myHandle, conversations, contacts } from './state.js';
import { showToast } from './helpers.js';
import { blockedUsers } from './block-user.js';
import { showContactProfile } from './navigation.js';

// ================================================================
// KEBAB MENU
// ================================================================
export var kebabOpen = false;

export function toggleKebabMenu(e) {
    e.stopPropagation();
    var menu = document.getElementById('kebabMenu');
    kebabOpen = !kebabOpen;
    if (kebabOpen) {
        // Update block label
        var handle = activeChat && activeChat.startsWith('@') ? activeChat.slice(1) : null;
        var lbl = document.getElementById('kebabBlockLabel');
        lbl.textContent = (handle && blockedUsers[handle]) ? 'Unblock user' : 'Block user';
        menu.classList.add('active');
    } else {
        menu.classList.remove('active');
    }
}

export function hideKebabMenu() {
    document.getElementById('kebabMenu').classList.remove('active');
    kebabOpen = false;
}

export function viewChatProfile() {
    hideKebabMenu();
    if (activeChat) showContactProfile(activeChat);
}

export function exportChatHistory() {
    hideKebabMenu();
    if (!activeChat) return;
    var msgs = conversations[activeChat] || [];
    if (msgs.length === 0) { showToast('No messages to export', 'warning'); return; }
    var contact = contacts[activeChat] || {};
    var handle = contact.handle || (activeChat.startsWith('@') ? activeChat.slice(1) : 'unknown');
    var lines = ['Chat with @' + handle, 'Exported: ' + new Date().toLocaleString(), ''];
    msgs.forEach(function(m) {
        var who = m.sent ? (myHandle ? '@' + myHandle.handle : 'Me') : '@' + handle;
        var time = new Date(m.ts * 1000).toLocaleString();
        lines.push('[' + time + '] ' + who + ': ' + m.text);
    });
    var text = lines.join('\n');
    var blob = new Blob([text], { type: 'text/plain' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = 'privime-chat-' + handle + '.txt';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    showToast('Chat exported', 'success');
}
