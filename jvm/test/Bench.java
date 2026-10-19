import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;
public class Bench extends MIDlet implements Runnable {
    int fld; static int sf;
    int call(int a, int b) { return a + b; }
    public void run() {
        long t0 = System.currentTimeMillis(); int s = 0;
        for (int i = 0; i < 300000; i++) { s += i * 3 ^ (i >> 2); }
        long t1 = System.currentTimeMillis(); System.out.println("loop 300k iters: " + (t1 - t0) + " ms (" + s + ")");
        for (int i = 0; i < 100000; i++) s = call(s, i);
        long t2 = System.currentTimeMillis(); System.out.println("100k calls: " + (t2 - t1) + " ms");
        int[] a = new int[1000]; for (int k = 0; k < 200; k++) for (int i = 0; i < 1000; i++) a[i] = a[i] + i;
        long t3 = System.currentTimeMillis(); System.out.println("200k array rmw: " + (t3 - t2) + " ms");
        for (int i = 0; i < 100000; i++) { fld += i; sf ^= fld; }
        long t4 = System.currentTimeMillis(); System.out.println("100k field ops: " + (t4 - t3) + " ms");
        Image im = Image.createImage(240, 320); Graphics g = im.getGraphics(); g.setColor(0xff0000);
        for (int i = 0; i < 100; i++) g.fillRect(0, 0, 240, 320);
        long t5 = System.currentTimeMillis(); System.out.println("100 full fillRect: " + (t5 - t4) + " ms");
        Image sp = Image.createImage(32, 32); for (int i = 0; i < 300; i++) g.drawImage(sp, (i * 7) % 200, (i * 5) % 280, Graphics.TOP | Graphics.LEFT);
        long t6 = System.currentTimeMillis(); System.out.println("300 sprites 32x32: " + (t6 - t5) + " ms");
        for (int i = 0; i < 200; i++) g.drawString("Hello benchmark text", 4, (i * 3) % 300, Graphics.TOP | Graphics.LEFT);
        long t7 = System.currentTimeMillis(); System.out.println("200 drawString(20): " + (t7 - t6) + " ms");
        StringBuffer sb = new StringBuffer(); for (int i = 0; i < 2000; i++) { sb.append(i); if (sb.length() > 200) sb.setLength(0); }
        long t8 = System.currentTimeMillis(); System.out.println("2000 sb.append(int): " + (t8 - t7) + " ms");
        double d = 0; for (int i = 1; i < 20000; i++) d += Math.sqrt(i) * Math.sin(i);
        long t9 = System.currentTimeMillis(); System.out.println("20k sqrt+sin double: " + (t9 - t8) + " ms (" + (int) d + ")");
        System.out.println("BENCH DONE");
    }
    protected void startApp() { new Thread(this).start(); }
    protected void pauseApp() {} protected void destroyApp(boolean u) {}
}
