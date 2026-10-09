package com.fit3.hub;

import java.nio.file.*;
import java.util.*;

/** PC-only test: checks Chacha against the vectors that Node's reference implementation produced (net/vectors.txt). */
public final class ChachaTest {
    static byte[] hex(String s) { if (s.equals("-")) return new byte[0]; byte[] b = new byte[s.length() / 2]; for (int i = 0; i < b.length; i++) b[i] = (byte) Integer.parseInt(s.substring(2 * i, 2 * i + 2), 16); return b; }
    public static void main(String[] a) throws Exception {
        int ok = 0, bad = 0;
        for (String line : Files.readAllLines(Paths.get(a[0]))) {
            if (line.isBlank()) continue; String[] t = line.trim().split(" ");
            byte[] key = hex(t[0]), nonce = hex(t[1]), aad = hex(t[2]), pt = hex(t[3]), want = hex(t[4]);
            byte[] got = Chacha.seal(key, nonce, aad, pt);
            boolean good = Arrays.equals(got, want);
            byte[] back = Chacha.open(key, nonce, aad, want, 0, pt.length);
            good &= back != null && Arrays.equals(back, pt);
            byte[] tam = want.clone(); tam[tam.length - 1] ^= 1; good &= Chacha.open(key, nonce, aad, tam, 0, pt.length) == null;
            if (good) ok++; else { bad++; System.out.println("FAIL pt=" + pt.length + " aad=" + aad.length); }
        }
        System.out.println(ok + " vectors ok, " + bad + " failures");
        if (bad != 0) System.exit(1);
    }
}
