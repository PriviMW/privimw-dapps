'use strict';

import { MAX_FILE_SIZE, INLINE_FILE_MAX_SIZE, IPFS_ADD_TIMEOUT, IPFS_GET_TIMEOUT,
         ALLOWED_MIME_TYPES, AUTO_DL_MAX_SIZE,
         COMPRESS_MAX_DIM, COMPRESS_QUALITY, COMPRESS_MIN_SIZE } from './config.js';
import { activeChat, myHandle, contacts, conversations,
         fileUploadInProgress, setFileUploadInProgress,
         pendingFile, setPendingFile,
         downloadedFiles, setDownloadedFile,
         bubbleCtxMsg, bubbleCtxWid } from './state.js';

// Base64 <-> Uint8Array helpers for inline file delivery
function uint8ToBase64(bytes) {
    var binary = '';
    for (var i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
    return btoa(binary);
}

function base64ToUint8(b64) {
    var binary = atob(b64);
    var bytes = new Uint8Array(binary.length);
    for (var i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    return bytes;
}

// Inline file data lookup — stores base64 ciphertext for inline files (keyed by CID)
// Used by downloadAndDecryptFile to skip IPFS when data is available in the message
var inlineFileData = {};

export function registerInlineFileData(cid, data) {
    if (cid && data) inlineFileData[cid] = data;
}

// LRU eviction for blob URLs — prevents unbounded memory growth
var blobUrlOrder = []; // CIDs in insertion order (oldest first)
var MAX_CACHED_BLOBS = 30;

// ================================================================
// INDEXEDDB CACHE — persist decrypted files across sessions
// ================================================================
var IDB_NAME = 'privime_files';
var IDB_STORE = 'files';
var IDB_VERSION = 1;
var IDB_MAX_ENTRIES = 100;
var _idb = null;

function openIDB() {
    if (_idb) return Promise.resolve(_idb);
    return new Promise(function(resolve, reject) {
        var req = indexedDB.open(IDB_NAME, IDB_VERSION);
        req.onupgradeneeded = function(e) {
            var db = e.target.result;
            if (!db.objectStoreNames.contains(IDB_STORE)) {
                var store = db.createObjectStore(IDB_STORE, { keyPath: 'cid' });
                store.createIndex('ts', 'ts');
            }
        };
        req.onsuccess = function() { _idb = req.result; resolve(_idb); };
        req.onerror = function() { resolve(null); };
    });
}

async function idbGet(cid) {
    try {
        var db = await openIDB();
        if (!db) return null;
        return new Promise(function(resolve) {
            var tx = db.transaction(IDB_STORE, 'readonly');
            var req = tx.objectStore(IDB_STORE).get(cid);
            req.onsuccess = function() { resolve(req.result || null); };
            req.onerror = function() { resolve(null); };
        });
    } catch (e) { return null; }
}

async function idbPut(cid, blob, mime) {
    try {
        var db = await openIDB();
        if (!db) return;
        var buf = await blob.arrayBuffer();
        var tx = db.transaction(IDB_STORE, 'readwrite');
        var store = tx.objectStore(IDB_STORE);
        store.put({ cid: cid, data: buf, mime: mime, ts: Date.now() });
        // Evict oldest entries if over limit
        var countReq = store.count();
        countReq.onsuccess = function() {
            if (countReq.result > IDB_MAX_ENTRIES) {
                var idx = store.index('ts');
                var delCount = countReq.result - IDB_MAX_ENTRIES;
                var cursor = idx.openCursor();
                cursor.onsuccess = function(e) {
                    var c = e.target.result;
                    if (c && delCount > 0) {
                        c.delete();
                        delCount--;
                        c.continue();
                    }
                };
            }
        };
    } catch (e) { /* IDB write failed, non-fatal */ }
}
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
// DRAG AND DROP
// ================================================================

var dropCounter = 0; // track nested dragenter/dragleave

export function onChatDragOver(e) {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'copy';
}

export function onChatDragEnter(e) {
    e.preventDefault();
    dropCounter++;
    var overlay = document.getElementById('dropOverlay');
    if (overlay) overlay.classList.add('active');
}

export function onChatDragLeave(e) {
    e.preventDefault();
    dropCounter--;
    if (dropCounter <= 0) {
        dropCounter = 0;
        var overlay = document.getElementById('dropOverlay');
        if (overlay) overlay.classList.remove('active');
    }
}

export function onChatDrop(e) {
    e.preventDefault();
    dropCounter = 0;
    var overlay = document.getElementById('dropOverlay');
    if (overlay) overlay.classList.remove('active');

    var files = e.dataTransfer && e.dataTransfer.files;
    if (files && files.length > 0) {
        handleFileSelection(files[0]);
    }
}

// ================================================================
// IMAGE COMPRESSION
// ================================================================

function shouldCompress(file) {
    return isImageMime(file.type) && file.type !== 'image/gif' &&
           file.size > COMPRESS_MIN_SIZE;
}

async function compressImage(file) {
    var blobUrl = URL.createObjectURL(file);
    try {
        var img = new Image();
        img.src = blobUrl;
        await new Promise(function(resolve, reject) {
            img.onload = resolve;
            img.onerror = function() { reject(new Error('Failed to load image')); };
        });

        var w = img.naturalWidth, h = img.naturalHeight;
        // Only resize if larger than max dimension
        if (w > COMPRESS_MAX_DIM || h > COMPRESS_MAX_DIM) {
            var ratio = Math.min(COMPRESS_MAX_DIM / w, COMPRESS_MAX_DIM / h);
            w = Math.round(w * ratio);
            h = Math.round(h * ratio);
        }

        var canvas = document.createElement('canvas');
        canvas.width = w;
        canvas.height = h;
        canvas.getContext('2d').drawImage(img, 0, 0, w, h);

        var blob = await new Promise(function(resolve) {
            canvas.toBlob(resolve, 'image/jpeg', COMPRESS_QUALITY);
        });

        // Only use compressed version if it's actually smaller
        if (blob.size < file.size) {
            return blob;
        }
        return null; // original is smaller, skip compression
    } finally {
        URL.revokeObjectURL(blobUrl);
    }
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

    try {
        // 1. Compress images (resize + JPEG encode)
        var sendMime = file.type;
        var sendSize = file.size;
        var sendName = file.name;
        if (shouldCompress(file)) {
            showToast('Compressing...', 'info');
            var compressed = await compressImage(file);
            if (compressed) {
                file = compressed;
                sendMime = 'image/jpeg';
                sendSize = compressed.size;
                // Update extension if original wasn't JPEG
                if (sendName && !/\.jpe?g$/i.test(sendName)) {
                    sendName = sendName.replace(/\.[^.]+$/, '.jpg');
                }
            }
        }

        // 2. Read file
        showToast('Encrypting...', 'info');
        var plaintext = await file.arrayBuffer();

        // 3. Generate key + IV
        var ck = await generateFileKey();

        // 4. Encrypt
        var ciphertext = await encryptFile(plaintext, ck.key, ck.iv);

        // 5. Decide delivery: inline (< 200KB) vs IPFS
        var cipherUint8 = new Uint8Array(ciphertext);
        var fileCid;
        var inlineData;

        if (cipherUint8.length <= INLINE_FILE_MAX_SIZE) {
            // INLINE: embed encrypted data directly in SBBS message — no IPFS, guaranteed delivery
            showToast('Sending inline...', 'info');
            inlineData = uint8ToBase64(cipherUint8);
            fileCid = 'inline-' + Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
        } else {
            // IPFS: upload encrypted data (requires sender to be online for receiver to download)
            showToast('Uploading to IPFS...', 'info');
            var uint8Arr = Array.from(cipherUint8);
            fileCid = await new Promise(function(resolve, reject) {
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
        }

        // 6. Construct SBBS message
        var ts = Math.floor(Date.now() / 1000);
        var fileMeta = {
            cid: fileCid,
            key: ck.keyHex,
            iv: ck.ivHex,
            name: truncateFilename(sendName, 60),
            size: sendSize,
            mime: sendMime
        };
        if (inlineData) {
            fileMeta.data = inlineData;
            // Register so sender's own auto-download can find the data
            registerInlineFileData(fileCid, inlineData);
        }

        var msgObj = {
            v: 1, t: 'file', ts: ts,
            from: myHandle.handle,
            dn: myHandle.display_name || '',
            to: activeChat.startsWith('@') ? activeChat.slice(1)
                : (contacts[activeChat] && contacts[activeChat].handle)
        };
        if (caption) msgObj.msg = caption;
        msgObj.file = fileMeta;

        // 8. Instant UI
        if (!conversations[convKey]) conversations[convKey] = [];
        conversations[convKey].push({
            text: caption || '',
            ts: ts,
            sent: true,
            file: fileMeta
        });
        saveToStorage();
        renderChatMessages(convKey);

        // 9. Send via SBBS
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
    // Check in-memory session cache first
    if (downloadedFiles[fileInfo.cid]) {
        if (isImageMime(fileInfo.mime)) {
            showInlineImage(downloadedFiles[fileInfo.cid], bubbleEl);
        } else {
            triggerDownloadFromUrl(downloadedFiles[fileInfo.cid], fileInfo.name, fileInfo.mime);
        }
        return;
    }

    var actionEl = bubbleEl ? bubbleEl.querySelector('.file-action') : null;

    // Check IndexedDB cache (persists across sessions)
    var cached = await idbGet(fileInfo.cid);
    if (cached && cached.data) {
        var cachedBlob = new Blob([cached.data], { type: cached.mime || fileInfo.mime });
        var cachedUrl = URL.createObjectURL(cachedBlob);
        cacheBlobUrl(fileInfo.cid, cachedUrl);
        if (isImageMime(fileInfo.mime)) {
            showInlineImage(cachedUrl, bubbleEl);
        } else {
            triggerDownloadFromUrl(cachedUrl, fileInfo.name, fileInfo.mime);
        }
        return;
    }

    try {
        var plaintext;
        // Check inline data: from fileInfo directly, or from the lookup map
        var inData = fileInfo.data || inlineFileData[fileInfo.cid];

        if (inData) {
            // INLINE PATH: data embedded in message — decrypt directly, no IPFS needed
            if (actionEl) actionEl.textContent = 'Decrypting...';
            var inlineCipher = base64ToUint8(inData);
            if (inlineCipher.length > MAX_FILE_SIZE) throw new Error('Inline file exceeds size limit');
            plaintext = await decryptFile(inlineCipher.buffer, fileInfo.key, fileInfo.iv);
        } else {
            // IPFS PATH: download from network (requires sender to be online)
            if (actionEl) actionEl.textContent = 'Downloading...';
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
            if (result.length > MAX_FILE_SIZE) throw new Error('Downloaded file exceeds size limit');
            var cipherBuf = new Uint8Array(result).buffer;
            plaintext = await decryptFile(cipherBuf, fileInfo.key, fileInfo.iv);
        }

        // Create blob + cache in memory and IndexedDB
        var blob = new Blob([plaintext], { type: fileInfo.mime });
        var blobUrl = URL.createObjectURL(blob);
        cacheBlobUrl(fileInfo.cid, blobUrl);

        // Persist to IndexedDB for cross-session cache
        idbPut(fileInfo.cid, blob, fileInfo.mime);

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

function cacheBlobUrl(cid, blobUrl) {
    setDownloadedFile(cid, blobUrl);
    blobUrlOrder.push(cid);
    // Evict oldest blob URLs when cache exceeds limit
    while (blobUrlOrder.length > MAX_CACHED_BLOBS) {
        var oldCid = blobUrlOrder.shift();
        if (downloadedFiles[oldCid]) {
            URL.revokeObjectURL(downloadedFiles[oldCid]);
            delete downloadedFiles[oldCid];
        }
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
var autoDownloadFailed = {};  // { cid: true } — stop retrying failed downloads
var autoDownloadActive = 0;
var MAX_CONCURRENT_AUTO_DL = 3;

export function autoDownloadImages() {
    var bubbles = document.querySelectorAll('.msg-bubble.file');
    bubbles.forEach(function(bubble) {
        // Concurrency cap — stop queuing when limit reached
        if (autoDownloadActive >= MAX_CONCURRENT_AUTO_DL) return;

        var cidAttr = bubble.getAttribute('data-cid');
        if (!cidAttr) return;
        // Already downloaded, in progress, or previously failed
        if (downloadedFiles[cidAttr] || autoDownloadPending[cidAttr] || autoDownloadFailed[cidAttr]) return;

        var mimeAttr = bubble.getAttribute('data-mime');
        var sizeAttr = parseInt(bubble.getAttribute('data-size'), 10);

        // Auto-process: inline files (any type, no network cost) OR small IPFS images
        var hasInlineData = !!(inlineFileData[cidAttr]);
        if (!hasInlineData && (!isImageMime(mimeAttr) || sizeAttr > AUTO_DL_MAX_SIZE)) return;

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
        }, bubble).catch(function() {
            // Mark as failed to prevent retry spam on every render cycle
            autoDownloadFailed[cidAttr] = true;
        }).finally(function() {
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
