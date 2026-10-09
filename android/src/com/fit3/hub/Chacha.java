package com.fit3.hub;

import java.math.BigInteger;

/** ChaCha20-Poly1305 (RFC 8439), same wire behaviour as net/chacha.c on the watch. Pure Java, no Android APIs. */
public final class Chacha {
    private Chacha() {}

    private static int rotl(int v, int n) { return (v << n) | (v >>> (32 - n)); }
    private static int ld32(byte[] p, int o) { return (p[o] & 255) | ((p[o + 1] & 255) << 8) | ((p[o + 2] & 255) << 16) | ((p[o + 3] & 255) << 24); }
    private static void st32(byte[] p, int o, int v) { p[o] = (byte) v; p[o + 1] = (byte) (v >>> 8); p[o + 2] = (byte) (v >>> 16); p[o + 3] = (byte) (v >>> 24); }

    /** One 64-byte ChaCha20 block. */
    public static byte[] block(byte[] key, int counter, byte[] nonce) {
        int[] s = new int[16], x = new int[16];
        s[0] = 0x61707865; s[1] = 0x3320646e; s[2] = 0x79622d32; s[3] = 0x6b206574;
        for (int i = 0; i < 8; i++) s[4 + i] = ld32(key, 4 * i);
        s[12] = counter; s[13] = ld32(nonce, 0); s[14] = ld32(nonce, 4); s[15] = ld32(nonce, 8);
        System.arraycopy(s, 0, x, 0, 16);
        for (int i = 0; i < 10; i++) {
            qr(x, 0, 4, 8, 12); qr(x, 1, 5, 9, 13); qr(x, 2, 6, 10, 14); qr(x, 3, 7, 11, 15);
            qr(x, 0, 5, 10, 15); qr(x, 1, 6, 11, 12); qr(x, 2, 7, 8, 13); qr(x, 3, 4, 9, 14);
        }
        byte[] out = new byte[64];
        for (int i = 0; i < 16; i++) st32(out, 4 * i, x[i] + s[i]);
        return out;
    }
    private static void qr(int[] x, int a, int b, int c, int d) {
        x[a] += x[b]; x[d] ^= x[a]; x[d] = rotl(x[d], 16); x[c] += x[d]; x[b] ^= x[c]; x[b] = rotl(x[b], 12);
        x[a] += x[b]; x[d] ^= x[a]; x[d] = rotl(x[d], 8); x[c] += x[d]; x[b] ^= x[c]; x[b] = rotl(x[b], 7);
    }
    private static byte[] xor(byte[] key, int counter, byte[] nonce, byte[] in, int off, int len) {
        byte[] out = new byte[len];
        for (int pos = 0; pos < len; pos += 64) {
            byte[] ks = block(key, counter++, nonce); int n = Math.min(64, len - pos);
            for (int i = 0; i < n; i++) out[pos + i] = (byte) (in[off + pos + i] ^ ks[i]);
        }
        return out;
    }

    private static final BigInteger P = BigInteger.ONE.shiftLeft(130).subtract(BigInteger.valueOf(5));
    private static BigInteger le(byte[] b, int off, int len) { byte[] r = new byte[len + 1]; for (int i = 0; i < len; i++) r[len - i] = b[off + i]; return new BigInteger(r); }

    private static byte[] poly1305(byte[] key32, byte[] msg) {
        byte[] rb = new byte[16]; System.arraycopy(key32, 0, rb, 0, 16);
        rb[3] &= 15; rb[7] &= 15; rb[11] &= 15; rb[15] &= 15; rb[4] &= (byte) 252; rb[8] &= (byte) 252; rb[12] &= (byte) 252;
        BigInteger r = le(rb, 0, 16), s = le(key32, 16, 16), acc = BigInteger.ZERO;
        for (int i = 0; i < msg.length; i += 16) {
            int n = Math.min(16, msg.length - i);
            BigInteger blk = le(msg, i, n).add(BigInteger.ONE.shiftLeft(8 * n));
            acc = acc.add(blk).multiply(r).mod(P);
        }
        BigInteger tag = acc.add(s).mod(BigInteger.ONE.shiftLeft(128));
        byte[] out = new byte[16]; byte[] t = tag.toByteArray();
        for (int i = 0; i < 16 && i < t.length; i++) out[i] = t[t.length - 1 - i];
        return out;
    }
    private static byte[] macData(byte[] aad, byte[] ct, int ctLen) {
        int pa = (16 - aad.length % 16) % 16, pc = (16 - ctLen % 16) % 16;
        byte[] m = new byte[aad.length + pa + ctLen + pc + 16]; int o = 0;
        System.arraycopy(aad, 0, m, o, aad.length); o += aad.length + pa;
        System.arraycopy(ct, 0, m, o, ctLen); o += ctLen + pc;
        st32(m, o, aad.length); st32(m, o + 8, ctLen);
        return m;
    }

    /** Returns ciphertext || 16-byte tag. */
    public static byte[] seal(byte[] key, byte[] nonce, byte[] aad, byte[] pt) {
        byte[] ct = xor(key, 1, nonce, pt, 0, pt.length);
        byte[] otk = block(key, 0, nonce); byte[] pk = new byte[32]; System.arraycopy(otk, 0, pk, 0, 32);
        byte[] tag = poly1305(pk, macData(aad, ct, ct.length));
        byte[] out = new byte[ct.length + 16]; System.arraycopy(ct, 0, out, 0, ct.length); System.arraycopy(tag, 0, out, ct.length, 16);
        return out;
    }
    /** ct holds ciphertext followed by the 16-byte tag. Returns the plaintext, or null if authentication fails. */
    public static byte[] open(byte[] key, byte[] nonce, byte[] aad, byte[] ct, int off, int len) {
        byte[] body = new byte[len]; System.arraycopy(ct, off, body, 0, len);
        byte[] otk = block(key, 0, nonce); byte[] pk = new byte[32]; System.arraycopy(otk, 0, pk, 0, 32);
        byte[] tag = poly1305(pk, macData(aad, body, len));
        int diff = 0; for (int i = 0; i < 16; i++) diff |= tag[i] ^ ct[off + len + i];
        if (diff != 0) return null;
        return xor(key, 1, nonce, body, 0, len);
    }
}
