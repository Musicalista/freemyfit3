package javax.microedition.media;
public class Manager {
    public static final String TONE_DEVICE_LOCATOR = "device://tone";
    public static Player createPlayer(java.io.InputStream in, String type) throws java.io.IOException, MediaException { return new Silent(); }
    public static Player createPlayer(String loc) throws java.io.IOException, MediaException { return new Silent(); }
    public static void playTone(int note, int ms, int vol) throws MediaException {}
    static class Silent implements Player, javax.microedition.media.control.VolumeControl {
        public void setMute(boolean m) {} public boolean isMuted() { return false; } public int setLevel(int l) { return l; } public int getLevel() { return 50; }
        int st = UNREALIZED;
        public void realize() { st = REALIZED; } public void prefetch() { st = PREFETCHED; } public void start() { st = STARTED; } public void stop() { st = PREFETCHED; }
        public void deallocate() { st = REALIZED; } public void close() { st = CLOSED; } public int getState() { return st; } public void setLoopCount(int n) {}
        public void addPlayerListener(PlayerListener l) {} public void removePlayerListener(PlayerListener l) {} public long getDuration() { return TIME_UNKNOWN; }
        public long setMediaTime(long t) { return t; } public long getMediaTime() { return 0; } public Control getControl(String t) { return t != null && t.endsWith("VolumeControl") ? this : null; } public Control[] getControls() { return new Control[0]; }
    }
}
