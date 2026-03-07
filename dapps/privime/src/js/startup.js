'use strict';

import { GROTH_PER_BEAM } from './config.js';
import { MSG_REFRESH_MS } from './config.js';
import { isConnected, setConnected, shaderBytes, myHandle, setMyHandle,
         myWalletId, setMyWalletId, conversations, setConversations,
         contacts, setContacts, deletedConvs, unreadCounts,
         allMessages, setAllMessages, currentPage, activeChat,
         msgPollTimer, setMsgPollTimer, registrationFee, setRegistrationFee } from './state.js';
import { setSplashStatus, showToast, updateFeeDisplay } from './helpers.js';
import { privimeInvoke } from './shader.js';
import { loadShaderBytes } from './shader.js';
import { readMessages, sendReadReceipts } from './sbbs.js';
import { loadFromStorage, saveToStorage, setUserStorageScope, CONV_VERSION,
         contractStartTs, userStorageKey } from './storage.js';
import { subscribeToEvents, registerEventHandlers } from './wallet-api.js';
import { showHome } from './navigation.js';
import { renderChatMessages } from './render-chat.js';
import { renderContactList } from './render-home.js';
import { resolveWalletIdToContact, findHandleByWalletId, resolveHandleIntoContact } from './contacts.js';
import { normalizeWalletId } from './registration.js';
import { registerApplyMyHandle } from './registration.js';
import { isBlocked } from './block-user.js';

// ================================================================
// STARTUP SEQUENCE
// ================================================================
export function onWalletConnected() {
    setConnected(true);
    document.getElementById('statusDot').className = 'status-dot connected';

    loadFromStorage(); // Restore myHandle, conversations, contacts before first render

    // If we have a cached handle, show home instantly -- don't wait for shader
    if (myHandle) {
        showHome();
        startMsgPoll();
    } else {
        setSplashStatus('Loading shader...');
    }

    loadShaderBytes(function(err) {
        if (err) { setSplashStatus('Error: ' + err); return; }
        subscribeToEvents();
        refreshAll(); // Verify identity in background, update if changed
    });
}

export function refreshAll() {
    if (!isConnected || !shaderBytes) return;
    // Fetch registration fee from contract
    privimeInvoke('manager', 'view_pool', {}, function(r) {
        if (r && r.pool && r.pool.registration_fee) {
            setRegistrationFee(parseFloat((r.pool.registration_fee / GROTH_PER_BEAM).toFixed(8)));
            updateFeeDisplay();
        }
    });
    privimeInvoke('user', 'my_handle', {}, function(result) {
        if (result && !result.error) {
            applyMyHandle(result);
        } else {
            // Not registered -- clear identity so stale data can't leak across wallet instances
            setMyHandle(null);
            setMyWalletId(null);
            showHome();
        }
    });
}

export function applyMyHandle(result) {
    if (result.registered) {
        var confirmedHandle = result.handle.toLowerCase();
        var scopeChanged = !myHandle || myHandle.handle.toLowerCase() !== confirmedHandle;

        if (scopeChanged) {
            // Different handle than cached -- switch storage scope and apply the same
            // CONV_VERSION check that loadFromStorage does for the primary scope.
            setUserStorageScope(confirmedHandle);
            setConversations({});
            setContacts({});
            // Re-import userStorageKey after setUserStorageScope changed it
            var _userStorageKey = userStorageKey;
            var sv2 = parseInt(localStorage.getItem(_userStorageKey + '_convver') || '1');
            if (sv2 < CONV_VERSION) {
                localStorage.removeItem(_userStorageKey + '_conv');
                localStorage.removeItem(_userStorageKey + '_contacts');
                localStorage.setItem(_userStorageKey + '_convver', CONV_VERSION);
            } else {
                var saved = localStorage.getItem(_userStorageKey + '_conv');
                if (saved) try { setConversations(JSON.parse(saved)); } catch(e) {}
                saved = localStorage.getItem(_userStorageKey + '_contacts');
                if (saved) try { setContacts(JSON.parse(saved)); } catch(e) {}
            }
        }

        setMyHandle({
            handle: result.handle,
            wallet_id: result.wallet_id,
            display_name: result.display_name || '',
            registered_height: result.registered_height
        });
        setMyWalletId(normalizeWalletId(result.wallet_id) || result.wallet_id);
        saveToStorage();
        showHome();
        // Use all=true on startup to retrieve any messages that arrived while offline.
        // read_messages(all=false) only returns messages not yet consumed in this session,
        // so pending messages from offline periods could be missed without all=true here.
        loadMessages(true);
        if (!msgPollTimer) startMsgPoll();
    } else {
        setMyHandle(null);
        setMyWalletId(null);
        showHome();
    }
}

