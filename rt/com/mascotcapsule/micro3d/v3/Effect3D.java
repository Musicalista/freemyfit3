package com.mascotcapsule.micro3d.v3;
public class Effect3D { public Light light; public int shading; public boolean toon; public Texture tex; public Effect3D(Light l, int shading, boolean tc, Texture t) { light = l; this.shading = shading; toon = tc; tex = t; } }
