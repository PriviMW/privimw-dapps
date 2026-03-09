'use strict';

import { registrationFee } from './state.js';

// ================================================================
// HELPERS
// ================================================================
export function extractError(r) {
    if (typeof r === 'string') return r;
    if (r && r.error && typeof r.error === 'object') return r.error.message || JSON.stringify(r.error);
    if (r && typeof r.error === 'string') return r.error;
    return (r && r.message) || 'Unknown error';
}

export function shortWalletId(wid) {
    if (!wid || wid.length < 16) return wid || '';
    return wid.substring(0, 8) + '...' + wid.substring(wid.length - 8);
}

// Fix BVM sign-extended UTF-8: the BVM's DocAddText treats char as signed,
// so bytes > 127 (e.g. 0xE2 in emoji ✨) become negative → sign-extended to
// 0xFFFFFFE2 → JSON "\uFFFFFFE2" → JS parses as U+FFFF + "FFE2" literal text.
// Result: each mangled byte shows as ￿ffe2 (replacement char + "ff" + hex byte).
// This reconstructs the original UTF-8 bytes and decodes them.
export function fixBvmUtf8(str) {
    if (!str || typeof str !== 'string') return str || '';
    // Quick check: look for the mangled pattern (char >= U+FFFD followed by "ff")
    var hasMangled = false;
    for (var i = 0; i < str.length; i++) {
        var code = str.charCodeAt(i);
        if (code >= 0xFFFD && i + 4 < str.length &&
            str.substring(i + 1, i + 3).toLowerCase() === 'ff') {
            hasMangled = true; break;
        }
    }
    if (!hasMangled) return str;
    // Parse: normal ASCII chars → byte, mangled pattern (￿ff + 2 hex) → decoded byte
    var bytes = [];
    var i = 0;
    while (i < str.length) {
        var code = str.charCodeAt(i);
        if (code >= 0xFFFD && i + 4 < str.length) {
            var tail = str.substring(i + 1, i + 5).toLowerCase();
            var m = tail.match(/^ff([0-9a-f]{2})$/);
            if (m) {
                bytes.push(parseInt(m[1], 16));
                i += 5;
                continue;
            }
        }
        if (code < 128) {
            bytes.push(code);
        }
        i++;
    }
    try {
        return new TextDecoder('utf-8').decode(new Uint8Array(bytes));
    } catch (e) {
        return str;
    }
}

export function escHtml(str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

// Escape for use inside HTML attribute strings (both single and double-quoted contexts)
export function escAttr(str) {
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/\\/g, '\\\\');
}

export function formatTs(ts) {
    if (!ts) return '';
    var now  = Math.floor(Date.now() / 1000);
    var diff = now - ts;
    if (diff < 60)   return 'now';
    if (diff < 3600) return Math.floor(diff / 60) + 'm';
    if (diff < 86400) return Math.floor(diff / 3600) + 'h';
    return Math.floor(diff / 86400) + 'd';
}

export function formatTime(ts) {
    if (!ts) return '';
    var d = new Date(ts * 1000);
    return d.getHours().toString().padStart(2, '0') + ':' + d.getMinutes().toString().padStart(2, '0');
}

export function formatDateSep(ts) {
    if (!ts) return '';
    var now = new Date(), d = new Date(ts * 1000);
    var today  = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    var msgDay = new Date(d.getFullYear(), d.getMonth(), d.getDate());
    var diff   = Math.round((today - msgDay) / 86400000);
    if (diff === 0) return 'Today';
    if (diff === 1) return 'Yesterday';
    return d.toLocaleDateString(undefined, {
        month: 'short', day: 'numeric',
        year: d.getFullYear() !== now.getFullYear() ? 'numeric' : undefined
    });
}

export function copyToClipboard(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(function() {
            showToast('Copied to clipboard', 'success');
        }, function() {
            copyToClipboardFallback(text);
        });
    } else {
        copyToClipboardFallback(text);
    }
}

function copyToClipboardFallback(text) {
    try {
        var ta = document.createElement('textarea');
        ta.value = text;
        ta.style.cssText = 'position:fixed;top:0;left:0;opacity:0;';
        document.body.appendChild(ta);
        ta.select();
        document.execCommand('copy');
        document.body.removeChild(ta);
        showToast('Copied to clipboard', 'success');
    } catch (e) {
        showToast('Copy failed', 'error');
    }
}

export function formatFee() {
    var f = registrationFee;
    return (f % 1 === 0 ? f.toFixed(0) : f.toFixed(2)) + ' BEAM';
}

export function updateFeeDisplay() {
    var fee = formatFee();
    var el;
    el = document.getElementById('heroFee');
    if (el) el.textContent = 'Registration fee: ' + fee + ' · Permanent';
    el = document.getElementById('regFeeInfo');
    if (el) el.innerHTML = 'Registration fee: <strong style="color:var(--text-primary);">' + fee + '</strong> · Non-refundable · Permanent until released';
    el = document.getElementById('regBtnFee');
    if (el) el.textContent = fee;
}

export function showToast(msg, type) {
    var container = document.getElementById('toastContainer');
    var toast = document.createElement('div');
    toast.className = 'toast ' + (type || 'info');
    toast.textContent = msg;
    container.appendChild(toast);
    setTimeout(function() {
        toast.style.opacity = '0';
        toast.style.transition = 'opacity 0.3s ease';
        setTimeout(function() { if (toast.parentNode) toast.parentNode.removeChild(toast); }, 300);
    }, 3000);
}

