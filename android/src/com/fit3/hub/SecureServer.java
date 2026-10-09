package com.fit3.hub;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * The watch's secure proxy server (same wire protocol as webbridge/proxy.js and net/netstack.c). No Android APIs.
 *   client -> 8 random bytes (cn);  server -> 8 random bytes (sn), then an encrypted record "OK"
 *   session key: k1 = block(psk, 0, cn||0000)[0..32]; key = block(k1, 1, sn||0000)[0..32]
 *   record: u16 BE length of (ciphertext + 16-byte tag), ciphertext, tag; nonce = dir(1) 000 counter(8, BE); AAD = the 2 length bytes (dir 0 = client->server)
 *   requests: 'G' url/search | 'A' AI question | 'E' echo (tests) | 'B' n test bytes;   responses: records of flag (0 more / 1 last) + up to 1000 data bytes
 */
public final class SecureServer {
    public interface Log { void log(String line); }

    private final byte[] psk; private final int port; private final Groq groq; private final Log log;
    private volatile boolean running; private ServerSocket ss; private final ExecutorService pool = Executors.newCachedThreadPool();
    public final AtomicInteger clients = new AtomicInteger(), requests = new AtomicInteger();
    private static final SecureRandom RNG = new SecureRandom();

    public SecureServer(byte[] psk, int port, Groq groq, Log log) { this.psk = psk; this.port = port; this.groq = groq; this.log = log; }

    public void start() throws IOException {
        ss = new ServerSocket(port, 8, InetAddress.getByName("0.0.0.0")); running = true; log.log("proxy ouvindo na porta " + port);
        Thread t = new Thread(new Runnable() { public void run() {
            while (running) { try { final Socket s = ss.accept(); pool.execute(new Runnable() { public void run() { serve(s); } }); } catch (IOException e) { if (running) log.log("accept: " + e.getMessage()); } }
        } }, "fit3-accept"); t.setDaemon(true); t.start();
    }
    public void stop() { running = false; try { if (ss != null) ss.close(); } catch (IOException ignored) {} pool.shutdownNow(); }

    private static byte[] nonce(int dir, long ctr) { byte[] n = new byte[12]; n[0] = (byte) dir; for (int i = 0; i < 8; i++) n[4 + i] = (byte) (ctr >>> (56 - 8 * i)); return n; }
    private static byte[] derive(byte[] psk, byte[] cn, byte[] sn) {
        byte[] n1 = new byte[12]; System.arraycopy(cn, 0, n1, 0, 8); byte[] k1 = new byte[32]; System.arraycopy(Chacha.block(psk, 0, n1), 0, k1, 0, 32);
        byte[] n2 = new byte[12]; System.arraycopy(sn, 0, n2, 0, 8); byte[] key = new byte[32]; System.arraycopy(Chacha.block(k1, 1, n2), 0, key, 0, 32);
        return key;
    }

    private void serve(Socket sock) {
        String peer = String.valueOf(sock.getRemoteSocketAddress()); clients.incrementAndGet(); log.log("relogio conectou: " + peer);
        Groq.Session session = new Groq.Session();
        try {
            sock.setTcpNoDelay(true); sock.setSoTimeout(5 * 60 * 1000);
            DataInputStream in = new DataInputStream(sock.getInputStream()); OutputStream out = sock.getOutputStream();
            byte[] cn = new byte[8]; in.readFully(cn); byte[] sn = new byte[8]; RNG.nextBytes(sn); out.write(sn);
            byte[] key = derive(psk, cn, sn); long rctr = 0, sctr = 0;
            sctr = send(out, key, sctr, "OK".getBytes(StandardCharsets.US_ASCII));
            for (;;) {
                int cl = in.readUnsignedShort(); if (cl < 17 || cl > 1200) throw new IOException("registro invalido");
                byte[] rec = new byte[cl]; in.readFully(rec);
                byte[] aad = new byte[] {(byte) (cl >> 8), (byte) cl};
                byte[] msg = Chacha.open(key, nonce(0, rctr++), aad, rec, 0, cl - 16);
                if (msg == null) throw new IOException("chave errada ou registro adulterado");
                if (msg.length == 0) continue;
                requests.incrementAndGet(); byte[] reply = handle(msg, session);
                sctr = reply(out, key, sctr, reply);
            }
        } catch (IOException e) {
            log.log("relogio desconectou (" + (e.getMessage() == null ? "fim" : e.getMessage()) + ")");
        } finally { clients.decrementAndGet(); try { sock.close(); } catch (IOException ignored) {} }
    }

    private byte[] handle(byte[] msg, Groq.Session session) {
        char t = (char) (msg[0] & 255); String arg = new String(msg, 1, msg.length - 1, StandardCharsets.UTF_8).trim();
        try {
            switch (t) {
                case 'E': return new String(msg, 1, msg.length - 1, StandardCharsets.UTF_8).getBytes(StandardCharsets.UTF_8);
                case 'B': { int n = Math.min(Integer.parseInt(arg), 65000); byte[] b = new byte[n]; for (int i = 0; i < n; i++) b[i] = (byte) (i % 251); return b; }
                case 'G': log.log("pagina: " + arg); return WatchPage.page(arg, 1);
                case 'A': log.log("IA: " + arg); return groq.ask(arg, session);
                default: return "?unknown request".getBytes(StandardCharsets.UTF_8);
            }
        } catch (Exception e) { return ("!" + e.getMessage()).getBytes(StandardCharsets.UTF_8); }
    }

    private static long reply(OutputStream out, byte[] key, long sctr, byte[] data) throws IOException {
        if (data.length == 0) return send(out, key, sctr, new byte[] {1});
        for (int off = 0; off < data.length; ) {
            int n = Math.min(1000, data.length - off); byte[] rec = new byte[n + 1]; rec[0] = (byte) (off + n >= data.length ? 1 : 0);
            System.arraycopy(data, off, rec, 1, n); off += n; sctr = send(out, key, sctr, rec);
        }
        return sctr;
    }
    private static long send(OutputStream out, byte[] key, long sctr, byte[] payload) throws IOException {
        byte[] aad = new byte[] {(byte) ((payload.length + 16) >> 8), (byte) (payload.length + 16)};
        byte[] ct = Chacha.seal(key, nonce(1, sctr), aad, payload);
        byte[] rec = new byte[2 + ct.length]; rec[0] = aad[0]; rec[1] = aad[1]; System.arraycopy(ct, 0, rec, 2, ct.length);
        out.write(rec); out.flush(); return sctr + 1;
    }
}
