package com.mascotcapsule.micro3d.v3;
public class FigureLayout {
    public AffineTrans at; public int cx, cy, near, far, angle;
    public FigureLayout() {}
    public void setAffineTrans(AffineTrans a) { at = a; }
    public void setCenter(int x, int y) { cx = x; cy = y; }
    public void setPerspective(int near, int far, int angle) { this.near = near; this.far = far; this.angle = angle; }
}
