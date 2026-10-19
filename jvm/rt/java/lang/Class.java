package java.lang;
public final class Class {
    int id;
    private Class() {}
    public native String getName();
    public static native Class forName(String n) throws ClassNotFoundException;
    public native Object newInstance() throws InstantiationException, IllegalAccessException;
    public boolean isInstance(Object o) { return o != null && o.getClass() == this; }
    public String toString() { return "class " + getName(); }
    public java.io.InputStream getResourceAsStream(String n) {
        byte[] b = jvmhost.Host.res(n);
        if (b == null) return null;
        return new java.io.ByteArrayInputStream(b);
    }
}
