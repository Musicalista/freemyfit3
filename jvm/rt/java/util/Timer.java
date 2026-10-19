package java.util;
public class Timer {
    private Vector tasks = new Vector(); private boolean dead; private Thread th;
    public Timer() { th = new Thread(new Runnable() { public void run() { loop(); } }); th.start(); }
    private void loop() {
        while (!dead) {
            long now = System.currentTimeMillis(); TimerTask run = null;
            synchronized (tasks) {
                for (int i = 0; i < tasks.size(); i++) {
                    TimerTask t = (TimerTask) tasks.elementAt(i);
                    if (t.cancelled) { tasks.removeElementAt(i); i--; continue; }
                    if (t.next <= now) { run = t; if (t.period > 0) t.next += t.period; else { t.cancelled = true; } break; }
                }
            }
            if (run != null) { try { run.run(); } catch (Throwable e) {} }
            else { try { Thread.sleep(5); } catch (InterruptedException e) {} }
        }
    }
    private void add(TimerTask t, long delay, long period) {
        if (t.scheduled) throw new IllegalStateException("Task already scheduled or cancelled");
        t.scheduled = true; t.next = System.currentTimeMillis() + delay; t.period = period; synchronized (tasks) { tasks.addElement(t); }
    }
    public void schedule(TimerTask t, long delay) { add(t, delay, 0); }
    public void schedule(TimerTask t, long delay, long period) { add(t, delay, period); }
    public void scheduleAtFixedRate(TimerTask t, long delay, long period) { add(t, delay, period); }
    public void cancel() { dead = true; }
}
