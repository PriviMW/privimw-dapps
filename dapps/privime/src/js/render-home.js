'use strict';

import { myHandle, conversations, contacts, unreadCounts } from './state.js';
import { escHtml, escAttr, formatTs, shortWalletId } from './helpers.js';

// ================================================================
// HOME RENDERING
// ================================================================
export function renderHome() {
    if (!myHandle) {
        document.getElementById('homeHandleLabel').textContent = '';
        document.getElementById('homeHero').style.display = 'flex';
        document.getElementById('homeContacts').style.display = 'none';
    } else {
        document.getElementById('homeHandleLabel').textContent = '@' + myHandle.handle;
        document.getElementById('homeHero').style.display = 'none';
        var c = document.getElementById('homeContacts');
        c.style.display = 'flex';
        c.style.flexDirection = 'column';
        renderContactList();
    }
}

export function renderContactList() {
    var list = document.getElementById('contactList');
    var walletIds = Object.keys(conversations);

    if (walletIds.length === 0) {
        list.innerHTML = '<div class="contacts-empty">No conversations yet.<br>Search for a @handle to start chatting.</div>';
        return;
    }

    // Sort by latest message timestamp (newest first)
    walletIds.sort(function(a, b) {
        var aLast = lastMessage(conversations[a]);
        var bLast = lastMessage(conversations[b]);
        return (bLast ? bLast.ts : 0) - (aLast ? aLast.ts : 0);
    });

    list.innerHTML = walletIds.map(function(wid) {
        var contact = contacts[wid] || {};
        var handle  = contact.handle || shortWalletId(wid);
        var name    = contact.display_name || '';
        var last    = lastMessage(conversations[wid]);
        var unread  = unreadCounts[wid] || 0;
        var preview = '';
        if (last && last.file) {
            preview = '\u{1F4CE} ' + escHtml(last.file.name || 'File');
        } else if (last) {
            preview = escHtml(last.text.substring(0, 60));
        }
        var timeStr = last ? formatTs(last.ts) : '';
        var initial = handle.charAt(0).toUpperCase();
        var badge   = unread > 0 ? '<span class="unread-badge">' + Math.min(unread, 9) + '</span>' : '';

        var displayLabel = name ? escHtml(name) : '@' + escHtml(handle);

        return '<div class="contact-item" onclick="showChatPage(\'' + escAttr(wid) + '\')" oncontextmenu="showContactContextMenu(event,\'' + escAttr(wid) + '\')">' +
            '<div class="contact-avatar">' + initial + badge + '</div>' +
            '<div class="contact-info">' +
                '<div class="contact-name">' + displayLabel + '</div>' +
                '<div class="contact-preview">' + (last && last.sent ? '<span style="color:var(--text-muted);">You: </span>' : '') + preview + '</div>' +
            '</div>' +
            '<div class="contact-meta"><div class="contact-time">' + timeStr + '</div></div>' +
        '</div>';
    }).join('');
}

export function lastMessage(msgs) {
    if (!msgs || msgs.length === 0) return null;
    return msgs[msgs.length - 1];
}
