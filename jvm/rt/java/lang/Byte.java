package java.lang;
public final class Byte {
    public static final byte MIN_VALUE = -128, MAX_VALUE = 127;
    private byte v;
    public Byte(byte b) { v = b; }
    public byte byteValue() { return v; }
    public int hashCode() { return v; }
    public boolean equals(Object o) { return o instanceof Byte && ((Byte) o).v == v; }
    public String toString() { return Integer.toString(v); }
    public static byte parseByte(String s) { int i = Integer.parseInt(s); if (i < -128 || i > 127) throw new NumberFormatException(s); return (byte) i; }
}
