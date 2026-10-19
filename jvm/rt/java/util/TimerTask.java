package java.util;
public abstract class TimerTask implements Runnable {
    boolean cancelled; long next; long period; boolean scheduled;
    protected TimerTask() {}
    public abstract void run();
    public boolean cancel() { boolean r = scheduled && !cancelled; cancelled = true; return r; }
}
