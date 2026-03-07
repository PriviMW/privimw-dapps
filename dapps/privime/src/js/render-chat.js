'use strict';

import { GROTH_PER_BEAM } from './config.js';
import { conversations, setChatMsgCache, downloadedFiles } from './state.js';
import { escHtml, escAttr, formatTime, formatDateSep,
         formatFileSize, isImageMime, truncateFilename } from './helpers.js';
import { autoDownloadImages } from './file-sharing.js';

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
        var extraCls = '';
        var extraAttrs = '';
        if (m.isTip) {
            var tipBeam = m.tipAmount ? (m.tipAmount / GROTH_PER_BEAM).toFixed(8).replace(/\.?0+$/, '') : '?';
            var tipVerb = m.sent ? 'Sent' : 'Received';
            bubbleContent = '<div class="tip-bubble-inner">' +
                '<span class="tip-icon">\u{1F48E}</span> ' +
                tipVerb + '&nbsp; <span class="tip-amount">' + tipBeam + ' BEAM</span>' +
                '</div>';
            extraCls = ' tip';
        } else if (m.file) {
            extraCls = ' file';
            extraAttrs = ' data-cid="' + escAttr(m.file.cid) + '"' +
                ' data-key="' + escAttr(m.file.key) + '"' +
                ' data-iv="' + escAttr(m.file.iv) + '"' +
                ' data-mime="' + escAttr(m.file.mime) + '"' +
                ' data-size="' + m.file.size + '"' +
                ' data-name="' + escAttr(m.file.name) + '"';
            var fileIcon = isImageMime(m.file.mime) ? '\u{1F5BC}' : '\u{1F4CE}';
            var sizeStr = formatFileSize(m.file.size);
            var nameStr = escHtml(truncateFilename(m.file.name, 40));
            var cached = downloadedFiles[m.file.cid];
            var previewHtml;
            if (cached && isImageMime(m.file.mime)) {
                previewHtml = '<img src="' + cached + '" class="file-inline-img" onclick="openLightbox(this.src)">';
            } else if (isImageMime(m.file.mime)) {
                previewHtml = '<div class="file-loading-spinner"></div>';
            } else {
                previewHtml = '';
            }
            var actionStyle = (cached && isImageMime(m.file.mime)) ? ' style="display:none"' : '';
            var actionLabel = isImageMime(m.file.mime) ? 'View' : 'Download';
            bubbleContent = '<div class="file-bubble-inner">' +
                '<div class="file-preview">' + previewHtml + '</div>' +
                '<div class="file-info-row">' +
                '<span class="file-icon">' + fileIcon + '</span>' +
                '<div class="file-details">' +
                '<div class="file-name">' + nameStr + '</div>' +
                '<div class="file-size">' + sizeStr + '</div>' +
                '</div>' +
                '<button class="file-action"' + actionStyle + ' onclick="onFileAction(event,' + i + ')">' + actionLabel + '</button>' +
                '</div>' +
                (m.text ? '<div class="file-caption">' + escHtml(m.text) + '</div>' : '') +
                '</div>';
        } else {
            bubbleContent = replyHtml + escHtml(m.text);
        }
        html.push('<div class="msg-bubble ' + cls + extraCls + '"' + extraAttrs + ' oncontextmenu="showBubbleMenu(event,' + i + ')">' +
            bubbleContent +
            '<div class="msg-time">' + time + tickHtml + '</div>' +
        '</div>');
    });
    el.innerHTML = html.join('');

    if (wasAtBottom) {
        el.scrollTop = el.scrollHeight;
    }

    // Auto-download small images after render
    autoDownloadImages();
}
