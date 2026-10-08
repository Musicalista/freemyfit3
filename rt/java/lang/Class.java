package java.lang;
public final class Class {
    int id;
    private Class() {}
    public java.io.InputStream getResourceAsStream(String n) {
        byte[] b = jvmhost.Host.res(n);
        if (b == null) return null;
        return new java.io.ByteArrayInputStream(b);
    }
}
