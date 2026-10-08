package java.lang;
public class Thread implements Runnable {
    private Runnable target;
    public Thread() {}
    public Thread(Runnable r) { target = r; }
    public void run() { if (target != null) target.run(); }
    public void start() { jvmhost.Host.spawn(this); }
    public static void sleep(long ms) throws InterruptedException {
        jvmhost.Host.sleep(ms);
        javax.microedition.lcdui.Display.pump();
    }
    public static void yield() { javax.microedition.lcdui.Display.pump(); }
}
