import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;
import java.util.*;
public class Gc extends MIDlet implements Runnable {
    static class Node { Node next; int[] data; String s; Node(int i, Node n) { next = n; data = new int[16]; for (int k = 0; k < 16; k++) data[k] = i + k; s = "n" + i; } }
    static Vector keep = new Vector(); static boolean bad; static int rounds;
    public void run() {
        Random r = new Random(7);
        for (int round = 0; round < 400; round++) {
            Node head = null; int sum = 0;
            for (int i = 0; i < 300; i++) { head = new Node(i, head); sum += i * 16 + 120; }
            Hashtable h = new Hashtable(); for (int i = 0; i < 100; i++) h.put("k" + i, new Integer(i));
            StringBuffer sb = new StringBuffer(); for (int i = 0; i < 50; i++) sb.append("x" + i);
            keep.addElement(new byte[r.nextInt(2000)]); if (keep.size() > 50) keep.removeElementAt(0);
            int s2 = 0; for (Node n = head; n != null; n = n.next) { for (int k = 0; k < 16; k++) s2 += n.data[k]; if (!n.s.startsWith("n")) bad = true; }
            if (s2 != sum + 0 * 1 && s2 != sumCheck(300)) bad = true;
            for (int i = 0; i < 100; i++) if (((Integer) h.get("k" + i)).intValue() != i) bad = true;
            if (sb.length() != 50 + 45 * 1 + 5 * 0 + 9 * 0 + 0 && sb.toString().indexOf("x49") < 0) bad = true;
            rounds = round; Thread.yield();
        }
        System.out.println("GCTEST done rounds=" + rounds + " bad=" + bad + " free=" + Runtime.getRuntime().freeMemory());
    }
    static int sumCheck(int n) { int s = 0; for (int i = 0; i < n; i++) for (int k = 0; k < 16; k++) s += i + k; return s; }
    protected void startApp() { for (int t = 0; t < 3; t++) new Thread(this).start(); }
    protected void pauseApp() {} protected void destroyApp(boolean u) {}
}
