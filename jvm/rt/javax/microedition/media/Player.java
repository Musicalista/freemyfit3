package javax.microedition.media;
public interface Player extends Controllable {
    int CLOSED = 0, UNREALIZED = 100, REALIZED = 200, PREFETCHED = 300, STARTED = 400; long TIME_UNKNOWN = -1;
    void realize() throws MediaException; void prefetch() throws MediaException; void start() throws MediaException; void stop() throws MediaException;
    void deallocate(); void close(); int getState(); void setLoopCount(int n); void addPlayerListener(PlayerListener l); void removePlayerListener(PlayerListener l);
    long getDuration(); long setMediaTime(long t) throws MediaException; long getMediaTime();
}
