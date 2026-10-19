package javax.microedition.lcdui;
public class AlertType {
    public static final AlertType INFO = new AlertType(), WARNING = new AlertType(), ERROR = new AlertType(), ALARM = new AlertType(), CONFIRMATION = new AlertType();
    private AlertType() {}
    public boolean playSound(Display d) { return true; }
}
