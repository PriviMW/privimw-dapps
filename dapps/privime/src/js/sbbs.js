'use strict';

import { myHandle, myWalletId, conversations, contacts } from './state.js';
import { callApi } from './wallet-api.js';
import { isValidWalletId } from './registration.js';
import { saveToStorage } from './storage.js';

// ================================================================
// SBBS MESSAGING
// ================================================================
export function sendSbbsMessage(toWalletId, msgObj, callback) {
    callApi('send_message', { receiver: toWalletId, message: msgObj }, callback);
}

// SBBS is best-effort -- retry important messages to improve delivery reliability.
// Recipient-side dedup (ts+text+sent) prevents duplicates.
export function sendSbbsWithRetry(toWalletId, msgObj, retries) {
    sendSbbsMessage(toWalletId, msgObj, function() {});
    for (var i = 1; i <= (retries || 2); i++) {
        (function(delay) {
            setTimeout(function() { sendSbbsMessage(toWalletId, msgObj, function() {}); }, delay);
        })(i * 5000);
    }
}

export function readMessages(fetchAll, callback) {
    callApi('read_messages', { all: fetchAll ? true : false }, callback);
}

// Send read receipts for received messages the user has now seen.
// Batches all unack'd received timestamps into one SBBS ack message.
export function sendReadReceipts(convKey) {
    if (!myHandle || !myWalletId) return;
    var msgs = conversations[convKey];
    if (!msgs || msgs.length === 0) return;

    var receiver = contacts[convKey] && contacts[convKey].wallet_id;
    if (!receiver || !isValidWalletId(receiver)) return;

    // Collect timestamps of received messages not yet ack'd
    var unacked = [];
    msgs.forEach(function(m) {
        if (!m.sent && !m.acked) unacked.push(m.ts);
    });
    if (unacked.length === 0) return;

    // Mark locally as acked so we don't resend
    msgs.forEach(function(m) {
        if (!m.sent && !m.acked) m.acked = true;
    });
    saveToStorage();

    // Send a single ack message
    var ackObj = { v: 1, t: 'ack', read: unacked, from: myHandle.handle };
    sendSbbsMessage(receiver, ackObj, function() {}); // fire-and-forget
}
