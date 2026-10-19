package javax.microedition.io;
public class Connector {
    public static final int READ = 1, WRITE = 2, READ_WRITE = 3;
    public static Connection open(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static Connection open(String n, int m) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static Connection open(String n, int m, boolean t) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.InputStream openInputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.DataInputStream openDataInputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.OutputStream openOutputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
    public static java.io.DataOutputStream openDataOutputStream(String n) throws java.io.IOException { throw new ConnectionNotFoundException(n); }
}
