package java.util;
public class TimeZone {
    private static TimeZone def = new TimeZone();
    public static TimeZone getDefault() { return def; }
    public static TimeZone getTimeZone(String id) { return def; }
    public int getRawOffset() { return 0; }
    public String getID() { return "UTC"; }
}
