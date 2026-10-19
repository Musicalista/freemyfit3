package java.util;
public class Stack extends Vector {
    public Stack() {}
    public Object push(Object o) { addElement(o); return o; }
    public Object pop() { Object o = peek(); removeElementAt(elementCount - 1); return o; }
    public Object peek() { if (elementCount == 0) throw new EmptyStackException(); return elementData[elementCount - 1]; }
    public boolean empty() { return elementCount == 0; }
    public int search(Object o) { int i = lastIndexOf(o); return i >= 0 ? elementCount - i : -1; }
}
