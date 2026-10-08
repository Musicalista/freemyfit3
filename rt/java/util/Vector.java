package java.util;
public class Vector {
    protected Object[] elementData; protected int elementCount;
    public Vector() { elementData = new Object[10]; }
    public Vector(int cap) { elementData = new Object[cap < 1 ? 1 : cap]; }
    private void grow() { Object[] n = new Object[elementData.length * 2]; System.arraycopy(elementData, 0, n, 0, elementCount); elementData = n; }
    public int size() { return elementCount; }
    public boolean isEmpty() { return elementCount == 0; }
    public void addElement(Object o) { if (elementCount == elementData.length) grow(); elementData[elementCount++] = o; }
    public Object elementAt(int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(); return elementData[i]; }
    public Object firstElement() { return elementAt(0); }
    public Object lastElement() { return elementAt(elementCount - 1); }
    public void setElementAt(Object o, int i) { if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(); elementData[i] = o; }
    public void insertElementAt(Object o, int i) {
        if (i < 0 || i > elementCount) throw new ArrayIndexOutOfBoundsException();
        if (elementCount == elementData.length) grow();
        System.arraycopy(elementData, i, elementData, i + 1, elementCount - i); elementData[i] = o; elementCount++;
    }
    public void removeElementAt(int i) {
        if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException();
        System.arraycopy(elementData, i + 1, elementData, i, elementCount - i - 1); elementData[--elementCount] = null;
    }
    public int indexOf(Object o) { for (int i = 0; i < elementCount; i++) if (o == null ? elementData[i] == null : o.equals(elementData[i])) return i; return -1; }
    public boolean contains(Object o) { return indexOf(o) >= 0; }
    public boolean removeElement(Object o) { int i = indexOf(o); if (i < 0) return false; removeElementAt(i); return true; }
    public void removeAllElements() { for (int i = 0; i < elementCount; i++) elementData[i] = null; elementCount = 0; }
    public void copyInto(Object[] a) { System.arraycopy(elementData, 0, a, 0, elementCount); }
}
