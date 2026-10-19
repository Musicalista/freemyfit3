package java.util;
public class Calendar {
    public static final int YEAR = 1, MONTH = 2, DAY_OF_MONTH = 5, DAY_OF_WEEK = 7, AM_PM = 9, HOUR = 10, HOUR_OF_DAY = 11, MINUTE = 12, SECOND = 13, MILLISECOND = 14, DATE = 5;
    public static final int SUNDAY = 1, MONDAY = 2, TUESDAY = 3, WEDNESDAY = 4, THURSDAY = 5, FRIDAY = 6, SATURDAY = 7;
    private long t; private int[] f = new int[15];
    protected Calendar() {}
    public static Calendar getInstance() { Calendar c = new Calendar(); c.setTime(new Date()); return c; }
    public static Calendar getInstance(TimeZone z) { return getInstance(); }
    public final Date getTime() { return new Date(t); }
    public long getTimeInMillis() { return t; }
    public final void setTime(Date d) { t = d.getTime(); compute(); }
    public void setTimeInMillis(long m) { t = m; compute(); }
    public int get(int field) { return f[field]; }
    public void set(int field, int v) { f[field] = v; }
    private void compute() {
        long ms = t; f[MILLISECOND] = (int) (ms % 1000); long s = ms / 1000; f[SECOND] = (int) (s % 60); long m = s / 60; f[MINUTE] = (int) (m % 60); long h = m / 60;
        f[HOUR_OF_DAY] = (int) (h % 24); f[HOUR] = f[HOUR_OF_DAY] % 12; f[AM_PM] = f[HOUR_OF_DAY] >= 12 ? 1 : 0;
        long days = h / 24; f[DAY_OF_WEEK] = (int) ((days + 4) % 7) + 1;
        long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097; long doe = z - era * 146097;
        long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; long y = yoe + era * 400; long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        long mp = (5 * doy + 2) / 153; long d = doy - (153 * mp + 2) / 5 + 1; long mo = mp < 10 ? mp + 3 : mp - 9; if (mo <= 2) y++;
        f[YEAR] = (int) y; f[MONTH] = (int) mo - 1; f[DAY_OF_MONTH] = (int) d;
    }
}
