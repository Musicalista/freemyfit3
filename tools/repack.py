"""repack.py <orig.bin> <out.bin> [index=file ...]  -- rebuild an FWD package, replacing components by index."""
import struct,sys,zlib,zipfile,io
P=lambda *a:struct.pack(*a)
u32=lambda d,o:struct.unpack_from('<I',d,o)[0]
def crc(b):return zlib.crc32(b)&0xffffffff
def rezip(name,raw):
    bio=io.BytesIO()
    with zipfile.ZipFile(bio,'w',zipfile.ZIP_DEFLATED) as z:
        zi=zipfile.ZipInfo(name,(1980,1,1,0,0,0)); zi.compress_type=zipfile.ZIP_DEFLATED; z.writestr(zi,raw)
    return bio.getvalue()
def fix_main(raw,size=None):
    """recompute main-image header: size @0x100c, crc32 @0x1008 over image with [0:4]=FF, [0x1000:0x1010]=0"""
    b=bytearray(raw); size=size or u32(raw,0x100c)
    b[0x100c:0x1010]=P('<I',size)
    n=bytearray(b[:size]); n[0:4]=b'\xff'*4; n[0x1000:0x1010]=b'\0'*16
    b[0x1008:0x100c]=P('<I',crc(bytes(n)))
    return bytes(b)
def repack(src,repl):
    b=open(src,'rb').read()
    hdr,us,off,size=u32(b,76),u32(b,80),u32(b,84),u32(b,88)
    updater=b[hdr:off-4]; p=b[off:off+size]; cnt=p[70]; tableEnd=80+cnt*128
    table=bytearray(p[:tableEnd]); comps=[]; fw=res=0
    for i in range(cnt):
        o=80+i*128; kind=struct.unpack_from('<H',p,o)[0]
        start,stored=u32(p,o+4),u32(p,o+8)
        zb=p[start:start+stored]
        if i in repl:
            name=zipfile.ZipFile(io.BytesIO(zb)).namelist()[0]
            raw=repl[i]; raw=fix_main(raw) if kind==1 else raw
            zb=rezip(name,raw)
        else: raw=zipfile.ZipFile(io.BytesIO(zb)).read(zipfile.ZipFile(io.BytesIO(zb)).namelist()[0])
        comps.append((zb,crc(raw)))
        if kind: fw+=len(raw)
        else: res+=len(raw)
    cur=tableEnd+4
    for i,(zb,_) in enumerate(comps):
        struct.pack_into('<II',table,80+i*128+4,cur,len(zb)); cur+=len(zb)+4
    struct.pack_into('<II',table,72,fw,res)
    body=bytes(table)+P('<I',crc(bytes(table)))
    for zb,c in comps: body+=zb+P('<I',c)
    out=bytearray(b[:92]); 
    newoff=92+len(updater)+4
    struct.pack_into('<II',out,84,newoff,len(body))
    return bytes(out)+updater+P('<I',crc(updater))+body+P('<I',crc(body))
if __name__=='__main__':
    repl={int(a.split('=')[0]):open(a.split('=',1)[1],'rb').read() for a in sys.argv[3:]}
    open(sys.argv[2],'wb').write(repack(sys.argv[1],repl))
