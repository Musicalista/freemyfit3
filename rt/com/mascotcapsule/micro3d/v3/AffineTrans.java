package com.mascotcapsule.micro3d.v3;
public class AffineTrans {
    public int m00 = 4096, m01, m02, m03, m10, m11 = 4096, m12, m13, m20, m21, m22 = 4096, m23;
    public AffineTrans() {}
    private static int r(double v) { return (int) Math.floor(v * 4096.0 + 0.5); }
    /* world->view: x right, y down, z forward (right-handed); look is a direction vector */
    public void lookAt(Vector3D pos, Vector3D look, Vector3D up) {
        double zx = look.x, zy = look.y, zz = look.z, l = Math.sqrt(zx * zx + zy * zy + zz * zz);
        zx /= l; zy /= l; zz /= l;
        double ux = up.x, uy = up.y, uz = up.z, d = ux * zx + uy * zy + uz * zz;
        ux -= d * zx; uy -= d * zy; uz -= d * zz; l = Math.sqrt(ux * ux + uy * uy + uz * uz);
        ux /= l; uy /= l; uz /= l;
        double yx = -ux, yy = -uy, yz = -uz;
        double xx = yy * zz - yz * zy, xy = yz * zx - yx * zz, xz = yx * zy - yy * zx;
        m00 = r(xx); m01 = r(xy); m02 = r(xz); m10 = r(yx); m11 = r(yy); m12 = r(yz); m20 = r(zx); m21 = r(zy); m22 = r(zz);
        m03 = (int) Math.floor(-(xx * pos.x + xy * pos.y + xz * pos.z) + 0.5);
        m13 = (int) Math.floor(-(yx * pos.x + yy * pos.y + yz * pos.z) + 0.5);
        m23 = (int) Math.floor(-(zx * pos.x + zy * pos.y + zz * pos.z) + 0.5);
    }
}