// Register applyMyHandle with registration.js to break circular dependency
registerApplyMyHandle(applyMyHandle);

export function loadMessages(fetchAll) {
    readMessages(fetchAll, function(result) {
        if (!result || result.error) return;
        processMessages(result);
    });
}

export function processMessages(result) {
    // read_messages returns { messages: [...] } or array
    var msgs = result.messages || result || [];
    if (!Array.isArray(msgs)) return;

    setAllMessages(msgs);
    var newUnread = {};

    msgs.forEach(function(m) {
        var payload = getMessagePayload(m);
        if (!payload) return;
        if (payload.v !== 1) return;

        // Skip messages from before this contract instance was first opened
        if (payload.ts && payload.ts < contractStartTs) return;

        // Sanitize payload.from -- only valid handle chars (a-z, 0-9, underscore).
        // Prevents XSS via crafted SBBS messages injecting JS into onclick attributes.
        if (payload.from) {
            payload.from = payload.from.replace(/^@/, '').toLowerCase().replace(/[^a-z0-9_]/g, '');
            if (!payload.from) return; // entirely invalid -- skip message
        }

        // Handle read receipts (ack messages)
        if (payload.t === 'ack' && payload.read && Array.isArray(payload.read)) {
            var ackFrom = payload.from ? '@' + payload.from : null;
            if (ackFrom && conversations[ackFrom]) {
                var changed = false;
                payload.read.forEach(function(ts) {
                    conversations[ackFrom].forEach(function(msg) {
                        if (msg.sent && msg.ts === ts && !msg.read) {
                            msg.read = true;
                            changed = true;
                        }
                    });
                });
                if (changed) {
                    saveToStorage();
                    if (currentPage === 'chat' && activeChat === ackFrom) renderChatMessages(ackFrom);
                }
            }
            return;
        }

        if (payload.t !== 'dm' && payload.t !== 'tip' && payload.t !== 'file') return;

        // Skip messages from blocked users
        if (payload.from && isBlocked(payload.from.replace(/^@/, '').toLowerCase())) return;

        // Raw sender key from SBBS (may be 33-byte compressed pubkey = 66 hex chars)
        var senderWalletId = (m.sender || m.from || '').trim();
        if (!senderWalletId) return;

        var text = payload.msg || '';
        var ts   = payload.ts  || 0;

        // Detect sent messages from payload.from matching our own handle
        var sent = !!(myHandle && payload.from &&
                      payload.from.replace(/^@/, '').toLowerCase() === myHandle.handle.toLowerCase());

        // NOTE: We do NOT filter by payload.to. Each wallet instance has its own SBBS
        // inbox (one identity per wallet, enforced by the contract's UserKey derivation).
        // Messages are naturally isolated -- no cross-account filtering needed.

        // Determine conversation key -- always use the registered receive address so
        // incoming and outgoing messages for the same person share ONE conversation.
        var convKey;
        if (sent) {
            // Prefer payload.to (recipient handle) -- always correct, set by sender.
            // Wallet_id reverse lookup is unreliable: multiple handles can share one address.
            if (payload.to) {
                convKey = '@' + payload.to.toLowerCase();
            } else {
                // Legacy fallback for messages sent before payload.to was added
                var receiverWid = (m.receiver || m.to || '').trim() || '';
                if (!receiverWid) return;
                var foundHandle = findHandleByWalletId(receiverWid);
                convKey = foundHandle ? '@' + foundHandle : receiverWid;
            }
        } else if (payload.from) {
            var fromHandle = payload.from.replace(/^@/, '').toLowerCase();
            // '@handle' is the permanent conversation key. Handles are contract-enforced unique.
            // wallet_ids are NOT safe to use as keys: multiple handles on the same Beam wallet
            // instance may register with the same wallet address, causing collisions.
            var hKey = '@' + fromHandle;
            if (!contacts[hKey]) contacts[hKey] = { handle: fromHandle };
            // Update display name from message payload if sender included it
            if (payload.dn && contacts[hKey].display_name !== payload.dn) {
                contacts[hKey].display_name = payload.dn;
            }
            convKey = hKey;
            if (!contacts[hKey].wallet_id && !contacts[hKey]._resolving) {
                contacts[hKey]._resolving = true;
                resolveHandleIntoContact(fromHandle, hKey);
            }
        } else {
            convKey = senderWalletId;
            if (!contacts[senderWalletId] || !contacts[senderWalletId]._resolving) {
                if (!contacts[senderWalletId]) contacts[senderWalletId] = {};
                contacts[senderWalletId]._resolving = true;
                resolveWalletIdToContact(senderWalletId);
            }
        }
        if (!convKey) return;

        // Skip messages that belong to a deleted conversation (tombstone check).
        // Tombstones are keyed by wallet_id or '@handle' with the max ts at deletion time.
        // Messages sent AFTER deletion (ts > tombstoneTs) still come through.
        var tombstoneTs = deletedConvs[convKey];
        if (tombstoneTs && ts <= tombstoneTs) return;

        if (!conversations[convKey]) conversations[convKey] = [];

        // Avoid duplicates by ts+text+sent (or ts+sent+cid for files)
        var exists;
        if (payload.t === 'file' && payload.file) {
            var fileCid = payload.file.cid;
            exists = conversations[convKey].some(function(x) {
                return x.ts === ts && x.sent === sent && x.file && x.file.cid === fileCid;
            });
        } else {
            exists = conversations[convKey].some(function(x) { return x.ts === ts && x.text === text && x.sent === sent; });
        }
        if (!exists) {
            var msgData = { text: text, ts: ts, sent: sent, reply: payload.reply || null };
            if (payload.t === 'tip') {
                msgData.isTip = true;
                msgData.tipAmount = payload.amount || 0;
            }
            if (payload.t === 'file' && payload.file) {
                msgData.file = {
                    cid: payload.file.cid || '',
                    key: payload.file.key || '',
                    iv: payload.file.iv || '',
                    name: payload.file.name || 'file',
                    size: payload.file.size || 0,
                    mime: payload.file.mime || 'application/octet-stream'
                };
                msgData.text = payload.msg || ''; // caption
            }
            conversations[convKey].push(msgData);
            // Only count as unread if this is a genuinely new message (not a re-processed one)
            if (!sent && convKey !== activeChat) {
                newUnread[convKey] = (newUnread[convKey] || 0) + 1;
            }
        }
    });

    // Sort conversations chronologically
    for (var key in conversations) {
        conversations[key].sort(function(a, b) { return a.ts - b.ts; });
    }

    // Merge new unreads into existing counts (don't replace -- previous polls' counts must survive)
    for (var k in newUnread) {
        unreadCounts[k] = (unreadCounts[k] || 0) + newUnread[k];
    }

    saveToStorage();

    if (currentPage === 'chat' && activeChat) {
        renderChatMessages(activeChat);
        if (unreadCounts[activeChat]) {
            unreadCounts[activeChat] = 0;
            saveToStorage();
        }
        // Send read receipts immediately for new messages arriving while chat is open
        sendReadReceipts(activeChat);
    }
    if (currentPage === 'home') {
        renderContactList();
    }
}

