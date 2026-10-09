package com.fit3.hub;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Groq chat for the watch. The API key lives only on the phone. Returns the answer in the watch's page format. No Android APIs. */
public final class Groq {
    /** Settings supplied by the app (SharedPreferences) or by the PC test harness. */
    public interface Config { String key(); String model(); String system(); }

    /** Per-connection chat history, so follow-up questions have context. */
    public static final class Session { final List<String[]> hist = new ArrayList<>(); }

    public static final String DEFAULT_MODEL = "openai/gpt-oss-20b";
    public static final String DEFAULT_SYSTEM = "You are an assistant shown on a tiny smartwatch screen (about 31 characters per line, plain text only). "
        + "Answer in the same language as the user. Be brief and direct: a few short sentences, no markdown, no tables, no emojis.";

    private final Config cfg; private final String base;
    public Groq(Config cfg) { this.cfg = cfg; String b = System.getProperty("groq.base"); this.base = (b != null ? b : "https://api.groq.com/openai/v1").replaceAll("/$", ""); }

    static byte[] page(String title, String body) {
        String b = WatchPage.normalize(body); if (b.length() > 18000) b = b.substring(0, 18000);
        return ("W1\nS1\nU\nT" + WatchPage.normalize(title) + "\n" + b + "\n\u0001\n").getBytes(StandardCharsets.UTF_8);
    }
    static String stripMarkdown(String s) {
        return s.replaceAll("<think>[\\s\\S]*?</think>", "").replaceAll("(?i)```[a-z]*\\n?", "").replaceAll("\\*\\*([^*]+)\\*\\*", "$1").replaceAll("__([^_]+)__", "$1")
                .replaceAll("(?m)^#{1,6}\\s*", "").replaceAll("`([^`]+)`", "$1").replaceAll("(?m)^\\s*[*-]\\s+", "- ").trim();
    }
    static String esc(String s) {
        StringBuilder o = new StringBuilder("\"");
        for (int i = 0; i < s.length(); i++) { char c = s.charAt(i);
            switch (c) { case '"': o.append("\\\""); break; case '\\': o.append("\\\\"); break; case '\n': o.append("\\n"); break; case '\r': o.append("\\r"); break; case '\t': o.append("\\t"); break;
                default: if (c < 32) o.append(String.format("\\u%04x", (int) c)); else o.append(c); } }
        return o.append('"').toString();
    }

    public byte[] ask(String question, Session s) {
        String key = cfg.key();
        if (key == null || key.trim().isEmpty()) return page("IA", "A chave do Groq nao esta configurada.\n\nAbra o app no celular, cole a chave em 'Groq' e toque em Salvar.\n\nGroq API key is not set: open the app and paste it under 'Groq'.");
        String model = cfg.model() == null || cfg.model().trim().isEmpty() ? DEFAULT_MODEL : cfg.model().trim();
        String system = cfg.system() == null || cfg.system().trim().isEmpty() ? DEFAULT_SYSTEM : cfg.system();
        StringBuilder msgs = new StringBuilder("[{\"role\":\"system\",\"content\":" + esc(system) + "}");
        int from = Math.max(0, s.hist.size() - 8);
        for (int i = from; i < s.hist.size(); i++) msgs.append(",{\"role\":\"").append(s.hist.get(i)[0]).append("\",\"content\":").append(esc(s.hist.get(i)[1])).append("}");
        msgs.append(",{\"role\":\"user\",\"content\":").append(esc(question)).append("}]");
        String body = "{\"model\":" + esc(model) + ",\"messages\":" + msgs + ",\"max_tokens\":900,\"temperature\":0.6" + (model.contains("gpt-oss") ? ",\"reasoning_effort\":\"low\"" : "") + "}";
        try {
            HttpURLConnection c = (HttpURLConnection) new URL(base + "/chat/completions").openConnection();
            c.setRequestMethod("POST"); c.setDoOutput(true); c.setConnectTimeout(12000); c.setReadTimeout(40000);
            c.setRequestProperty("Authorization", "Bearer " + key.trim()); c.setRequestProperty("Content-Type", "application/json");
            OutputStream os = c.getOutputStream(); os.write(body.getBytes(StandardCharsets.UTF_8)); os.close();
            int code = c.getResponseCode(); InputStream in = code >= 400 ? c.getErrorStream() : c.getInputStream();
            ByteArrayOutputStream bo = new ByteArrayOutputStream(); byte[] buf = new byte[4096]; int n; if (in != null) while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
            String txt = new String(bo.toByteArray(), StandardCharsets.UTF_8);
            Object j = Json.parse(txt);
            if (code >= 400) { Object err = Json.get(j, "error", "message"); return page("IA", "Erro do Groq (" + code + "): " + (err != null ? err : txt.substring(0, Math.min(300, txt.length())))); }
            Object content = Json.get(j, "choices", 0, "message", "content");
            String answer = stripMarkdown(content == null || content.toString().isEmpty() ? "(sem resposta)" : content.toString());
            s.hist.add(new String[] {"user", question}); s.hist.add(new String[] {"assistant", answer});
            while (s.hist.size() > 12) s.hist.remove(0);
            return page("IA", "Voce: " + question + "\n\n" + answer);
        } catch (Exception e) {
            return page("IA", "Nao consegui falar com o Groq: " + (e instanceof java.net.SocketTimeoutException ? "tempo esgotado" : e.getMessage()));
        }
    }
}
