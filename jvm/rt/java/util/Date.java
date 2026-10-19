package java.util;
public class Date {
    private long t;
    public Date() { t = System.currentTimeMillis(); }
    public Date(long t) { this.t = t; }
    public long getTime() { return t; }
    public void setTime(long t) { this.t = t; }
    public boolean equals(Object o) { return o instanceof Date && ((Date) o).t == t; }
    public int hashCode() { return (int) (t ^ (t >>> 32)); }
    public String toString() { return Long.toString(t); }
}