export function getMessagePayload(m) {
    try {
        var p = m.message || m.payload || m;
        if (typeof p === 'string') p = JSON.parse(p);
        return (p && p.v) ? p : null;
    } catch (e) { return null; }
}

// Lightweight identity check -- detects wallet switches without full refresh
export function checkIdentityChange() {
    if (!isConnected || !shaderBytes) return;
    privimeInvoke('user', 'my_handle', {}, function(result) {
        if (result && !result.error && result.registered) {
            var newHandle = result.handle.toLowerCase();
            if (myHandle && myHandle.handle.toLowerCase() !== newHandle) {
                // Identity changed (user switched wallet or account) -- full re-init
                applyMyHandle(result);
            }
        } else if (result && !result.error && !result.registered && myHandle) {
            // Handle was released or wallet changed to one without a handle
            setMyHandle(null);
            showHome();
        }
    });
}

// ================================================================
// POLLING
// ================================================================
export function startMsgPoll() {
    if (msgPollTimer) clearInterval(msgPollTimer);
    setMsgPollTimer(setInterval(function() {
        if (isConnected && myHandle) loadMessages(false);
    }, MSG_REFRESH_MS));
}

// Register event handlers with wallet-api.js (breaks circular dependency)
registerEventHandlers({
    checkIdentityChange: checkIdentityChange,
    refreshAll: refreshAll,
    loadMessages: loadMessages,
    showHome: showHome
});

