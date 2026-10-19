package javax.microedition.lcdui;
import java.util.Vector;
public abstract class Displayable {
    public Vector commands = new Vector(); CommandListener listener; String title; public boolean dirty = true;
    public void addCommand(Command c) { if (!commands.contains(c)) { int i = 0; while (i < commands.size() && ((Command) commands.elementAt(i)).priority <= c.priority) i++; commands.insertElementAt(c, i); } dirty = true; }
    public void removeCommand(Command c) { commands.removeElement(c); dirty = true; }
    public void setCommandListener(CommandListener l) { listener = l; }
    public void setTicker(Ticker t) {}
    public Ticker getTicker() { return null; }
    public String getTitle() { return title; }
    public void setTitle(String t) { title = t; dirty = true; }
    public boolean isShown() { return Display.cur == this; }
    public int getWidth() { return jvmhost.Host.width(); }
    public int getHeight() { return jvmhost.Host.height(); }
    public void _fire(Command c) { if (listener != null) listener.commandAction(c, this); }
    public void _shown() { dirty = true; }
    public void _hidden() {}
    public void _key(int code, boolean down) {}
    public void _pointer(int type, int x, int y) {}
    public void _tick() {}
}
