package javax.microedition.rms;
public interface RecordEnumeration {
    int numRecords(); boolean hasNextElement(); boolean hasPreviousElement();
    byte[] nextRecord() throws RecordStoreException; byte[] previousRecord() throws RecordStoreException;
    int nextRecordId() throws InvalidRecordIDException; int previousRecordId() throws InvalidRecordIDException;
    void reset(); void rebuild(); void keepUpdated(boolean b); boolean isKeptUpdated(); void destroy();
}
