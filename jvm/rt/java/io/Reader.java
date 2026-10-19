package java.io;
public abstract class Reader {
    public abstract int read() throws IOException;
    public void close() throws IOException {}
}
