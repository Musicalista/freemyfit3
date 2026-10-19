package java.io;
public class DataOutputStream extends OutputStream implements DataOutput {
    protected OutputStream out;
    public DataOutputStream(OutputStream o) { out = o; }
    public void write(int b) throws IOException { out.write(b); }
    public void write(byte[] b, int off, int len) throws IOException { out.write(b, off, len); }
    public void flush() throws IOException { out.flush(); }
    public void close() throws IOException { out.close(); }
    public final void writeBoolean(boolean v) throws IOException { out.write(v ? 1 : 0); }
    public final void writeByte(int v) throws IOException { out.write(v); }
    public final void writeShort(int v) throws IOException { out.write(v >> 8); out.write(v); }
    public final void writeChar(int v) throws IOException { out.write(v >> 8); out.write(v); }
    public final void writeInt(int v) throws IOException { out.write(v >> 24); out.write(v >> 16); out.write(v >> 8); out.write(v); }
    public final void writeLong(long v) throws IOException { writeInt((int) (v >> 32)); writeInt((int) v); }
    public final void writeFloat(float f) throws IOException { writeInt(Float.floatToIntBits(f)); }
    public final void writeDouble(double d) throws IOException { writeLong(Double.doubleToLongBits(d)); }
    public final void writeUTF(String s) throws IOException { byte[] b = s.getBytes(); writeShort(b.length); out.write(b, 0, b.length); }
    public final void writeChars(String s) throws IOException { for (int i = 0; i < s.length(); i++) writeChar(s.charAt(i)); }
}
