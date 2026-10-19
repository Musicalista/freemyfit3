package java.lang;
public class Thread implements Runnable {
    public static final int MIN_PRIORITY = 1, NORM_PRIORITY = 5, MAX_PRIORITY = 10;
    private Runnable target; private boolean started; private int prio = 5; private String name;
    public Thread() {}
    public Thread(Runnable r) { target = r; }
    public Thread(Runnable r, String n) { target = r; name = n; }
    public Thread(String n) { name = n; }
    public void run() { if (target != null) target.run(); }
    public void start() {
        if (started) throw new IllegalThreadStateException();
        started = true; if (!spawn0()) throw new OutOfMemoryError();
    }
    private native boolean spawn0();
    public final native boolean isAlive();
    public final void join() throws InterruptedException { while (isAlive()) sleep(5); }
    public final void setPriority(int p) { prio = p; }
    public final int getPriority() { return prio; }
    public final String getName() { return name == null ? "Thread" : name; }
    public void interrupt() {}
    public static native Thread currentThread();
    public static native void sleep(long ms) throws InterruptedException;
    public static native void yield();
    public static int activeCount() { return 1; }
    public String toString() { return "Thread[" + getName() + "]"; }
}
