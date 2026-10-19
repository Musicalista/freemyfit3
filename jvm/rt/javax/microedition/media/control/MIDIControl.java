package javax.microedition.media.control;
public interface MIDIControl extends javax.microedition.media.Control { void shortMidiEvent(int t, int a, int b); int setVolume(int c, int v); int getVolume(int c); }
