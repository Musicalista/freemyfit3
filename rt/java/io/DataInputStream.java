package java.io;
public class DataInputStream extends InputStream {
    protected InputStream in;
    public DataInputStream(InputStream i) { in = i; }
    public int read() throws IOException { return in.read(); }
    public int read(byte[] b, int off, int len) throws IOException { return in.read(b, off, len); }
    public void close() throws IOException { in.close(); }
    public final void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }
    public final void readFully(byte[] b, int off, int len) throws IOException {
        int n = 0; while (n < len) { int c = in.read(b, off + n, len - n); if (c < 0) throw new EOFException(); n += c; }
    }
    public final int readUnsignedByte() throws IOException { int c = in.read(); if (c < 0) throw new EOFException(); return c; }
    public final byte readByte() throws IOException { return (byte) readUnsignedByte(); }
    public final boolean readBoolean() throws IOException { return readUnsignedByte() != 0; }
    public final short readShort() throws IOException { return (short) ((readUnsignedByte() << 8) | readUnsignedByte()); }
    public final int readUnsignedShort() throws IOException { return (readUnsignedByte() << 8) | readUnsignedByte(); }
    public final int readInt() throws IOException { return (readUnsignedByte() << 24) | (readUnsignedByte() << 16) | (readUnsignedByte() << 8) | readUnsignedByte(); }
    public final long readLong() throws IOException { return ((long) readInt() << 32) | (readInt() & 0xffffffffL); }
}
