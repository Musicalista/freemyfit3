package javax.microedition.lcdui;
public final class Font {
    public static final int FACE_SYSTEM = 0, FACE_MONOSPACE = 32, FACE_PROPORTIONAL = 64;
    public static final int STYLE_PLAIN = 0, STYLE_BOLD = 1, STYLE_ITALIC = 2, STYLE_UNDERLINED = 4;
    public static final int SIZE_SMALL = 8, SIZE_MEDIUM = 0, SIZE_LARGE = 16;
    public static final int FONT_STATIC_TEXT = 0, FONT_INPUT_TEXT = 1;
    int face, style, size; private static Font def = new Font(0, 0, 0);
    private Font(int f, int st, int sz) { face = f; style = st; size = sz; }
    public static Font getDefaultFont() { return def; }
    public static Font getFont(int face, int style, int size) { return new Font(face, style, size); }
    public static Font getFont(int spec) { return def; }
    public int getStyle() { return style; }
    public int getSize() { return size; }
    public int getFace() { return face; }
    public boolean isPlain() { return style == 0; }
    public boolean isBold() { return (style & 1) != 0; }
    public boolean isItalic() { return (style & 2) != 0; }
    public boolean isUnderlined() { return (style & 4) != 0; }
    public int getHeight() { return size == SIZE_LARGE ? 14 : 12; }
    public int getBaselinePosition() { return size == SIZE_LARGE ? 11 : 10; }
    public int charWidth(char c) { return size == SIZE_LARGE ? 9 : 8; }
    public int charsWidth(char[] ch, int off, int len) { return len * charWidth('x'); }
    public int stringWidth(String s) { return s.length() * charWidth('x'); }
    public int substringWidth(String s, int off, int len) { return len * charWidth('x'); }
}
