package java.lang;
public final class Short {
    public static final short MIN_VALUE = -32768, MAX_VALUE = 32767;
    private short v;
    public Short(short b) { v = b; }
    public short shortValue() { return v; }
    public int hashCode() { return v; }
    public boolean equals(Object o) { return o instanceof Short && ((Short) o).v == v; }
    public String toString() { return Integer.toString(v); }
    public static short parseShort(String s) { int i = Integer.parseInt(s); if (i < -32768 || i > 32767) throw new NumberFormatException(s); return (short) i; }
}
