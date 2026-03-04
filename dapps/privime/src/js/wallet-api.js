'use strict';

import { BEAM, apiCallbacks, incrementCallId, myHandle,
         isSubscribed, setSubscribed } from './state.js';

// ================================================================
// WALLET API
// ================================================================
export function callApi(method, params, callback) {
    if (!BEAM || !BEAM.api || !BEAM.api.callWalletApi) {
        if (callback) callback({ error: { message: 'Wallet not connected' } });
        return;
    }
    var id = incrementCallId();
    apiCallbacks.set(id, { callback: callback, method: method });
    try {
        var payload = JSON.stringify({ jsonrpc: '2.0', id: id, method: method, params: params || {} });
        BEAM.api.callWalletApi(payload);
    } catch (e) {
        apiCallbacks.delete(id);
        if (callback) callback({ error: { message: e.message } });
    }
}

export function handleApiResult(json) {
    var answer;
    try { answer = typeof json === 'string' ? JSON.parse(json) : json; }
    catch (e) { return; }
    if (answer.result && (answer.result.ev_system_state || answer.result.ev_txs_changed)) {
        onWalletEvent(answer.result);
    }

    var info = apiCallbacks.get(answer.id);
    if (!info) return;
    apiCallbacks.delete(answer.id);

    if (answer.error) { if (info.callback) info.callback({ error: answer.error }); return; }

    var result = answer.result;
    if (result && typeof result.output === 'string') {
        try {
            var shader = JSON.parse(result.output);
            if (shader.error) { if (info.callback) info.callback({ error: shader.error }); return; }
            if (result.raw_data !== undefined) shader.raw_data = result.raw_data;
            if (info.callback) info.callback(shader);
        } catch (e) {
            if (info.callback) info.callback({ error: 'Failed to parse shader response' });
        }
        return;
    }
    if (info.callback) info.callback(result);
}

// ================================================================
// EVENT HANDLER REGISTRATION (breaks circular dependency)
// ================================================================
// startup.js and navigation.js register their handlers here at init time
// so wallet-api.js never imports from them directly.
var _eventHandlers = {
    checkIdentityChange: null,
    refreshAll: null,
    loadMessages: null,
    showHome: null
};

export function registerEventHandlers(handlers) {
    if (handlers.checkIdentityChange) _eventHandlers.checkIdentityChange = handlers.checkIdentityChange;
    if (handlers.refreshAll) _eventHandlers.refreshAll = handlers.refreshAll;
    if (handlers.loadMessages) _eventHandlers.loadMessages = handlers.loadMessages;
    if (handlers.showHome) _eventHandlers.showHome = handlers.showHome;
}

function onWalletEvent(result) {
    if (result.ev_system_state) {
        // System state changed (new block or account switch) -- re-check identity
        if (_eventHandlers.checkIdentityChange) _eventHandlers.checkIdentityChange();
    }
    if (result.ev_txs_changed) {
        if (myHandle) {
            // Already initialized -- new messages only, no need to re-run my_handle
            if (_eventHandlers.loadMessages) _eventHandlers.loadMessages(false);
        } else {
            // Not yet initialized -- full refresh needed (e.g. to detect handle registration TX)
            if (_eventHandlers.refreshAll) _eventHandlers.refreshAll();
        }
    }
}

// ================================================================
// EVENT SUBSCRIPTION
// ================================================================
export function subscribeToEvents() {
    if (isSubscribed) return;
    setSubscribed(true);
    callApi('ev_subunsub', { ev_txs_changed: true, ev_system_state: true }, function() {});
}
