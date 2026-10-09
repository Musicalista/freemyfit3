package com.fit3.hub;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.GZIPInputStream;

/** Fetches a page (or runs a search) and converts it to the tiny text format the watch reader understands. Port of webbridge/server.js. No Android APIs. */
public final class WatchPage {
    private WatchPage() {}
    static final int MAX_BODY = 18000, MAX_LINKS = 40, LINK_URL = 120;
    private static final String SUPPORTED = "áàâãäéèêëíìîïóòôõöúùûüçñÁÀÂÃÄÉÈÊÍÌÓÒÔÕÖÚÙÛÜÇÑ¿¡ºª°€£";
    private static final String UA = "Mozilla/5.0 (compatible; Fit3Hub/1.0)";

    /** The watch font has ASCII + Portuguese accents + a few symbols: map everything else to something readable. */
    public static String normalize(String s) {
        s = s.replaceAll("[\u200b-\u200f\u2060\ufeff\u00ad]", "").replaceAll("[\u00a0\u2000-\u200a\u202f\u205f\u3000]", " ");
        s = s.replaceAll("[\u201c\u201d\u201e\u00ab\u00bb]", "\"").replaceAll("[\u2018\u2019\u201a\u2039\u203a`\u00b4]", "'").replaceAll("[\u2013\u2014\u2212]", "-")
             .replace("\u2026", "...").replaceAll("[\u2022\u00b7\u25cf\u25aa\u25a0\u25e6]", "*").replaceAll("[\u00d7\u2715\u2716]", "x").replace("\u2192", "->").replace("\u2190", "<-")
             .replace("\u2122", "(tm)").replace("\u00a9", "(c)").replace("\u00ae", "(R)");
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < s.length(); ) {
            int c = s.codePointAt(i); int n = Character.charCount(c); String ch = s.substring(i, i + n); i += n;
            if (c == '\n' || (c >= 32 && c <= 126) || SUPPORTED.indexOf(ch) >= 0) { out.append(ch); continue; }
            String base = Normalizer.normalize(ch, Normalizer.Form.NFD).replaceAll("[\u0300-\u036f]", "");
            out.append(!base.isEmpty() && !base.equals(ch) && base.matches("[\\x20-\\x7e]+") ? base : "?");
        }
        return out.toString();
    }

    private static final Map<String, String> ENT = new HashMap<>();
    static {
        String[][] e = {{"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"}, {"nbsp", " "}, {"ndash", "-"}, {"mdash", "-"}, {"hellip", "..."}, {"laquo", "\""}, {"raquo", "\""}, {"ldquo", "\""},
            {"rdquo", "\""}, {"lsquo", "'"}, {"rsquo", "'"}, {"copy", "(c)"}, {"reg", "(R)"}, {"euro", "\u20ac"}, {"pound", "\u00a3"}, {"deg", "\u00b0"}, {"middot", "*"}, {"bull", "*"}, {"times", "x"},
            {"aacute", "\u00e1"}, {"agrave", "\u00e0"}, {"acirc", "\u00e2"}, {"atilde", "\u00e3"}, {"eacute", "\u00e9"}, {"ecirc", "\u00ea"}, {"iacute", "\u00ed"}, {"oacute", "\u00f3"}, {"ocirc", "\u00f4"},
            {"otilde", "\u00f5"}, {"uacute", "\u00fa"}, {"ccedil", "\u00e7"}};
        for (String[] p : e) ENT.put(p[0], p[1]);
    }
    private static final Pattern ENT_RE = Pattern.compile("&(#x?[0-9a-f]+|[a-z]+);", Pattern.CASE_INSENSITIVE);
    static String decode(String s) {
        Matcher m = ENT_RE.matcher(s); StringBuffer sb = new StringBuffer();
        while (m.find()) {
            String e = m.group(1), r;
            if (e.charAt(0) == '#') {
                try { int n = (e.charAt(1) == 'x' || e.charAt(1) == 'X') ? Integer.parseInt(e.substring(2), 16) : Integer.parseInt(e.substring(1)); r = new String(Character.toChars(n)); } catch (Exception ex) { r = ""; }
            } else { r = ENT.get(e.toLowerCase()); if (r == null) r = ""; }
            m.appendReplacement(sb, Matcher.quoteReplacement(r));
        }
        m.appendTail(sb); return sb.toString();
    }
    static String resolve(String href, String base) {
        try { URI u = new URI(base).resolve(href.trim().replace(" ", "%20")); String sc = u.getScheme(); return sc != null && (sc.equals("http") || sc.equals("https")) ? u.toString() : null; } catch (Exception e) { return null; }
    }

    private static final Pattern TITLE = Pattern.compile("<title[^>]*>([\\s\\S]*?)</title>", Pattern.CASE_INSENSITIVE);
    private static final Pattern COMMENT = Pattern.compile("<!--[\\s\\S]*?-->");
    private static final Pattern STRIP = Pattern.compile("<(script|style|noscript|svg|iframe|template|head|nav|footer|form|button|select)\\b[\\s\\S]*?</\\1>", Pattern.CASE_INSENSITIVE);
    private static final Pattern ANCHOR = Pattern.compile("<a\\b[^>]*?href\\s*=\\s*(\"([^\"]*)\"|'([^']*)'|([^\\s>]+))[^>]*>([\\s\\S]*?)</a>", Pattern.CASE_INSENSITIVE);
    private static final Pattern TAG = Pattern.compile("<[^>]*>");

    /** Result of the HTML conversion: text where links read "text[n]", plus the link table. */
    static final class Conv { String title, text; List<String> links = new ArrayList<>(); }

    static Conv htmlToWatch(String html, String base) {
        Conv c = new Conv(); Matcher tm = TITLE.matcher(html);
        c.title = tm.find() ? decode(tm.group(1)).replaceAll("\\s+", " ").trim() : base;
        String h = COMMENT.matcher(html).replaceAll("");
        h = STRIP.matcher(h).replaceAll(" ");
        final Map<String, Integer> seen = new HashMap<>();
        Matcher am = ANCHOR.matcher(h); StringBuffer sb = new StringBuffer();
        while (am.find()) {
            String raw = am.group(2) != null ? am.group(2) : am.group(3) != null ? am.group(3) : am.group(4);
            String text = TAG.matcher(am.group(5)).replaceAll("").replaceAll("\\s+", " ").trim(), r;
            if (text.isEmpty()) r = " ";
            else {
                String url = resolve(decode(raw == null ? "" : raw), base);
                if (url == null || (raw != null && raw.startsWith("#"))) r = text;
                else {
                    Integer n = seen.get(url);
                    if (n == null) { if (c.links.size() >= MAX_LINKS) { m_append(am, sb, text); continue; } c.links.add(url); n = c.links.size(); seen.put(url, n); }
                    r = text + "[" + n + "]";
                }
            }
            am.appendReplacement(sb, Matcher.quoteReplacement(r));
        }
        am.appendTail(sb); h = sb.toString();
        h = h.replaceAll("(?i)<h[1-6][^>]*>", "\n\n# ").replaceAll("(?i)</h[1-6]>", "\n").replaceAll("(?i)<li[^>]*>", "\n- ").replaceAll("(?i)<(br|hr)\\b[^>]*>", "\n")
             .replaceAll("(?i)</(p|div|tr|ul|ol|table|section|article|blockquote|pre)>", "\n");
        h = TAG.matcher(h).replaceAll(" ");
        c.text = decode(h).replaceAll("[ \\t\\r\\f\\u000b]+", " ").replaceAll(" ?\\n ?", "\n").replaceAll("\\n{3,}", "\n\n").trim();
        return c;
    }
    private static void m_append(Matcher m, StringBuffer sb, String text) { m.appendReplacement(sb, Matcher.quoteReplacement(text)); }

    /** "W1\nS<seq>\nU<url>\nT<title>\n<body>\n\u0001\nL<n>\t<url>..." */
    static byte[] pack(int seq, String url, String title, String text, List<String> links) {
        String body = normalize(text);
        if (body.length() > MAX_BODY) body = body.substring(0, MAX_BODY) + "\n[...cortado]";
        String u = normalize(url); if (u.length() > 120) u = u.substring(0, 120);
        String t = normalize(title); if (t.length() > 80) t = t.substring(0, 80);
        StringBuilder out = new StringBuilder("W1\nS" + seq + "\nU" + u + "\nT" + t + "\n" + body + "\n\u0001\n");
        for (int i = 0; i < links.size(); i++) if (body.contains("[" + (i + 1) + "]")) { String l = links.get(i); if (l.length() > LINK_URL) l = l.substring(0, LINK_URL); out.append("L").append(i + 1).append('\t').append(l).append('\n'); }
        byte[] b = out.toString().getBytes(StandardCharsets.UTF_8);
        if (b.length > 23500) b = out.substring(0, 23000).concat("\n\u0001\n").getBytes(StandardCharsets.UTF_8);
        return b;
    }

    // ---------------------------------------------------------------- network
    static final class Fetched { String url, contentType, body; }
    static Fetched get(String url) throws Exception {
        for (int hop = 0; hop < 6; hop++) {
            HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
            c.setInstanceFollowRedirects(false); c.setConnectTimeout(12000); c.setReadTimeout(15000);
            c.setRequestProperty("User-Agent", UA); c.setRequestProperty("Accept", "text/html,application/xhtml+xml,text/plain;q=0.9,*/*;q=0.5");
            c.setRequestProperty("Accept-Language", "pt-BR,pt;q=0.9,en;q=0.8"); c.setRequestProperty("Accept-Encoding", "gzip");
            int code = c.getResponseCode();
            if (code >= 300 && code < 400 && c.getHeaderField("Location") != null) { String nu = resolve(c.getHeaderField("Location"), url); if (nu == null) break; url = nu; continue; }
            InputStream in = code >= 400 ? c.getErrorStream() : c.getInputStream(); if (in == null) throw new Exception("HTTP " + code);
            if ("gzip".equalsIgnoreCase(c.getContentEncoding())) in = new GZIPInputStream(in);
            ByteArrayOutputStream bo = new ByteArrayOutputStream(); byte[] buf = new byte[8192]; int n;
            while ((n = in.read(buf)) > 0 && bo.size() < 1500000) bo.write(buf, 0, n);
            in.close();
            byte[] raw = bo.toByteArray(); String ct = c.getContentType() == null ? "" : c.getContentType();
            String enc = "utf-8"; Matcher m = Pattern.compile("charset=([\\w-]+)", Pattern.CASE_INSENSITIVE).matcher(ct);
            if (m.find()) enc = m.group(1);
            else { Matcher m2 = Pattern.compile("<meta[^>]+charset=[\"']?([\\w-]+)", Pattern.CASE_INSENSITIVE).matcher(new String(raw, 0, Math.min(raw.length, 2048), StandardCharsets.ISO_8859_1)); if (m2.find()) enc = m2.group(1); }
            Charset cs; try { cs = Charset.forName(enc); } catch (Exception e) { cs = StandardCharsets.UTF_8; }
            Fetched f = new Fetched(); f.url = url; f.contentType = ct; f.body = new String(raw, cs);
            if (code >= 400) throw new Exception("HTTP " + code);
            return f;
        }
        throw new Exception("too many redirects");
    }

    static byte[] search(String q, int seq) throws Exception {
        Fetched r = get("https://html.duckduckgo.com/html/?q=" + URLEncoder.encode(q, "UTF-8"));
        Pattern re = Pattern.compile("<a[^>]*class=\"[^\"]*result__a[^\"]*\"[^>]*href=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</a>[\\s\\S]*?(?:class=\"[^\"]*result__snippet[^\"]*\"[^>]*>([\\s\\S]*?)</a>)?", Pattern.CASE_INSENSITIVE);
        Matcher m = re.matcher(r.body); List<String> urls = new ArrayList<>(); StringBuilder text = new StringBuilder("Resultados para \"" + q + "\"\n\n");
        while (m.find() && urls.size() < 12) {
            String href = decode(m.group(1)); Matcher u = Pattern.compile("[?&]uddg=([^&]+)").matcher(href);
            if (u.find()) href = java.net.URLDecoder.decode(u.group(1), "UTF-8"); else if (href.startsWith("//")) href = "https:" + href;
            String url = resolve(href, "https://duckduckgo.com/"); if (url == null) continue;
            String title = decode(TAG.matcher(m.group(2)).replaceAll("")).replaceAll("\\s+", " ").trim();
            String snip = m.group(3) == null ? "" : decode(TAG.matcher(m.group(3)).replaceAll("")).replaceAll("\\s+", " ").trim();
            urls.add(url); int n = urls.size();
            text.append(n).append(". ").append(title).append("[").append(n).append("]\n");
            if (!snip.isEmpty()) text.append("   ").append(snip.length() > 160 ? snip.substring(0, 160) : snip).append("\n");
            text.append("\n");
        }
        if (urls.isEmpty()) text.append("(nenhum resultado)");
        return pack(seq, "busca:" + q, "Busca: " + q, text.toString(), urls);
    }

    /** A URL, a bare domain, or free text (which becomes a search). */
    public static byte[] page(String target, int seq) {
        try {
            boolean isUrl = target.matches("(?i)^https?://.*") || (target.matches("^[\\w-]+(\\.[\\w-]+)+(/.*)?$") && !target.matches(".*\\s.*"));
            if (!isUrl) return search(target, seq);
            String url = target.matches("(?i)^https?://.*") ? target : "https://" + target;
            Fetched r = get(url);
            if (r.contentType.toLowerCase().contains("text/plain")) return pack(seq, r.url, r.url, r.body, new ArrayList<String>());
            Conv c = htmlToWatch(r.body, r.url);
            return pack(seq, r.url, c.title, c.text, c.links);
        } catch (Exception e) {
            return ("!" + (e.getMessage() == null ? e.toString() : e.getMessage())).getBytes(StandardCharsets.UTF_8);
        }
    }
}
