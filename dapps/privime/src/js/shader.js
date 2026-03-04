'use strict';

import { PRIVIME_CID } from './config.js';
import { shaderBytes, setShaderBytes } from './state.js';
import { callApi } from './wallet-api.js';

// ================================================================
// SHADER HELPERS
// ================================================================
export function privimeInvoke(role, action, extra, callback) {
    var parts = ['role=' + role, 'action=' + action];
    for (var k in extra) {
        if (extra.hasOwnProperty(k) && extra[k] !== undefined && extra[k] !== '') {
            parts.push(k + '=' + extra[k]);
        }
    }
    parts.push('cid=' + PRIVIME_CID);
    var apiParams = { args: parts.join(','), create_tx: false };
    if (shaderBytes) apiParams.contract = shaderBytes;
    callApi('invoke_contract', apiParams, callback);
}

export function privimeTx(role, action, extra, onReady, callback) {
    privimeInvoke(role, action, extra, function(result) {
        if (result && result.error) { if (callback) callback(result); return; }
        if (result && result.raw_data) {
            if (onReady) onReady();
            callApi('process_invoke_data', { data: result.raw_data }, callback);
        } else {
            if (callback) callback(result);
        }
    });
}

// ================================================================
// SHADER FILE LOADING
// ================================================================
export function loadShaderBytes(callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'app.wasm', true);
    xhr.responseType = 'arraybuffer';
    xhr.onload = function() {
        if (xhr.status !== 200 && xhr.status !== 0) {
            callback('Shader not found (HTTP ' + xhr.status + ')');
            return;
        }
        if (!xhr.response || xhr.response.byteLength === 0) {
            callback('Shader file is empty');
            return;
        }
        setShaderBytes(Array.from(new Uint8Array(xhr.response)));
        callback(null);
    };
    xhr.onerror = function() { callback('Failed to load app shader'); };
    xhr.send();
}
