package com.fit3.hub;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.InputStream;
import java.util.ArrayDeque;
import java.util.Locale;

/** App-wide state: settings, the shared secret, the log, and the running server. */
public final class Hub {
    private Hub() {}
    public static final int PORT = 8788;
    public static volatile SecureServer server;
    public static volatile WatchLink watch = new WatchLink();
    private static final ArrayDeque<String> LOG = new ArrayDeque<>();
    public interface Listener { void changed(); }
    public static volatile Listener listener;

    public static boolean pt() { return Locale.getDefault().getLanguage().startsWith("pt"); }
    public static String t(String pt, String en) { return pt() ? pt : en; }

    public static synchronized void log(String line) {
        String stamp = new java.text.SimpleDateFormat("HH:mm:ss", Locale.US).format(new java.util.Date());
        LOG.addLast(stamp + " " + line); while (LOG.size() > 300) LOG.removeFirst();
        Listener l = listener; if (l != null) l.changed();
    }
    public static synchronized String logText() { StringBuilder b = new StringBuilder(); for (String s : LOG) b.append(s).append('\n'); return b.toString(); }

    private static SharedPreferences prefs(Context c) { return c.getSharedPreferences("fit3hub", Context.MODE_PRIVATE); }
    public static String groqKey(Context c) { return prefs(c).getString("groq_key", ""); }
    public static String groqModel(Context c) { return prefs(c).getString("groq_model", Groq.DEFAULT_MODEL); }
    public static void saveGroq(Context c, String key, String model) { prefs(c).edit().putString("groq_key", key.trim()).putString("groq_model", model.trim()).apply(); }
    public static String lastWatch(Context c) { return prefs(c).getString("watch_addr", ""); }
    public static void saveWatch(Context c, String addr) { prefs(c).edit().putString("watch_addr", addr).apply(); }

    /** The 32-byte key shared with the watch firmware. It is baked into the app at build time (assets/proxy.key, 64 hex characters). */
    public static byte[] psk(Context c) throws Exception {
        InputStream in = c.getAssets().open("proxy.key"); byte[] b = new byte[256]; int n = in.read(b); in.close();
        String hex = new String(b, 0, n, "US-ASCII").trim(); byte[] k = new byte[32];
        if (hex.length() != 64) throw new IllegalStateException("proxy.key invalida");
        for (int i = 0; i < 32; i++) k[i] = (byte) Integer.parseInt(hex.substring(2 * i, 2 * i + 2), 16);
        return k;
    }
}
