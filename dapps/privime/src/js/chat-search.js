'use strict';

import { conversations, activeChat, downloadedFiles } from './state.js';
import { escHtml, escAttr, formatTime, formatDateSep,
         formatFileSize, isImageMime, truncateFilename } from './helpers.js';
import { renderChatMessages } from './render-chat.js';

// ================================================================
// IN-CONVERSATION SEARCH
// ================================================================
var chatSearchOpen = false;
var chatSearchMatches = []; // array of { msgIdx, startIdx }
var chatSearchCurrent = -1;

export function toggleChatSearch() {
    if (chatSearchOpen) { closeChatSearch(); return; }
    chatSearchOpen = true;
    document.getElementById('chatSearchBar').classList.add('active');
    document.getElementById('chatSearchInput').value = '';
    document.getElementById('chatSearchCount').textContent = '';
    chatSearchMatches = [];
    chatSearchCurrent = -1;
    document.getElementById('chatSearchInput').focus();
}

export function closeChatSearch() {
    chatSearchOpen = false;
    document.getElementById('chatSearchBar').classList.remove('active');
    chatSearchMatches = [];
    chatSearchCurrent = -1;
    // Re-render without highlights
    if (activeChat) renderChatMessages(activeChat);
}

export function onChatSearchInput() {
    var q = document.getElementById('chatSearchInput').value.toLowerCase();
    var msgs = conversations[activeChat] || [];
    chatSearchMatches = [];
    if (q.length > 0) {
        msgs.forEach(function(m, i) {
            var searchText = m.file ? (m.file.name || '') : m.text;
            var idx = searchText.toLowerCase().indexOf(q);
            while (idx !== -1) {
                chatSearchMatches.push({ msgIdx: i, startIdx: idx });
                idx = searchText.toLowerCase().indexOf(q, idx + 1);
            }
        });
    }
    var countEl = document.getElementById('chatSearchCount');
    if (chatSearchMatches.length > 0) {
        chatSearchCurrent = chatSearchMatches.length - 1; // start at newest
        countEl.textContent = (chatSearchCurrent + 1) + '/' + chatSearchMatches.length;
        highlightSearchResults(q);
    } else {
        chatSearchCurrent = -1;
        countEl.textContent = q.length > 0 ? '0/0' : '';
        if (activeChat) renderChatMessages(activeChat);
    }
}

export function onChatSearchKeydown(e) {
    if (e.key === 'Enter') { e.shiftKey ? chatSearchPrev() : chatSearchNext(); e.preventDefault(); }
    if (e.key === 'Escape') closeChatSearch();
}

export function chatSearchNext() {
    if (chatSearchMatches.length === 0) return;
    chatSearchCurrent = (chatSearchCurrent + 1) % chatSearchMatches.length;
    document.getElementById('chatSearchCount').textContent =
        (chatSearchCurrent + 1) + '/' + chatSearchMatches.length;
    highlightSearchResults(document.getElementById('chatSearchInput').value.toLowerCase());
}

export function chatSearchPrev() {
    if (chatSearchMatches.length === 0) return;
    chatSearchCurrent = (chatSearchCurrent - 1 + chatSearchMatches.length) % chatSearchMatches.length;
    document.getElementById('chatSearchCount').textContent =
        (chatSearchCurrent + 1) + '/' + chatSearchMatches.length;
    highlightSearchResults(document.getElementById('chatSearchInput').value.toLowerCase());
}

export function highlightSearchResults(query) {
    // Re-render messages with highlights
    var msgs = conversations[activeChat] || [];
    var el = document.getElementById('chatMessages');
    var currentMatch = chatSearchMatches[chatSearchCurrent];
    var matchByMsg = {};
    chatSearchMatches.forEach(function(m, i) {
        if (!matchByMsg[m.msgIdx]) matchByMsg[m.msgIdx] = [];
        matchByMsg[m.msgIdx].push({ startIdx: m.startIdx, isCurrent: i === chatSearchCurrent });
    });

    var html = [];
    var lastDateStr = '';
    msgs.forEach(function(m, i) {
        var cls = m.sent ? 'sent' : 'received';
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
        if (m.file) {
            var fileIcon = isImageMime(m.file.mime) ? '\u{1F5BC}' : '\u{1F4CE}';
            var sizeStr = formatFileSize(m.file.size);
            var nameStr = escHtml(truncateFilename(m.file.name, 40));
            var cached = downloadedFiles[m.file.cid];
            var previewHtml = '';
            if (cached && isImageMime(m.file.mime)) {
                previewHtml = '<img src="' + cached + '" class="file-inline-img">';
            }
            bubbleContent = '<div class="file-bubble-inner">' +
                '<div class="file-preview">' + previewHtml + '</div>' +
                '<div class="file-info-row">' +
                '<span class="file-icon">' + fileIcon + '</span>' +
                '<div class="file-details">' +
                '<div class="file-name">' + nameStr + '</div>' +
                '<div class="file-size">' + sizeStr + '</div>' +
                '</div></div>' +
                (m.text ? '<div class="file-caption">' + escHtml(m.text) + '</div>' : '') +
                '</div>';
            cls += ' file';
        } else {
            var textHtml;
            if (matchByMsg[i]) {
                textHtml = highlightText(m.text, matchByMsg[i]);
            } else {
                textHtml = escHtml(m.text);
            }
            bubbleContent = replyHtml + textHtml;
        }
        html.push('<div class="msg-bubble ' + cls + '" id="msg-' + i + '" oncontextmenu="showBubbleMenu(event,' + i + ')">' +
            bubbleContent +
            '<div class="msg-time">' + time + tickHtml + '</div>' +
        '</div>');
    });
    el.innerHTML = html.join('');

    // Scroll to current match
    if (currentMatch) {
        var target = document.getElementById('msg-' + currentMatch.msgIdx);
        if (target) target.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

export function highlightText(text, matches) {
    // Sort matches by startIdx descending so we can insert from the end without offset issues
    var query = document.getElementById('chatSearchInput').value;
    var qLen = query.length;
    var parts = [];
    var lastEnd = 0;
    var sortedAsc = matches.slice().sort(function(a, b) { return a.startIdx - b.startIdx; });
    sortedAsc.forEach(function(m) {
        if (m.startIdx < lastEnd) return; // overlapping
        parts.push(escHtml(text.substring(lastEnd, m.startIdx)));
        var cls = m.isCurrent ? 'search-highlight current' : 'search-highlight';
        parts.push('<span class="' + cls + '">' + escHtml(text.substring(m.startIdx, m.startIdx + qLen)) + '</span>');
        lastEnd = m.startIdx + qLen;
    });
    parts.push(escHtml(text.substring(lastEnd)));
    return parts.join('');
}
