package com.fit3.hub;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;

import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.UUID;
import java.util.zip.CRC32;

/**
 * Bluetooth serial link to the Galaxy Fit3, using the same service and wire protocol as the Fit3 flasher (and webbridge/bridge.html):
 *   AT commands:  "00AT^NAME=arg1=arg2" (no terminator); the watch answers with text such as "OK\r\n"
 *   read a file:  "061<path>\0" -> 0x40 + u32 BE size; then per block "062" -> 0x40 + u32 BE n, n bytes, 4 checksum bytes; a final "062" ends it
 *   write a file: "300" -> "300"; "33bin,<path>,<len>" -> "330"; blocks of up to 39600 bytes + CRC32 (LE) -> "310" each; "32" -> "320"; wait; "34" -> "340"
 * Firmware flashing is deliberately NOT implemented here.
 */
public final class WatchLink implements Closeable {
    public static final UUID SERVICE = UUID.fromString("db764ac8-4b08-7f25-aafe-59d03c27bae3");
    private BluetoothSocket sock; private InputStream in; private OutputStream out;

    public synchronized void open(BluetoothDevice dev) throws IOException {
        close();
        BluetoothAdapter ad = BluetoothAdapter.getDefaultAdapter(); if (ad != null) ad.cancelDiscovery();
        sock = dev.createRfcommSocketToServiceRecord(SERVICE); sock.connect(); in = sock.getInputStream(); out = sock.getOutputStream();
    }
    public synchronized boolean isOpen() { return sock != null && sock.isConnected(); }
    @Override public synchronized void close() { try { if (sock != null) sock.close(); } catch (IOException ignored) {} sock = null; in = null; out = null; }

    private void drain() throws IOException { while (in.available() > 0) in.skip(in.available()); }
    private byte[] readExact(int n, int timeoutMs) throws IOException {
        byte[] b = new byte[n]; int got = 0; long end = System.currentTimeMillis() + timeoutMs;
        while (got < n) {
            int av = in.available();
            if (av > 0) { int r = in.read(b, got, Math.min(av, n - got)); if (r < 0) throw new IOException("conexao encerrada"); got += r; }
            else { if (System.currentTimeMillis() > end) throw new IOException("o relogio nao respondeu"); try { Thread.sleep(8); } catch (InterruptedException e) { throw new IOException("interrompido"); } }
        }
        return b;
    }
    private void expect(String t) throws IOException {
        String got = new String(readExact(t.length(), 15000), StandardCharsets.US_ASCII);
        if (!got.equals(t)) throw new IOException("resposta inesperada: " + got + " (esperado " + t + ")");
    }

    /** Sends one AT command (without the "00AT^" prefix) and returns whatever the watch prints, waiting until it goes quiet. */
    public synchronized String at(String cmd, int maxWaitMs) throws IOException {
        if (!isOpen()) throw new IOException("relogio nao conectado");
        drain(); out.write(("00AT^" + cmd).getBytes(StandardCharsets.UTF_8)); out.flush();
        ByteArrayOutputStream bo = new ByteArrayOutputStream(); long start = System.currentTimeMillis(), lastData = start;
        for (;;) {
            int av = in.available();
            if (av > 0) { byte[] b = new byte[av]; int r = in.read(b); if (r > 0) { bo.write(b, 0, r); lastData = System.currentTimeMillis(); } }
            long now = System.currentTimeMillis();
            if (bo.size() > 0 && now - lastData > 450) break;
            if (now - start > maxWaitMs) break;
            try { Thread.sleep(15); } catch (InterruptedException e) { break; }
        }
        return new String(bo.toByteArray(), StandardCharsets.UTF_8).trim();
    }

    /** Returns the file contents, or null if the file does not exist. */
    public synchronized byte[] readFile(String path) throws IOException {
        if (!isOpen()) throw new IOException("relogio nao conectado");
        drain(); out.write(("061" + path + "\0").getBytes(StandardCharsets.UTF_8)); out.flush();
        byte[] h = readExact(5, 15000); if (h[0] != 0x40) return null;
        int total = ((h[1] & 255) << 24) | ((h[2] & 255) << 16) | ((h[3] & 255) << 8) | (h[4] & 255);
        if (total < 0 || total > 8 * 1024 * 1024) throw new IOException("arquivo grande demais (" + total + " bytes)");
        byte[] data = new byte[total]; int got = 0;
        while (got < total) {
            out.write("062".getBytes(StandardCharsets.US_ASCII)); out.flush();
            byte[] hh = readExact(5, 15000); int n = ((hh[1] & 255) << 24) | ((hh[2] & 255) << 16) | ((hh[3] & 255) << 8) | (hh[4] & 255);
            if (hh[0] != 0x40 || n <= 0 || n > total - got) throw new IOException("bloco invalido");
            byte[] blk = readExact(n, 60000); readExact(4, 60000); System.arraycopy(blk, 0, data, got, n); got += n;
        }
        out.write("062".getBytes(StandardCharsets.US_ASCII)); out.flush(); try { Thread.sleep(100); } catch (InterruptedException ignored) {}
        return data;
    }

    public interface Progress { void percent(int p); }
    public synchronized void writeFile(String path, byte[] data) throws IOException { writeFile(path, data, null); }
    public synchronized void writeFile(String path, byte[] data, Progress pr) throws IOException {
        if (!isOpen()) throw new IOException("relogio nao conectado");
        drain(); out.write("300".getBytes(StandardCharsets.US_ASCII)); out.flush(); expect("300");
        out.write(("33bin," + path + "," + data.length).getBytes(StandardCharsets.UTF_8)); out.flush(); expect("330");
        for (int off = 0; off < data.length; off += 39600) {
            int n = Math.min(39600, data.length - off); byte[] frame = new byte[n + 4]; System.arraycopy(data, off, frame, 0, n);
            CRC32 crc = new CRC32(); crc.update(data, off, n); long c = crc.getValue();
            frame[n] = (byte) c; frame[n + 1] = (byte) (c >>> 8); frame[n + 2] = (byte) (c >>> 16); frame[n + 3] = (byte) (c >>> 24);
            out.write(frame); out.flush(); expect("310");
            if (pr != null) pr.percent((off + n) * 100 / data.length);
        }
        out.write("32".getBytes(StandardCharsets.US_ASCII)); out.flush(); expect("320");
        try { Thread.sleep(250); } catch (InterruptedException ignored) {}
        out.write("34".getBytes(StandardCharsets.US_ASCII)); out.flush(); expect("340");
    }
}
