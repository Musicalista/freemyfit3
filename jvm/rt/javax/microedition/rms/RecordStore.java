package javax.microedition.rms;
import java.util.Vector;
public class RecordStore {
    public static final int AUTHMODE_PRIVATE = 0, AUTHMODE_ANY = 1;
    private String name; private Vector recs = new Vector(); private int nextId = 1; private boolean open = true; private int version;
    private RecordStore(String n) { name = n; }
    public static RecordStore openRecordStore(String n, boolean create) throws RecordStoreException {
        byte[] d = jvmhost.Host.rmsRead(n); RecordStore rs = new RecordStore(n);
        if (d == null) { if (!create) throw new RecordStoreNotFoundException(n); rs.flush(); return rs; }
        rs.load(d); return rs;
    }
    public static RecordStore openRecordStore(String n, boolean c, int a, boolean w) throws RecordStoreException { return openRecordStore(n, c); }
    public static void deleteRecordStore(String n) throws RecordStoreException {
        if (!jvmhost.Host.rmsDelete(n)) throw new RecordStoreNotFoundException(n);
    }
    public static String[] listRecordStores() { return null; }
    private void load(byte[] d) {
        int p = 0; nextId = rd(d, p); p += 4; int n = rd(d, p); p += 4;
        for (int i = 0; i < n; i++) { int id = rd(d, p); p += 4; int len = rd(d, p); p += 4; byte[] b = new byte[len]; System.arraycopy(d, p, b, 0, len); p += len; recs.addElement(new Object[] { new Integer(id), b }); }
    }
    private static int rd(byte[] d, int p) { return ((d[p] & 0xff) << 24) | ((d[p + 1] & 0xff) << 16) | ((d[p + 2] & 0xff) << 8) | (d[p + 3] & 0xff); }
    private static void wr(byte[] d, int p, int v) { d[p] = (byte) (v >> 24); d[p + 1] = (byte) (v >> 16); d[p + 2] = (byte) (v >> 8); d[p + 3] = (byte) v; }
    private void flush() {
        int total = 8; for (int i = 0; i < recs.size(); i++) total += 8 + ((byte[]) ((Object[]) recs.elementAt(i))[1]).length;
        byte[] d = new byte[total]; int p = 0; wr(d, p, nextId); p += 4; wr(d, p, recs.size()); p += 4;
        for (int i = 0; i < recs.size(); i++) { Object[] r = (Object[]) recs.elementAt(i); byte[] b = (byte[]) r[1]; wr(d, p, ((Integer) r[0]).intValue()); p += 4; wr(d, p, b.length); p += 4; System.arraycopy(b, 0, d, p, b.length); p += b.length; }
        jvmhost.Host.rmsWrite(name, d, total); version++;
    }
    private void chk() throws RecordStoreNotOpenException { if (!open) throw new RecordStoreNotOpenException(); }
    private int idx(int id) throws InvalidRecordIDException { for (int i = 0; i < recs.size(); i++) if (((Integer) ((Object[]) recs.elementAt(i))[0]).intValue() == id) return i; throw new InvalidRecordIDException(); }
    public void closeRecordStore() throws RecordStoreException { chk(); open = false; }
    public String getName() { return name; }
    public int getNumRecords() throws RecordStoreNotOpenException { chk(); return recs.size(); }
    public int getSize() throws RecordStoreNotOpenException { chk(); return jvmhost.Host.rmsSize(name); }
    public int getSizeAvailable() throws RecordStoreNotOpenException { return 65536; }
    public int getNextRecordID() throws RecordStoreException { chk(); return nextId; }
    public int getVersion() throws RecordStoreNotOpenException { return version; }
    public long getLastModified() throws RecordStoreNotOpenException { return 0; }
    public int addRecord(byte[] d, int off, int len) throws RecordStoreException {
        chk(); byte[] b = new byte[len]; if (len > 0) System.arraycopy(d, off, b, 0, len);
        int id = nextId++; recs.addElement(new Object[] { new Integer(id), b }); flush(); return id;
    }
    public void deleteRecord(int id) throws RecordStoreException { chk(); recs.removeElementAt(idx(id)); flush(); }
    public int getRecordSize(int id) throws RecordStoreException { chk(); return ((byte[]) ((Object[]) recs.elementAt(idx(id)))[1]).length; }
    public byte[] getRecord(int id) throws RecordStoreException {
        chk(); byte[] b = (byte[]) ((Object[]) recs.elementAt(idx(id)))[1]; byte[] r = new byte[b.length]; System.arraycopy(b, 0, r, 0, b.length); return r;
    }
    public int getRecord(int id, byte[] buf, int off) throws RecordStoreException { byte[] b = getRecord(id); System.arraycopy(b, 0, buf, off, b.length); return b.length; }
    public void setRecord(int id, byte[] d, int off, int len) throws RecordStoreException {
        chk(); int i = idx(id); byte[] b = new byte[len]; if (len > 0) System.arraycopy(d, off, b, 0, len);
        ((Object[]) recs.elementAt(i))[1] = b; flush();
    }
    public RecordEnumeration enumerateRecords(RecordFilter f, RecordComparator c, boolean keep) throws RecordStoreNotOpenException {
        chk(); Vector ids = new Vector();
        for (int i = 0; i < recs.size(); i++) { Object[] r = (Object[]) recs.elementAt(i); if (f == null || f.matches((byte[]) r[1])) ids.addElement(r[0]); }
        if (c != null) {
            for (int i = 1; i < ids.size(); i++) for (int j = i; j > 0; j--) {
                try { if (c.compare(getRecord(((Integer) ids.elementAt(j - 1)).intValue()), getRecord(((Integer) ids.elementAt(j)).intValue())) == RecordComparator.FOLLOWS) { Object t = ids.elementAt(j); ids.setElementAt(ids.elementAt(j - 1), j); ids.setElementAt(t, j - 1); } else break; } catch (RecordStoreException e) { break; }
            }
        }
        return new Enum(this, ids);
    }
    static class Enum implements RecordEnumeration {
        RecordStore rs; Vector ids; int pos;
        Enum(RecordStore r, Vector v) { rs = r; ids = v; }
        public int numRecords() { return ids.size(); }
        public boolean hasNextElement() { return pos < ids.size(); }
        public boolean hasPreviousElement() { return pos > 0; }
        public byte[] nextRecord() throws RecordStoreException { return rs.getRecord(nextRecordId()); }
        public byte[] previousRecord() throws RecordStoreException { return rs.getRecord(previousRecordId()); }
        public int nextRecordId() throws InvalidRecordIDException { if (pos >= ids.size()) throw new InvalidRecordIDException(); return ((Integer) ids.elementAt(pos++)).intValue(); }
        public int previousRecordId() throws InvalidRecordIDException { if (pos <= 0) throw new InvalidRecordIDException(); return ((Integer) ids.elementAt(--pos)).intValue(); }
        public void reset() { pos = 0; }
        public void rebuild() {}
        public void keepUpdated(boolean b) {}
        public boolean isKeptUpdated() { return false; }
        public void destroy() {}
    }
}
