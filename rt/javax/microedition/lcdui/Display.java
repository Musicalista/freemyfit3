package javax.microedition.lcdui;
import javax.microedition.midlet.MIDlet;
public class Display {
    private static Display instance = new Display();
    private static Displayable current;
    private Display() {}
    public static Display getDisplay(MIDlet m) { return instance; }
    public void setCurrent(Displayable d) { current = d; }
    public Displayable getCurrent() { return current; }
    public static void pump() {
        int ev;
        while ((ev = jvmhost.Host.pollKey()) != 0) {
            ev -= 1;
            int down = ev & 1;
            int code = (ev >> 1) - 4096;
            if (current instanceof Canvas) ((Canvas) current)._key(code, down != 0);
        }
    }
}
