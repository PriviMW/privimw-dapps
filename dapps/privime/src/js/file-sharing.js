'use strict';

import { MAX_FILE_SIZE, IPFS_ADD_TIMEOUT, IPFS_GET_TIMEOUT,
         ALLOWED_MIME_TYPES, AUTO_DL_MAX_SIZE } from './config.js';
import { activeChat, myHandle, contacts, conversations,
         fileUploadInProgress, setFileUploadInProgress,
         pendingFile, setPendingFile,
         downloadedFiles, setDownloadedFile,
         bubbleCtxMsg, bubbleCtxWid } from './state.js';

// LRU eviction for blob URLs — prevents unbounded memory growth
var blobUrlOrder = []; // CIDs in insertion order (oldest first)
var MAX_CACHED_BLOBS = 30;
import { showToast, extractError, escHtml, formatFileSize,
         isImageMime, truncateFilename, saveFileWithPicker } from './helpers.js';
import { saveToStorage } from './storage.js';
import { callApi } from './wallet-api.js';
import { sendSbbsMessage } from './sbbs.js';
import { renderChatMessages } from './render-chat.js';
import { isValidWalletId } from './registration.js';
import { generateFileKey, encryptFile, decryptFile } from './crypto.js';

// ================================================================
// FILE PICKER
// ================================================================

export function openFilePicker() {
    if (!activeChat || !myHandle) return;
    var input = document.getElementById('filePickerInput');
    if (!input) return;
    input.value = '';
    input.click();
}

export function handleFileSelection(file) {
    if (!file) return;
    if (file.size > MAX_FILE_SIZE) {
        showToast('File too large (max ' + formatFileSize(MAX_FILE_SIZE) + ')', 'warning');
        return;
    }
    if (ALLOWED_MIME_TYPES.indexOf(file.type) === -1) {
        showToast('Unsupported file type: ' + (file.type || 'unknown'), 'warning');
        return;
    }
    // Show preview bar and wait for user to send
    setPendingFile(file);
    showFilePreviewBar(file);
}

function showFilePreviewBar(file) {
    var bar = document.getElementById('filePreviewBar');
    if (!bar) return;
    var icon = document.getElementById('filePreviewIcon');
    var text = document.getElementById('filePreviewText');
    if (icon) icon.textContent = isImageMime(file.type) ? '\u{1F5BC}' : '\u{1F4CE}';
    if (text) text.textContent = truncateFilename(file.name, 30) + ' (' + formatFileSize(file.size) + ')';
    bar.style.display = 'flex';
    // Change placeholder to hint about captions
    var input = document.getElementById('chatInput');
    if (input) {
        input.placeholder = 'Add a caption...';
        input.focus();
    }
}

export function cancelFileAttachment() {
    setPendingFile(null);
    var bar = document.getElementById('filePreviewBar');
    if (bar) bar.style.display = 'none';
    var input = document.getElementById('chatInput');
    if (input) input.placeholder = 'Message...';
}

// ================================================================
// SENDER FLOW
// ================================================================

