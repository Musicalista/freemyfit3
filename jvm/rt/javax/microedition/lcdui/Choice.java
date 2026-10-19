package javax.microedition.lcdui;
public interface Choice {
    int EXCLUSIVE = 1, MULTIPLE = 2, IMPLICIT = 3, POPUP = 4, TEXT_WRAP_DEFAULT = 0, TEXT_WRAP_ON = 1, TEXT_WRAP_OFF = 2;
    int size(); String getString(int i); Image getImage(int i); int append(String s, Image im); void insert(int i, String s, Image im); void delete(int i); void deleteAll(); void set(int i, String s, Image im);
    boolean isSelected(int i); int getSelectedIndex(); void setSelectedIndex(int i, boolean sel); int getSelectedFlags(boolean[] a); void setSelectedFlags(boolean[] a);
    void setFitPolicy(int p); int getFitPolicy(); void setFont(int i, Font f); Font getFont(int i);
}
