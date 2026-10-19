package javax.microedition.midlet;
public abstract class MIDlet {
    public static MIDlet current; static boolean quit;
    protected MIDlet() { current = this; }
    protected abstract void startApp() throws MIDletStateChangeException;
    protected abstract void pauseApp();
    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;
    public final void notifyDestroyed() { quit(); }
    public final void notifyPaused() {}
    public final void resumeRequest() {}
    public final String getAppProperty(String k) { return null; }
    public final boolean platformRequest(String url) { return false; }
    public final int checkPermission(String p) { return 1; }
    public static void quit() { quit = true; jvmhost.Host.quit(); }
    public static void boot(MIDlet m) throws Exception { m.startApp(); }
}
