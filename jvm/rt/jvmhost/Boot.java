package jvmhost;
public final class Boot implements Runnable {
    String cls;
    public void run() {
        try {
            Class c = Class.forName(cls);
            javax.microedition.midlet.MIDlet m = (javax.microedition.midlet.MIDlet) c.newInstance();
            javax.microedition.midlet.MIDlet.boot(m);
        } catch (Throwable t) { Host.log("MIDlet failed: " + t + Host.trace()); Host.quit(); }
    }
}
