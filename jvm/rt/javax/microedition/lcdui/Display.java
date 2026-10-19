package javax.microedition.lcdui;
import java.util.Vector;
import javax.microedition.midlet.MIDlet;
public class Display {
    static Display inst = new Display(); static Displayable cur; public static int keyStates; static Vector serial = new Vector(); static boolean uiStarted;
    private Display() {}
    public static Display getDisplay(MIDlet m) { if (!uiStarted) { uiStarted = true; new Thread(new UiLoop()).start(); } return inst; }
    public void setCurrent(Displayable d) {
        if (d == null) return;
        Displayable o = cur; if (o != null && o != d) o._hidden();
        cur = d; d._shown();
    }
    public void setCurrent(Alert a, Displayable next) { a.next = next; setCurrent(a); }
    public Displayable getCurrent() { return cur; }
    public void callSerially(Runnable r) { synchronized (serial) { serial.addElement(r); } }
    public boolean isColor() { return true; }
    public int numColors() { return 65536; }
    public int numAlphaLevels() { return 2; }
    public int getColor(int c) { return 0; }
    public int getBorderStyle(boolean h) { return 0; }
    public int getBestImageWidth(int t) { return 32; }
    public int getBestImageHeight(int t) { return 32; }
    public boolean vibrate(int ms) { jvmhost.Host.vibrate(ms); return true; }
    public boolean flashBacklight(int ms) { return true; }
    static void dispatch() {
        int t;
        while ((t = jvmhost.Host.pollEvent()) != 0) {
            int a = jvmhost.Host.pollArg(); Displayable d = cur; if (d == null) continue;
            if (t == 1 || t == 2) { updateKeyState(a, t == 1); d._key(a, t == 1); }
            else d._pointer(t, a & 0xffff, a >>> 16);
        }
    }
    static void updateKeyState(int code, boolean down) {
        int bit = 0;
        switch (code) {
            case -1: case 50: bit = 2; break; case -2: case 56: bit = 64; break; case -3: case 52: bit = 4; break; case -4: case 54: bit = 32; break;
            case -5: case 53: bit = 256; break; case 49: bit = 512; break; case 51: bit = 1024; break; case 55: bit = 2048; break; case 57: bit = 4096; break;
        }
        if (down) keyStates |= bit; else keyStates &= ~bit;
    }
}
