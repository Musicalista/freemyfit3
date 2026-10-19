package javax.microedition.lcdui;
final class UiLoop implements Runnable {
    public void run() {
        while (true) {
            Display.dispatch();
            Runnable r = null;
            synchronized (Display.serial) { if (Display.serial.size() > 0) { r = (Runnable) Display.serial.elementAt(0); Display.serial.removeElementAt(0); } }
            if (r != null) { try { r.run(); } catch (Throwable t) { jvmhost.Host.log("serial: " + t); } }
            Displayable d = Display.cur; if (d != null) d._tick();
            try { Thread.sleep(8); } catch (InterruptedException e) {}
        }
    }
}
