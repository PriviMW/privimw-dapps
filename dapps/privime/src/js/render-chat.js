'use strict';

import { GROTH_PER_BEAM } from './config.js';
import { conversations, setChatMsgCache } from './state.js';
import { escHtml, formatTime, formatDateSep } from './helpers.js';

// ================================================================
// CHAT RENDERING
// ================================================================
export function renderChatMessages(walletId, forceScroll) {
    var msgs = conversations[walletId] || [];
    var el   = document.getElementById('chatMessages');

    // Only auto-scroll if user is already near the bottom (within 120px), or forced
    var wasAtBottom = forceScroll || el.scrollHeight - el.scrollTop - el.clientHeight < 120;

    setChatMsgCache(msgs);

    if (msgs.length === 0) {
        el.innerHTML = '<div class="chat-empty"><div style="font-size:40px;margin-bottom:12px;">\u{1F4AC}</div><div>No messages yet.</div><div style="margin-top:6px;font-size:12px;">Messages expire after 12 hours</div></div>';
        return;
    }

    var html = [];
    var lastDateStr = '';
    msgs.forEach(function(m, i) {
        var cls  = m.sent ? 'sent' : 'received';
        var time = formatTime(m.ts);
        var dateStr = formatDateSep(m.ts);
        if (dateStr !== lastDateStr) {
            html.push('<div class="date-sep">' + escHtml(dateStr) + '</div>');
            lastDateStr = dateStr;
        }
        var replyHtml = m.reply
            ? '<div class="reply-quote">' + escHtml(m.reply.substring(0, 80)) + (m.reply.length > 80 ? '\u2026' : '') + '</div>'
            : '';
        var tickHtml = '';
        if (m.sent) {
            tickHtml = m.read
                ? ' <span class="msg-tick read">\u2713\u2713</span>'
                : ' <span class="msg-tick">\u2713</span>';
        }
        var bubbleContent;
        if (m.isTip) {
            var tipBeam = m.tipAmount ? (m.tipAmount / GROTH_PER_BEAM).toFixed(8).replace(/\.?0+$/, '') : '?';
            var tipVerb = m.sent ? 'Sent' : 'Received';
            bubbleContent = '<div class="tip-bubble-inner">' +
                '<span class="tip-icon">\u{1F48E}</span> ' +
                tipVerb + '&nbsp; <span class="tip-amount">' + tipBeam + ' BEAM</span>' +
                '</div>';
        } else {
            bubbleContent = replyHtml + escHtml(m.text);
        }
        html.push('<div class="msg-bubble ' + cls + (m.isTip ? ' tip' : '') + '" oncontextmenu="showBubbleMenu(event,' + i + ')">' +
            bubbleContent +
            '<div class="msg-time">' + time + tickHtml + '</div>' +
        '</div>');
    });
    el.innerHTML = html.join('');

    if (wasAtBottom) {
        el.scrollTop = el.scrollHeight;
    }
}
