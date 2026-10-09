package com.fit3.hub;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Minimal JSON reader (objects, arrays, strings, numbers, true/false/null): enough for the Groq responses, and it runs on the PC tests too. */
public final class Json {
    private final String s; private int i;
    private Json(String s) { this.s = s; }

    public static Object parse(String text) { try { Json j = new Json(text); j.ws(); return j.value(); } catch (RuntimeException e) { return null; } }

    /** Walks a path of String keys / Integer indexes; returns null if any step is missing. */
    @SuppressWarnings("unchecked")
    public static Object get(Object node, Object... path) {
        for (Object p : path) {
            if (node == null) return null;
            if (p instanceof String && node instanceof Map) node = ((Map<String, Object>) node).get(p);
            else if (p instanceof Integer && node instanceof List) { List<Object> l = (List<Object>) node; int k = (Integer) p; node = k < l.size() ? l.get(k) : null; }
            else return null;
        }
        return node;
    }

    private void ws() { while (i < s.length() && Character.isWhitespace(s.charAt(i))) i++; }
    private Object value() {
        char c = s.charAt(i);
        if (c == '{') { i++; Map<String, Object> m = new LinkedHashMap<>(); ws(); if (s.charAt(i) == '}') { i++; return m; }
            for (;;) { ws(); String k = str(); ws(); i++; ws(); m.put(k, value()); ws(); char d = s.charAt(i++); if (d == '}') return m; if (d != ',') throw new RuntimeException(); } }
        if (c == '[') { i++; List<Object> l = new ArrayList<>(); ws(); if (s.charAt(i) == ']') { i++; return l; }
            for (;;) { ws(); l.add(value()); ws(); char d = s.charAt(i++); if (d == ']') return l; if (d != ',') throw new RuntimeException(); } }
        if (c == '"') return str();
        if (s.startsWith("true", i)) { i += 4; return Boolean.TRUE; }
        if (s.startsWith("false", i)) { i += 5; return Boolean.FALSE; }
        if (s.startsWith("null", i)) { i += 4; return null; }
        int st = i; while (i < s.length() && "+-0123456789.eE".indexOf(s.charAt(i)) >= 0) i++;
        if (st == i) throw new RuntimeException(); return Double.valueOf(s.substring(st, i));
    }
    private String str() {
        if (s.charAt(i) != '"') throw new RuntimeException(); i++;
        StringBuilder b = new StringBuilder();
        for (;;) { char c = s.charAt(i++); if (c == '"') return b.toString();
            if (c != '\\') { b.append(c); continue; }
            char e = s.charAt(i++);
            switch (e) { case 'n': b.append('\n'); break; case 't': b.append('\t'); break; case 'r': b.append('\r'); break; case 'b': b.append('\b'); break; case 'f': b.append('\f'); break;
                case 'u': b.append((char) Integer.parseInt(s.substring(i, i + 4), 16)); i += 4; break; default: b.append(e); } }
    }
}
