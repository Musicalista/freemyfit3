package java.io;
public class PrintStream extends OutputStream {
    public PrintStream() {}
    public void write(int b) {}
    public void print(String s) { jvmhost.Host.log(s); }
    public void print(Object o) { jvmhost.Host.log(String.valueOf(o)); }
    public void print(int i) { jvmhost.Host.log(Integer.toString(i)); }
    public void print(char c) { jvmhost.Host.log(String.valueOf(c)); }
    public void println() {}
    public void println(String s) { jvmhost.Host.log(s); }
    public void println(Object o) { jvmhost.Host.log(String.valueOf(o)); }
    public void println(int i) { jvmhost.Host.log(Integer.toString(i)); }
    public void println(long i) { jvmhost.Host.log(Long.toString(i)); }
    public void println(boolean b) { jvmhost.Host.log(b ? "true" : "false"); }
    public void println(char c) { jvmhost.Host.log(String.valueOf(c)); }
}
