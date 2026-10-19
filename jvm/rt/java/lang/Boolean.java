package java.lang;
public final class Boolean {
    public static final Boolean TRUE = new Boolean(true), FALSE = new Boolean(false);
    private boolean v;
    public Boolean(boolean b) { v = b; }
    public boolean booleanValue() { return v; }
    public int hashCode() { return v ? 1231 : 1237; }
    public boolean equals(Object o) { return o instanceof Boolean && ((Boolean) o).v == v; }
    public String toString() { return v ? "true" : "false"; }
    public static Boolean valueOf(boolean b) { return b ? TRUE : FALSE; }
}
