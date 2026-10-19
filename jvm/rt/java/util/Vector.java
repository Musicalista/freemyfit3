package java.util;
public class Vector {
    protected Object[] elementData; protected int elementCount; protected int capacityIncrement;
    public Vector() { this(10, 0); }
    public Vector(int n) { this(n, 0); }
    public Vector(int n, int inc) { if (n < 0) throw new IllegalArgumentException(); elementData = new Object[n < 1 ? 1 : n]; capacityIncrement = inc; }
    private void grow(int need) {
        if (need <= elementData.length) return;
        int n = capacityIncrement > 0 ? elementData.length + capacityIncrement : elementData.length * 2; if (n < need) n = need;
        Object[] a = new Object[n]; System.arraycopy(elementData, 0, a, 0, elementCount); elementData = a;
    }
    public final int size() { return elementCount; }
    public final int capacity() { return elementData.length; }
    public final boolean isEmpty() { return elementCount == 0; }
    public final void ensureCapacity(int n) { grow(n); }
    public final void trimToSize() {}
    public final void addElement(Object o) { grow(elementCount + 1); elementData[elementCount++] = o; }
    public final Object elementAt(int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(i + " >= " + elementCount); return elementData[i]; }
    public final Object firstElement() { if (elementCount == 0) throw new NoSuchElementException(); return elementData[0]; }
    public final Object lastElement() { if (elementCount == 0) throw new NoSuchElementException(); return elementData[elementCount - 1]; }
    public final void setElementAt(Object o, int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(); elementData[i] = o; }
    public final void removeElementAt(int i) {
        if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException();
        System.arraycopy(elementData, i + 1, elementData, i, elementCount - i - 1); elementData[--elementCount] = null;
    }
    public final void insertElementAt(Object o, int i) {
        if (i < 0 || i > elementCount) throw new ArrayIndexOutOfBoundsException(); grow(elementCount + 1);
        System.arraycopy(elementData, i, elementData, i + 1, elementCount - i); elementData[i] = o; elementCount++;
    }
    public final boolean removeElement(Object o) { int i = indexOf(o); if (i < 0) return false; removeElementAt(i); return true; }
    public final void removeAllElements() { for (int i = 0; i < elementCount; i++) elementData[i] = null; elementCount = 0; }
    public final void setSize(int n) { grow(n); for (int i = n; i < elementCount; i++) elementData[i] = null; elementCount = n; }
    public final boolean contains(Object o) { return indexOf(o) >= 0; }
    public final int indexOf(Object o) { return indexOf(o, 0); }
    public final int indexOf(Object o, int from) {
        for (int i = from; i < elementCount; i++) { Object e = elementData[i]; if (o == null ? e == null : o.equals(e)) return i; }
        return -1;
    }
    public final int lastIndexOf(Object o) { for (int i = elementCount - 1; i >= 0; i--) { Object e = elementData[i]; if (o == null ? e == null : o.equals(e)) return i; } return -1; }
    public final void copyInto(Object[] a) { System.arraycopy(elementData, 0, a, 0, elementCount); }
    public final Enumeration elements() {
        final Vector v = this;
        return new Enumeration() { int i = 0;
            public boolean hasMoreElements() { return i < v.elementCount; }
            public Object nextElement() { if (i >= v.elementCount) throw new NoSuchElementException(); return v.elementData[i++]; } };
    }
    public String toString() { StringBuffer sb = new StringBuffer("["); for (int i = 0; i < elementCount; i++) { if (i > 0) sb.append(", "); sb.append(elementData[i]); } return sb.append("]").toString(); }
}
