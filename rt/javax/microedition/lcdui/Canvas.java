package javax.microedition.lcdui;
public abstract class Canvas extends Displayable {
    public static final int UP = 1, LEFT = 2, RIGHT = 5, DOWN = 6, FIRE = 8, GAME_A = 9, GAME_B = 10, GAME_C = 11, GAME_D = 12;
    public static final int KEY_NUM0 = 48, KEY_NUM1 = 49, KEY_NUM2 = 50, KEY_NUM3 = 51, KEY_NUM4 = 52, KEY_NUM5 = 53, KEY_NUM6 = 54, KEY_NUM7 = 55, KEY_NUM8 = 56, KEY_NUM9 = 57, KEY_STAR = 42, KEY_POUND = 35;
    private boolean dirty = true;
    protected Canvas() {}
    public int getWidth() { return jvmhost.Host.width(); }
    public int getHeight() { return jvmhost.Host.height(); }
    public boolean isDoubleBuffered() { return true; }
    public final void repaint() { dirty = true; }
    public final void repaint(int x, int y, int w, int h) { dirty = true; }
    public final void serviceRepaints() {
        if (dirty) { dirty = false; paint(new Graphics()); jvmhost.Host.present(); }
    }
    protected abstract void paint(Graphics g);
    protected void keyPressed(int keyCode) {}
    protected void keyReleased(int keyCode) {}
    protected void keyRepeated(int keyCode) {}
    protected void showNotify() {}
    protected void hideNotify() {}
    public int getGameAction(int k) {
        switch (k) {
            case -1: case 50: return UP;
            case -2: case 56: return DOWN;
            case -3: case 52: return LEFT;
            case -4: case 54: return RIGHT;
            case -5: case 53: return FIRE;
            case 49: return GAME_A; case 51: return GAME_B; case 55: return GAME_C; case 57: return GAME_D;
            default: return 0;
        }
    }
    public int getKeyCode(int a) {
        switch (a) { case UP: return -1; case DOWN: return -2; case LEFT: return -3; case RIGHT: return -4; case FIRE: return -5; default: return 0; }
    }
    public final void _key(int code, boolean down) { if (down) keyPressed(code); else keyReleased(code); }
}
