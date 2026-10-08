package javax.microedition.rms;
import java.util.Vector;
public class RecordStore {
    private static Vector stores = new Vector();
    private String name; private Vector recs = new Vector(); private int nextId = 1; private boolean open;
    private RecordStore(String n) { name = n; }
    public static RecordStore openRecordStore(String n, boolean create) throws RecordStoreException {
        for (int i = 0; i < stores.size(); i++) { RecordStore s = (RecordStore) stores.elementAt(i); if (s.name.equals(n)) { s.open = true; return s; } }
        if (!create) throw new RecordStoreNotFoundException(n);
        RecordStore s = new RecordStore(n); s.open = true; stores.addElement(s); return s;
    }
    public static void deleteRecordStore(String n) throws RecordStoreException {
        for (int i = 0; i < stores.size(); i++) { RecordStore s = (RecordStore) stores.elementAt(i); if (s.name.equals(n)) { stores.removeElementAt(i); return; } }
        throw new RecordStoreNotFoundException(n);
    }
    public void closeRecordStore() throws RecordStoreException { open = false; }
    public int getNumRecords() throws RecordStoreException { return recs.size(); }
    public int addRecord(byte[] d, int off, int len) throws RecordStoreException {
        byte[] c = new byte[len]; System.arraycopy(d, off, c, 0, len); recs.addElement(c); return nextId++;
    }
    public byte[] getRecord(int id) throws RecordStoreException {
        if (id < 1 || id > recs.size()) throw new InvalidRecordIDException();
        byte[] r = (byte[]) recs.elementAt(id - 1); byte[] c = new byte[r.length]; System.arraycopy(r, 0, c, 0, r.length); return c;
    }
    public void setRecord(int id, byte[] d, int off, int len) throws RecordStoreException {
        if (id < 1 || id > recs.size()) throw new InvalidRecordIDException();
        byte[] c = new byte[len]; System.arraycopy(d, off, c, 0, len); recs.setElementAt(c, id - 1);
    }
}
