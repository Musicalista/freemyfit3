package java.lang;
public final class Float {
    public static final float MAX_VALUE = 3.4028235E38f, MIN_VALUE = 1.4E-45f, POSITIVE_INFINITY = 1.0f / 0.0f, NEGATIVE_INFINITY = -1.0f / 0.0f, NaN = 0.0f / 0.0f;
    private float v;
    public Float(float f) { v = f; }
    public float floatValue() { return v; }
    public int intValue() { return (int) v; }
    public double doubleValue() { return v; }
    public int hashCode() { return floatToIntBits(v); }
    public boolean equals(Object o) { return o instanceof Float && ((Float) o).v == v; }
    public String toString() { return toString(v); }
    public boolean isNaN() { return v != v; }
    public static boolean isNaN(float f) { return f != f; }
    public static native int floatToIntBits(float f);
    public static native float intBitsToFloat(int i);
    public static String toString(float f) { return Double.toString(f); }
    public static float parseFloat(String s) { return (float) Double.parseDouble(s); }
    public static Float valueOf(String s) { return new Float(parseFloat(s)); }
}