export async function sendFile(file, caption) {
    if (fileUploadInProgress) {
        showToast('Upload already in progress', 'warning');
        return;
    }

    var convKey = activeChat;
    var receiver = contacts[activeChat] && contacts[activeChat].wallet_id;
    if (!receiver || !isValidWalletId(receiver)) {
        showToast('Recipient address not resolved', 'warning');
        return;
    }

    setFileUploadInProgress(true);
    showToast('Encrypting...', 'info');

    try {
        // 1. Read file
        var plaintext = await file.arrayBuffer();

        // 2. Generate key + IV
        var ck = await generateFileKey();

        // 3. Encrypt
        var ciphertext = await encryptFile(plaintext, ck.key, ck.iv);

        // 4. Convert to uint8 array for IPFS API
        showToast('Uploading to IPFS...', 'info');
        var uint8Arr = Array.from(new Uint8Array(ciphertext));

        // 5. Upload to IPFS
        var ipfsCid = await new Promise(function(resolve, reject) {
            callApi('ipfs_add', {
                data: uint8Arr,
                pin: true,
                timeout: IPFS_ADD_TIMEOUT
            }, function(result) {
                if (result && result.error) {
                    reject(new Error(extractError(result)));
                } else if (result && result.hash) {
                    resolve(result.hash);
                } else {
                    reject(new Error('No IPFS hash returned'));
                }
            });
        });

        // 6. Construct SBBS message
        var ts = Math.floor(Date.now() / 1000);
        var fileMeta = {
            cid: ipfsCid,
            key: ck.keyHex,
            iv: ck.ivHex,
            name: truncateFilename(file.name, 60),
            size: file.size,
            mime: file.type
        };

        var msgObj = {
            v: 1, t: 'file', ts: ts,
            from: myHandle.handle,
            dn: myHandle.display_name || '',
            to: activeChat.startsWith('@') ? activeChat.slice(1)
                : (contacts[activeChat] && contacts[activeChat].handle)
        };
        if (caption) msgObj.msg = caption;
        msgObj.file = fileMeta;

        // 7. Instant UI
        if (!conversations[convKey]) conversations[convKey] = [];
        conversations[convKey].push({
            text: caption || '',
            ts: ts,
            sent: true,
            file: fileMeta
        });
        saveToStorage();
        renderChatMessages(convKey);

        // 8. Send via SBBS
        sendSbbsMessage(receiver, msgObj, function(result) {
            if (result && result.error) {
                // Roll back
                conversations[convKey] = conversations[convKey].filter(function(m) {
                    return !(m.sent && m.ts === ts && m.file && m.file.cid === ipfsCid);
                });
                saveToStorage();
                renderChatMessages(convKey);
                showToast('Send failed: ' + extractError(result), 'error');
            } else {
                showToast('File sent', 'success');
                // SBBS retries
                setTimeout(function() { sendSbbsMessage(receiver, msgObj, function() {}); }, 5000);
                setTimeout(function() { sendSbbsMessage(receiver, msgObj, function() {}); }, 10000);
            }
        });
    } catch (err) {
        showToast('File send failed: ' + (err.message || err), 'error');
    } finally {
        setFileUploadInProgress(false);
    }
}

// ================================================================
// RECEIVER FLOW
// ================================================================

export function onFileAction(event, idx) {
    event.stopPropagation();
    var msgs = conversations[activeChat] || [];
    var msg = msgs[idx];
    if (!msg || !msg.file) return;
    var bubbleEl = event.target.closest('.msg-bubble');
    downloadAndDecryptFile(msg.file, bubbleEl);
}

export async function downloadAndDecryptFile(fileInfo, bubbleEl) {
    // Check session cache first
    if (downloadedFiles[fileInfo.cid]) {
        if (isImageMime(fileInfo.mime)) {
            showInlineImage(downloadedFiles[fileInfo.cid], bubbleEl);
        } else {
            triggerDownloadFromUrl(downloadedFiles[fileInfo.cid], fileInfo.name, fileInfo.mime);
        }
        return;
    }

    var actionEl = bubbleEl ? bubbleEl.querySelector('.file-action') : null;
    if (actionEl) actionEl.textContent = 'Downloading...';

    try {
        var result = await new Promise(function(resolve, reject) {
            callApi('ipfs_get', {
                hash: fileInfo.cid,
                timeout: IPFS_GET_TIMEOUT
            }, function(res) {
                if (res && res.error) {
                    reject(new Error(extractError(res)));
                } else if (res && res.data) {
                    resolve(res.data);
                } else {
                    reject(new Error('No data returned from IPFS'));
                }
            });
        });

        if (actionEl) actionEl.textContent = 'Decrypting...';

        // Validate actual download size (prevents spoofed metadata size
        // from bypassing AUTO_DL_MAX_SIZE for auto-downloads)
        if (result.length > MAX_FILE_SIZE) {
            throw new Error('Downloaded file exceeds size limit');
        }

        // Convert uint8 array to ArrayBuffer
        var cipherBuf = new Uint8Array(result).buffer;

        // Decrypt
        var plaintext = await decryptFile(cipherBuf, fileInfo.key, fileInfo.iv);

        // Create blob URL and cache with LRU eviction
        var blob = new Blob([plaintext], { type: fileInfo.mime });
        var blobUrl = URL.createObjectURL(blob);
        setDownloadedFile(fileInfo.cid, blobUrl);
        blobUrlOrder.push(fileInfo.cid);
        // Evict oldest blob URLs when cache exceeds limit
        while (blobUrlOrder.length > MAX_CACHED_BLOBS) {
            var oldCid = blobUrlOrder.shift();
            if (downloadedFiles[oldCid]) {
                URL.revokeObjectURL(downloadedFiles[oldCid]);
                delete downloadedFiles[oldCid];
            }
        }

        if (isImageMime(fileInfo.mime)) {
            showInlineImage(blobUrl, bubbleEl);
        } else {
            triggerDownloadFromUrl(blobUrl, fileInfo.name, fileInfo.mime);
            if (actionEl) actionEl.textContent = 'Download';
        }
    } catch (err) {
        if (actionEl) actionEl.textContent = 'Retry';
        showToast('Download failed: ' + (err.message || err), 'error');
    }
}

