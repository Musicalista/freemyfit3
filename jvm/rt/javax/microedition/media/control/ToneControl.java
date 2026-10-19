package javax.microedition.media.control;
public interface ToneControl extends javax.microedition.media.Control { byte VERSION = -2, TEMPO = -3, RESOLUTION = -4, BLOCK_START = -5, BLOCK_END = -6, PLAY_BLOCK = -7, SET_VOLUME = -8, REPEAT = -9, C4 = 60, SILENCE = -1; void setSequence(byte[] s); }
