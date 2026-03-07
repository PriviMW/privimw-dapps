import { describe, it, expect } from 'vitest';
import { generateFileKey, encryptFile, decryptFile } from '../crypto.js';

describe('generateFileKey', () => {
    it('returns keyHex (64 chars) and ivHex (24 chars)', async () => {
        var ck = await generateFileKey();
        expect(ck.keyHex).toHaveLength(64);
        expect(ck.ivHex).toHaveLength(24);
        expect(ck.key).toBeDefined();
        expect(ck.iv).toBeInstanceOf(Uint8Array);
        expect(ck.iv).toHaveLength(12);
    });

    it('generates different keys each time', async () => {
        var a = await generateFileKey();
        var b = await generateFileKey();
        expect(a.keyHex).not.toBe(b.keyHex);
    });
});

describe('encryptFile / decryptFile', () => {
    it('roundtrips correctly', async () => {
        var ck = await generateFileKey();
        var original = new TextEncoder().encode('Hello, PriviMe!');
        var ciphertext = await encryptFile(original.buffer, ck.key, ck.iv);

        // Ciphertext should differ from plaintext
        expect(new Uint8Array(ciphertext)).not.toEqual(original);

        // Decrypt should recover original
        var decrypted = await decryptFile(ciphertext, ck.keyHex, ck.ivHex);
        expect(new Uint8Array(decrypted)).toEqual(original);
    });

    it('works with binary data', async () => {
        var ck = await generateFileKey();
        var data = new Uint8Array(1024);
        for (var i = 0; i < data.length; i++) data[i] = i % 256;
        var ciphertext = await encryptFile(data.buffer, ck.key, ck.iv);
        var decrypted = await decryptFile(ciphertext, ck.keyHex, ck.ivHex);
        expect(new Uint8Array(decrypted)).toEqual(data);
    });

    it('throws on wrong key', async () => {
        var ck = await generateFileKey();
        var wrongKey = await generateFileKey();
        var data = new TextEncoder().encode('secret');
        var ciphertext = await encryptFile(data.buffer, ck.key, ck.iv);

        await expect(decryptFile(ciphertext, wrongKey.keyHex, ck.ivHex))
            .rejects.toThrow();
    });

    it('throws on wrong IV', async () => {
        var ck = await generateFileKey();
        var wrongIv = await generateFileKey();
        var data = new TextEncoder().encode('secret');
        var ciphertext = await encryptFile(data.buffer, ck.key, ck.iv);

        await expect(decryptFile(ciphertext, ck.keyHex, wrongIv.ivHex))
            .rejects.toThrow();
    });

    it('works with empty data', async () => {
        var ck = await generateFileKey();
        var data = new Uint8Array(0);
        var ciphertext = await encryptFile(data.buffer, ck.key, ck.iv);
        var decrypted = await decryptFile(ciphertext, ck.keyHex, ck.ivHex);
        expect(new Uint8Array(decrypted)).toEqual(data);
    });
});
