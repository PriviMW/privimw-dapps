'use strict';

// ================================================================
// FILE ENCRYPTION (AES-256-GCM via Web Crypto API)
// ================================================================

function bufToHex(buf) {
    return Array.from(new Uint8Array(buf)).map(function(b) {
        return b.toString(16).padStart(2, '0');
    }).join('');
}

function hexToBuf(hex) {
    var bytes = new Uint8Array(hex.length / 2);
    for (var i = 0; i < hex.length; i += 2) {
        bytes[i / 2] = parseInt(hex.substring(i, i + 2), 16);
    }
    return bytes;
}

/**
 * Generate a random AES-256-GCM key and IV.
 * Returns { key: CryptoKey, iv: Uint8Array, keyHex: string (64 chars), ivHex: string (24 chars) }
 */
export async function generateFileKey() {
    var key = await crypto.subtle.generateKey(
        { name: 'AES-GCM', length: 256 },
        true, // extractable (needed for hex export)
        ['encrypt', 'decrypt']
    );
    var iv = crypto.getRandomValues(new Uint8Array(12)); // 96-bit IV
    var rawKey = await crypto.subtle.exportKey('raw', key);
    return {
        key: key,
        iv: iv,
        keyHex: bufToHex(rawKey),
        ivHex: bufToHex(iv)
    };
}

/**
 * Encrypt an ArrayBuffer with AES-256-GCM.
 * Returns ciphertext ArrayBuffer (includes 16-byte GCM auth tag).
 */
export async function encryptFile(plaintext, key, iv) {
    return crypto.subtle.encrypt({ name: 'AES-GCM', iv: iv }, key, plaintext);
}

/**
 * Decrypt an ArrayBuffer with AES-256-GCM.
 * keyHex: 64-char hex string, ivHex: 24-char hex string.
 * Returns plaintext ArrayBuffer. Throws on wrong key/iv (GCM auth failure).
 */
export async function decryptFile(ciphertext, keyHex, ivHex) {
    var rawKey = hexToBuf(keyHex);
    var iv = hexToBuf(ivHex);
    var key = await crypto.subtle.importKey(
        'raw', rawKey, 'AES-GCM', false, ['decrypt']
    );
    return crypto.subtle.decrypt({ name: 'AES-GCM', iv: iv }, key, ciphertext);
}
