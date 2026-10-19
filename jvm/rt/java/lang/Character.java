package java.lang;
public final class Character {
    public static final char MIN_VALUE = 0, MAX_VALUE = (char) 0xffff;
    public static final int MIN_RADIX = 2, MAX_RADIX = 36;
    private char v;
    public Character(char c) { v = c; }
    public char charValue() { return v; }
    public int hashCode() { return v; }
    public boolean equals(Object o) { return o instanceof Character && ((Character) o).v == v; }
    public String toString() { return String.valueOf(v); }
    public static boolean isDigit(char c) { return c >= '0' && c <= '9'; }
    public static boolean isLowerCase(char c) { return (c >= 'a' && c <= 'z') || (c >= 0xdf && c <= 0xff && c != 0xf7); }
    public static boolean isUpperCase(char c) { return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7); }
    public static boolean isLetter(char c) { return isLowerCase(c) || isUpperCase(c) || c > 0xff; }
    public static boolean isLetterOrDigit(char c) { return isLetter(c) || isDigit(c); }
    public static boolean isSpaceChar(char c) { return c == ' ' || c == 0xa0; }
    public static boolean isWhitespace(char c) { return c == ' ' || (c >= 9 && c <= 13) || (c >= 0x1c && c <= 0x1f); }
    public static char toLowerCase(char c) { return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7) ? (char) (c + 32) : c; }
    public static char toUpperCase(char c) { return (c >= 'a' && c <= 'z') || (c >= 0xe0 && c <= 0xfe && c != 0xf7) ? (char) (c - 32) : c; }
    public static int digit(char c, int radix) {
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'z' ? c - 'a' + 10 : c >= 'A' && c <= 'Z' ? c - 'A' + 10 : -1;
        return d >= 0 && d < radix ? d : -1;
    }
    public static char forDigit(int d, int radix) { return (char) (d < 10 ? '0' + d : 'a' + d - 10); }
}
