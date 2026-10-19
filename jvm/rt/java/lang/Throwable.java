package java.lang;
public class Throwable {
    private String message;
    public Throwable() {}
    public Throwable(String m) { message = m; }
    public String getMessage() { return message; }
    public String toString() { String n = getClass().getName(); return message == null ? n : n + ": " + message; }
    public void printStackTrace() { jvmhost.Host.log(toString() + jvmhost.Host.trace()); }
}
