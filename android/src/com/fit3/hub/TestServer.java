package com.fit3.hub;

import java.nio.file.Files;
import java.nio.file.Paths;

/** PC-only launcher used by the end-to-end tests: java -Dgroq.base=... com.fit3.hub.TestServer <port> <keyhexfile> [groqkey] */
public final class TestServer {
    public static void main(String[] a) throws Exception {
        String hex = new String(Files.readAllBytes(Paths.get(a[1]))).trim(); byte[] key = new byte[32];
        for (int i = 0; i < 32; i++) key[i] = (byte) Integer.parseInt(hex.substring(2 * i, 2 * i + 2), 16);
        final String gk = a.length > 2 ? a[2] : "";
        Groq groq = new Groq(new Groq.Config() { public String key() { return gk; } public String model() { return ""; } public String system() { return ""; } });
        SecureServer s = new SecureServer(key, Integer.parseInt(a[0]), groq, new SecureServer.Log() { public void log(String l) { System.out.println(l); System.out.flush(); } });
        s.dialer = new SecureServer.Dialer() { public void dial(String n) throws Exception { if (n.startsWith("000")) throw new Exception("sem sinal"); System.out.println("DIAL " + n); System.out.flush(); } };
        s.start(); Thread.sleep(Long.MAX_VALUE);
    }
}
