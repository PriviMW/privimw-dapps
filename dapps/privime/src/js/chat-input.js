'use strict';

import { MAX_MSG_CHARS } from './config.js';
import { activeChat, myHandle, myWalletId, contacts, conversations,
         replyTo, setReplyTo } from './state.js';
import { showToast, extractError } from './helpers.js';
import { saveToStorage } from './storage.js';
import { sendSbbsMessage } from './sbbs.js';
import { renderChatMessages } from './render-chat.js';
import { isValidWalletId } from './registration.js';
import { resolveHandleIntoContact } from './contacts.js';
import { handleSlashCommand, openSlashPopup, closeSlashPopup, slashPopupOpen, slashSelectedIdx, setSlashSelectedIdx } from './slash-commands.js';
import { closeEmojiPanel } from './emoji.js';

// ================================================================
// CHAT INPUT
// ================================================================
export function onChatInput() {
    var input = document.getElementById('chatInput');
    var text  = input.value;
    var len   = text.length;
    var btn   = document.getElementById('sendBtn');
    var cc    = document.getElementById('charCount');

    // Auto-resize textarea
    input.style.height = 'auto';
    input.style.height = Math.min(input.scrollHeight, 120) + 'px';

    btn.disabled = (len === 0 || len > MAX_MSG_CHARS);

    // Slash command detection -- show popup when input starts with /
    if (text.startsWith('/') && !text.includes('\n')) {
        openSlashPopup(text);
    } else if (slashPopupOpen) {
        closeSlashPopup();
    }

    if (len > MAX_MSG_CHARS) {
        cc.textContent = len + ' / ' + MAX_MSG_CHARS + ' (too long)';
        cc.className = 'char-count at-limit';
    } else if (len > MAX_MSG_CHARS * 0.85) {
        cc.textContent = len + ' / ' + MAX_MSG_CHARS;
        cc.className = 'char-count near-limit';
    } else if (len > 0) {
        cc.textContent = len + ' / ' + MAX_MSG_CHARS;
        cc.className = 'char-count';
    } else {
        cc.textContent = '';
    }
}

export function onChatKeydown(e) {
    // Slash popup navigation
    if (slashPopupOpen) {
        var items = document.querySelectorAll('.slash-cmd-item');
        if (e.key === 'ArrowDown') {
            e.preventDefault();
            setSlashSelectedIdx(Math.min(slashSelectedIdx + 1, items.length - 1));
            items.forEach(function(el, i) { el.classList.toggle('selected', i === slashSelectedIdx); });
            return;
        }
        if (e.key === 'ArrowUp') {
            e.preventDefault();
            setSlashSelectedIdx(Math.max(slashSelectedIdx - 1, 0));
            items.forEach(function(el, i) { el.classList.toggle('selected', i === slashSelectedIdx); });
            return;
        }
        if (e.key === 'Tab' || (e.key === 'Enter' && !e.shiftKey)) {
            e.preventDefault();
            var sel = items[slashSelectedIdx];
            if (sel) sel.click();
            return;
        }
    }
    // Ctrl+Enter or Enter on desktop (not mobile) to send
    if (e.key === 'Enter' && !e.shiftKey && !isMobile()) {
        e.preventDefault();
        doSendMessage();
    }
}

export function isMobile() {
    return /android|iphone|ipad/i.test(navigator.userAgent);
}

export function cancelReply() {
    setReplyTo(null);
    var bar = document.getElementById('replyBar');
    if (bar) bar.style.display = 'none';
}

export function doSendMessage() {
    closeEmojiPanel();
    closeSlashPopup();
    var input  = document.getElementById('chatInput');
    var text   = input.value.trim();
    if (!text || !activeChat || !myWalletId) return;

    // Intercept slash commands -- don't send as message
    if (text.startsWith('/')) {
        if (handleSlashCommand(text)) {
            input.value = '';
            input.style.height = 'auto';
            document.getElementById('charCount').textContent = '';
            document.getElementById('sendBtn').disabled = true;
            return;
        }
    }

    if (text.length > MAX_MSG_CHARS) { showToast('Message too long (max ' + MAX_MSG_CHARS + ' chars)', 'warning'); return; }

    var btn = document.getElementById('sendBtn');
    btn.disabled = true;

    var msgObj = { v: 1, t: 'dm', msg: text, ts: Math.floor(Date.now() / 1000) };
    if (myHandle && myHandle.handle) msgObj.from = myHandle.handle;
    if (myHandle && myHandle.display_name) msgObj.dn = myHandle.display_name;
    // Include recipient handle so multi-account wallets filter by intended recipient
    var recipientHandle = activeChat.startsWith('@') ? activeChat.slice(1)
                        : (contacts[activeChat] && contacts[activeChat].handle);
    if (recipientHandle) msgObj.to = recipientHandle;
    if (replyTo) msgObj.reply = replyTo.text;

    // Instant UI -- show bubble and clear input immediately before API call
    var convKey = activeChat;
    if (!conversations[convKey]) conversations[convKey] = [];
    var replySnapshot = replyTo ? replyTo.text : null;
    cancelReply(); // clears replyTo + hides reply bar
    conversations[convKey].push({ text: text, ts: msgObj.ts, sent: true, reply: replySnapshot });
    saveToStorage();
    input.value = '';
    input.style.height = 'auto';
    document.getElementById('charCount').textContent = '';
    renderChatMessages(convKey);

    // Use registered receive address. Must be a valid hex wallet_id (not '@handle' string).
    var receiver = contacts[activeChat] && contacts[activeChat].wallet_id;
    if (!receiver || !isValidWalletId(receiver)) {
        // wallet_id not yet resolved -- async resolveHandleIntoContact hasn't completed
        btn.disabled = false;
        conversations[convKey] = conversations[convKey].filter(function(m) {
            return !(m.sent && m.ts === msgObj.ts && m.text === text);
        });
        saveToStorage();
        renderChatMessages(convKey);
        showToast('Resolving address... try again in a moment', 'warning');
        // Trigger resolve if not already in progress
        var h = activeChat.startsWith('@') ? activeChat.slice(1) : null;
        if (h && (!contacts[activeChat] || !contacts[activeChat]._resolving)) {
            if (!contacts[activeChat]) contacts[activeChat] = { handle: h };
            contacts[activeChat]._resolving = true;
            resolveHandleIntoContact(h, activeChat);
        }
        return;
    }

    sendSbbsMessage(receiver, msgObj, function(result) {
        btn.disabled = false;
        if (result && result.error) {
            // Roll back optimistic message on failure
            conversations[convKey] = conversations[convKey].filter(function(m) {
                return !(m.sent && m.ts === msgObj.ts && m.text === text);
            });
            saveToStorage();
            renderChatMessages(convKey);
            showToast('Send failed: ' + extractError(result), 'error');
        } else {
            // SBBS is best-effort -- retry in background for reliability
            setTimeout(function() { sendSbbsMessage(receiver, msgObj, function() {}); }, 5000);
            setTimeout(function() { sendSbbsMessage(receiver, msgObj, function() {}); }, 10000);
        }
    });
}