function showInlineImage(blobUrl, bubbleEl) {
    if (!bubbleEl) return;
    var preview = bubbleEl.querySelector('.file-preview');
    if (preview) {
        preview.innerHTML = '<img src="' + blobUrl + '" class="file-inline-img" onclick="openLightbox(this.src)">';
    }
    var actionEl = bubbleEl.querySelector('.file-action');
    if (actionEl) actionEl.style.display = 'none';
}

async function triggerDownloadFromUrl(blobUrl, filename, mime) {
    var resp = await fetch(blobUrl);
    var blob = await resp.blob();
    saveFileWithPicker(blob, filename);
}

// ================================================================
// AUTO-DOWNLOAD IMAGES
// ================================================================

var autoDownloadPending = {}; // { cid: true } — prevent concurrent downloads
var autoDownloadActive = 0;
var MAX_CONCURRENT_AUTO_DL = 3;

export function autoDownloadImages() {
    var bubbles = document.querySelectorAll('.msg-bubble.file');
    bubbles.forEach(function(bubble) {
        // Concurrency cap — stop queuing when limit reached
        if (autoDownloadActive >= MAX_CONCURRENT_AUTO_DL) return;

        var cidAttr = bubble.getAttribute('data-cid');
        if (!cidAttr) return;
        // Already downloaded or in progress
        if (downloadedFiles[cidAttr] || autoDownloadPending[cidAttr]) return;

        var mimeAttr = bubble.getAttribute('data-mime');
        var sizeAttr = parseInt(bubble.getAttribute('data-size'), 10);
        if (!isImageMime(mimeAttr) || sizeAttr > AUTO_DL_MAX_SIZE) return;

        // Already has image loaded
        if (bubble.querySelector('.file-inline-img')) return;

        autoDownloadPending[cidAttr] = true;
        autoDownloadActive++;

        // Find the full file info from conversations
        var keyAttr = bubble.getAttribute('data-key');
        var ivAttr = bubble.getAttribute('data-iv');
        var nameAttr = bubble.getAttribute('data-name');

        downloadAndDecryptFile({
            cid: cidAttr, key: keyAttr, iv: ivAttr,
            name: nameAttr, size: sizeAttr, mime: mimeAttr
        }, bubble).finally(function() {
            delete autoDownloadPending[cidAttr];
            autoDownloadActive--;
            // Trigger next batch after one completes
            autoDownloadImages();
        });
    });
}

// ================================================================
// LIGHTBOX
// ================================================================

export function openLightbox(src) {
    var img = document.getElementById('lightboxImg');
    var modal = document.getElementById('lightboxModal');
    if (img) img.src = src;
    if (modal) modal.classList.add('active');
}

export async function copyLightboxImage() {
    var img = document.getElementById('lightboxImg');
    if (!img || !img.src) return;
    try {
        var canvas = document.createElement('canvas');
        canvas.width = img.naturalWidth;
        canvas.height = img.naturalHeight;
        canvas.getContext('2d').drawImage(img, 0, 0);
        var pngBlob = await new Promise(function(r) { canvas.toBlob(r, 'image/png'); });
        await navigator.clipboard.write([
            new ClipboardItem({ 'image/png': pngBlob })
        ]);
        showToast('Image copied to clipboard', 'success');
    } catch (err) {
        showToast('Could not copy image', 'error');
    }
}

export function closeLightbox() {
    var modal = document.getElementById('lightboxModal');
    if (modal) modal.classList.remove('active');
    var img = document.getElementById('lightboxImg');
    if (img) img.src = '';
}

// ================================================================
// CONTEXT MENU: SAVE FILE
// ================================================================

export function doSaveFileBubble() {
    if (!bubbleCtxMsg || !bubbleCtxMsg.file) return;
    var bubbles = document.querySelectorAll('.msg-bubble');
    var idx = Array.from(bubbles).findIndex(function(b) {
        return b.getAttribute('data-cid') === bubbleCtxMsg.file.cid;
    });
    var bubbleEl = idx >= 0 ? bubbles[idx] : null;
    downloadAndDecryptFile(bubbleCtxMsg.file, bubbleEl);
}