export function setSplashStatus(msg) {
    document.getElementById('splashStatus').textContent = msg;
}

// File sharing utilities
export function formatFileSize(bytes) {
    if (!bytes || bytes <= 0) return '0 B';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
}

export function isImageMime(mime) {
    return mime && ['image/jpeg', 'image/png', 'image/gif', 'image/webp'].indexOf(mime) !== -1;
}

export function truncateFilename(name, max) {
    if (!name || name.length <= max) return name || '';
    var ext = name.lastIndexOf('.') !== -1 ? name.slice(name.lastIndexOf('.')) : '';
    return name.substring(0, max - ext.length - 3) + '...' + ext;
}

// Save/view a file — opens in-app viewer with save options
// Stores blob reference for save actions
var _viewerBlob = null;
var _viewerFilename = '';

export async function saveFileWithPicker(blob, filename) {
    // Try File System Access API first (lets user choose location)
    if (window.showSaveFilePicker) {
        try {
            var ext = filename.lastIndexOf('.') !== -1 ? filename.slice(filename.lastIndexOf('.')) : '';
            var opts = { suggestedName: filename };
            if (ext) {
                opts.types = [{ description: 'File', accept: {} }];
                opts.types[0].accept[blob.type || 'application/octet-stream'] = [ext];
            }
            var handle = await window.showSaveFilePicker(opts);
            var writable = await handle.createWritable();
            await writable.write(blob);
            await writable.close();
            showToast('File saved: ' + filename, 'success');
            return;
        } catch (err) {
            if (err.name === 'AbortError') return;
        }
    }
    // Open in-app viewer with blob URL (works for PDFs + images)
    openFileViewer(blob, filename);
}

function openFileViewer(blob, filename) {
    var existing = document.getElementById('fileViewerOverlay');
    if (existing) existing.remove();

    _viewerBlob = blob;
    _viewerFilename = filename;
    var mime = blob.type || '';
    var blobUrl = URL.createObjectURL(blob);

    var overlay = document.createElement('div');
    overlay.id = 'fileViewerOverlay';
    overlay.className = 'file-viewer-overlay';

    var header = '<div class="file-viewer-header">' +
        '<button class="file-viewer-close" onclick="closeFileViewer()">&times;</button>' +
        '<span class="file-viewer-name">' + escHtml(filename) + '</span>' +
        '<button class="file-viewer-save-btn" onclick="viewerCopyOrSave()">Copy</button>' +
        '</div>';

    var content = '';
    if (mime === 'application/pdf') {
        content = '<iframe class="file-viewer-frame" src="' + blobUrl + '#toolbar=1"></iframe>';
    } else if (mime && mime.indexOf('image/') === 0) {
        content = '<div class="file-viewer-img-wrap"><img class="file-viewer-img" src="' + blobUrl + '"></div>';
    } else if (mime === 'text/plain') {
        var reader = new FileReader();
        reader.onload = function() {
            var pre = document.getElementById('fileViewerTextContent');
            if (pre) pre.textContent = reader.result;
        };
        reader.readAsText(blob);
        content = '<pre class="file-viewer-text" id="fileViewerTextContent">Loading...</pre>';
    } else {
        content = '<div class="file-viewer-text" style="text-align:center;padding:40px;">' +
            '<div style="font-size:48px;margin-bottom:16px;">\u{1F4CE}</div>' +
            '<div>' + escHtml(filename) + '</div>' +
            '</div>';
    }

    overlay.innerHTML = header + '<div class="file-viewer-body">' + content + '</div>';
    document.body.appendChild(overlay);
}

export function closeFileViewer() {
    var el = document.getElementById('fileViewerOverlay');
    if (el) el.remove();
    _viewerBlob = null;
    _viewerFilename = '';
}

export async function viewerCopyOrSave() {
    if (!_viewerBlob) return;
    var mime = _viewerBlob.type || '';

    // Images: copy to clipboard
    if (mime.indexOf('image/') === 0) {
        try {
            // Convert to PNG for clipboard (clipboard API requires PNG)
            var img = new Image();
            var blobUrl = URL.createObjectURL(_viewerBlob);
            img.src = blobUrl;
            await new Promise(function(r) { img.onload = r; });
            var canvas = document.createElement('canvas');
            canvas.width = img.naturalWidth;
            canvas.height = img.naturalHeight;
            canvas.getContext('2d').drawImage(img, 0, 0);
            URL.revokeObjectURL(blobUrl);
            var pngBlob = await new Promise(function(r) { canvas.toBlob(r, 'image/png'); });
            await navigator.clipboard.write([
                new ClipboardItem({ 'image/png': pngBlob })
            ]);
            showToast('Image copied to clipboard', 'success');
            return;
        } catch (err) {
            // Clipboard write failed, fall through
        }
    }

    // Text: copy to clipboard
    if (mime === 'text/plain') {
        try {
            var text = await _viewerBlob.text();
            await navigator.clipboard.writeText(text);
            showToast('Text copied to clipboard', 'success');
            return;
        } catch (err) { /* fall through */ }
    }

    // Last resort: try <a download> — may or may not work
    var url = URL.createObjectURL(_viewerBlob);
    var a = document.createElement('a');
    a.href = url;
    a.download = _viewerFilename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function() { URL.revokeObjectURL(url); }, 5000);
    showToast('Download attempted — check your Downloads folder', 'info');
}
