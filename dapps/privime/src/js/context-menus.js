'use strict';

import { activeChat, contacts, conversations, unreadCounts, deletedConvs,
         chatMsgCache, bubbleCtxMsg, setBubbleCtxMsg,
         bubbleCtxWid, setBubbleCtxWid,
         myHandle, forwardText, setForwardText,
         setForwardMode, setReplyTo,
         emojiPanelOpen } from './state.js';
import { escHtml, escAttr, shortWalletId, showToast, extractError, copyToClipboard } from './helpers.js';
import { saveToStorage } from './storage.js';
import { renderContactList, lastMessage } from './render-home.js';
import { renderChatMessages } from './render-chat.js';
import { sendSbbsMessage } from './sbbs.js';
import { isValidWalletId } from './registration.js';
import { onChatInput, cancelReply } from './chat-input.js';
import { showChatPage } from './navigation.js';
import { hideKebabMenu, kebabOpen } from './kebab.js';
import { closeEmojiPanel } from './emoji.js';
import { closeSlashPopup, slashPopupOpen } from './slash-commands.js';
import { closeChatSearch } from './chat-search.js';

// ================================================================
// CONTACT CONTEXT MENU
// ================================================================
var contextMenuTarget = null;

export function showContactContextMenu(e, wid) {
    e.preventDefault();
    e.stopPropagation();
    contextMenuTarget = wid;
    var menu = document.getElementById('contextMenu');
    var x = Math.min(e.clientX, window.innerWidth - 180);
    var y = Math.min(e.clientY, window.innerHeight - 60);
    menu.style.left = x + 'px';
    menu.style.top  = y + 'px';
    menu.classList.add('active');
}

export function hideContextMenu() {
    document.getElementById('contextMenu').classList.remove('active');
    contextMenuTarget = null;
}

export function deleteChat() {
    if (!contextMenuTarget) return;
    var wid = contextMenuTarget;
    hideContextMenu();
    // Tombstone -- record max ts so re-imported messages are filtered on next startup
    var msgs = conversations[wid] || [];
    var maxTs = msgs.reduce(function(m, x) { return Math.max(m, x.ts); }, 0) || Math.floor(Date.now() / 1000);
    deletedConvs[wid] = maxTs;
    var contact = contacts[wid] || {};
    if (contact.handle) deletedConvs['@' + contact.handle] = maxTs;
    delete conversations[wid];
    delete unreadCounts[wid];
    saveToStorage();
    renderContactList();
}

// ================================================================
// BUBBLE CONTEXT MENU
// ================================================================
export function showBubbleMenu(e, idx) {
    e.preventDefault();
    e.stopPropagation();
    hideBubbleMenu();
    setBubbleCtxMsg(chatMsgCache[idx] || null);
    setBubbleCtxWid(activeChat);
    var menu = document.getElementById('bubbleContextMenu');
    var x = Math.min(e.clientX, window.innerWidth - 190);
    var y = Math.min(e.clientY, window.innerHeight - 150);
    menu.style.left = x + 'px';
    menu.style.top  = y + 'px';
    menu.classList.add('active');
}

export function hideBubbleMenu() {
    document.getElementById('bubbleContextMenu').classList.remove('active');
}

export function doCopyBubble() {
    hideBubbleMenu();
    if (bubbleCtxMsg) copyToClipboard(bubbleCtxMsg.text);
}

export function doReplyBubble() {
    hideBubbleMenu();
    if (!bubbleCtxMsg) return;
    setReplyTo({ text: bubbleCtxMsg.text });
    var bar = document.getElementById('replyBar');
    bar.style.display = 'flex';
    document.getElementById('replyBarText').textContent = bubbleCtxMsg.text;
    document.getElementById('chatInput').focus();
}

export function doForwardBubble() {
    hideBubbleMenu();
    if (!bubbleCtxMsg) return;
    setForwardText(bubbleCtxMsg.text);
    setForwardMode(true);
    showForwardModal();
}

function showForwardModal() {
    document.getElementById('forwardSearchInput').value = '';
    renderForwardContacts('');
    document.getElementById('forwardModal').classList.add('active');
    setTimeout(function() { document.getElementById('forwardSearchInput').focus(); }, 100);
}

export function closeForwardModal() {
    document.getElementById('forwardModal').classList.remove('active');
    setForwardMode(false);
    setForwardText(null);
}

export function onForwardSearch(val) {
    var q = val.trim().toLowerCase().replace(/^@/, '');
    renderForwardContacts(q);
}

