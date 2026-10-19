package java.io;
public class InputStreamReader extends Reader {
    private InputStream in;
    public InputStreamReader(InputStream i) { in = i; }
    public InputStreamReader(InputStream i, String enc) throws UnsupportedEncodingException { in = i; }
    public int read() throws IOException {
        int c = in.read(); if (c < 0x80) return c;
        if (c >= 0xc2 && c < 0xe0) { int d = in.read(); if (d < 0) return c; return ((c & 0x1f) << 6) | (d & 0x3f); }
        if (c >= 0xe0 && c < 0xf0) { int d = in.read(), e = in.read(); if (e < 0) return c; return ((c & 0x0f) << 12) | ((d & 0x3f) << 6) | (e & 0x3f); }
        return c;
    }
    public void close() throws IOException { in.close(); }
}
