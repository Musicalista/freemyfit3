package javax.microedition.rms;
public interface RecordComparator { int PRECEDES = -1, EQUIVALENT = 0, FOLLOWS = 1; int compare(byte[] a, byte[] b); }