function renderForwardContacts(filter) {
    var list = document.getElementById('forwardContactList');
    var items = [];

    // Show existing conversations as selectable contacts
    var walletIds = Object.keys(conversations);
    walletIds.sort(function(a, b) {
        var aLast = lastMessage(conversations[a]);
        var bLast = lastMessage(conversations[b]);
        return (bLast ? bLast.ts : 0) - (aLast ? aLast.ts : 0);
    });

    walletIds.forEach(function(wid) {
        var contact = contacts[wid] || {};
        var handle  = contact.handle || (wid.startsWith('@') ? wid.slice(1) : shortWalletId(wid));
        var name    = contact.display_name || '';

        // Filter
        if (filter && handle.indexOf(filter) === -1 && name.toLowerCase().indexOf(filter) === -1) return;
        // Don't show self
        if (myHandle && handle.toLowerCase() === myHandle.handle.toLowerCase()) return;

        var initial = handle.charAt(0).toUpperCase();
        var displayLabel = name ? escHtml(name) : '@' + escHtml(handle);

        items.push(
            '<div class="contact-item" onclick="doForwardTo(\'' + escAttr(wid) + '\')">' +
            '<div class="contact-avatar" style="width:40px;height:40px;font-size:16px;">' + initial + '</div>' +
            '<div class="contact-info">' +
                '<div class="contact-name">' + displayLabel + '</div>' +
            '</div></div>'
        );
    });

    if (items.length === 0 && !filter) {
        list.innerHTML = '<div style="padding:24px;text-align:center;color:var(--text-muted);font-size:13px;">No contacts yet</div>';
    } else if (items.length === 0) {
        list.innerHTML = '<div style="padding:16px;text-align:center;color:var(--text-muted);font-size:13px;">No matches for &ldquo;' + escHtml(filter) + '&rdquo;</div>';
    } else {
        list.innerHTML = items.join('');
    }
}

export function doForwardTo(wid) {
    if (!forwardText) { closeForwardModal(); return; }
    var text = forwardText;
    closeForwardModal();
    showChatPage(wid);

    // Actually send the message
    var receiver = contacts[wid] && contacts[wid].wallet_id;
    if (!receiver || !isValidWalletId(receiver)) {
        // Pre-fill input instead if address not resolved
        document.getElementById('chatInput').value = text;
        onChatInput();
        showToast('Address not resolved \u2014 message pre-filled', 'warning');
        return;
    }

    var msgObj = { v: 1, t: 'dm', msg: text, ts: Math.floor(Date.now() / 1000) };
    if (myHandle && myHandle.handle) msgObj.from = myHandle.handle;
    if (myHandle && myHandle.display_name) msgObj.dn = myHandle.display_name;
    var recipientHandle = wid.startsWith('@') ? wid.slice(1) : (contacts[wid] && contacts[wid].handle);
    if (recipientHandle) msgObj.to = recipientHandle;

    if (!conversations[wid]) conversations[wid] = [];
    conversations[wid].push({ text: text, ts: msgObj.ts, sent: true, reply: null });
    saveToStorage();
    renderChatMessages(wid);

    sendSbbsMessage(receiver, msgObj, function(result) {
        if (result && result.error) {
            conversations[wid] = conversations[wid].filter(function(m) {
                return !(m.sent && m.ts === msgObj.ts && m.text === text);
            });
            saveToStorage();
            renderChatMessages(wid);
            showToast('Forward failed: ' + extractError(result), 'error');
        } else {
            showToast('Message forwarded', 'success');
            setTimeout(function() { sendSbbsMessage(receiver, msgObj, function() {}); }, 5000);
            setTimeout(function() { sendSbbsMessage(receiver, msgObj, function() {}); }, 10000);
        }
    });
}

export function doDeleteBubble() {
    hideBubbleMenu();
    if (!bubbleCtxMsg || !bubbleCtxWid) return;
    var convs = conversations[bubbleCtxWid];
    if (!convs) return;
    var msg = bubbleCtxMsg;
    conversations[bubbleCtxWid] = convs.filter(function(m) {
        return !(m.ts === msg.ts && m.text === msg.text && m.sent === msg.sent);
    });
    saveToStorage();
    renderChatMessages(bubbleCtxWid);
}

// ================================================================
// GLOBAL EVENT LISTENERS
// ================================================================
export function setupGlobalListeners() {
    document.addEventListener('click', function(e) {
        hideContextMenu(); hideBubbleMenu();
        // Close kebab menu if click is outside
        if (kebabOpen && !e.target.closest('.kebab-menu') && !e.target.closest('.kebab-btn')) {
            hideKebabMenu();
        }
        // Close emoji panel if click is outside it and the emoji button
        if (emojiPanelOpen && !e.target.closest('.emoji-panel') && !e.target.closest('.emoji-btn')) {
            closeEmojiPanel();
        }
        // Close slash popup if click is outside
        if (slashPopupOpen && !e.target.closest('.slash-cmd-popup') && !e.target.closest('.slash-btn') && !e.target.closest('.chat-input')) {
            closeSlashPopup();
        }
    });
    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape') {
            hideContextMenu(); hideBubbleMenu(); cancelReply(); closeEmojiPanel();
            closeSlashPopup(); hideKebabMenu(); closeChatSearch();
            closeForwardModal();
            document.getElementById('confirmModal').classList.remove('active');
        }
    });
}
