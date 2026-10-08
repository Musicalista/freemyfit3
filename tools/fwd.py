import struct,sys,os,zipfile,io,hashlib,zlib
src,out=sys.argv[1],sys.argv[2]
os.makedirs(out,exist_ok=True)
b=open(src,'rb').read()
u32=lambda d,o:struct.unpack_from('<I',d,o)[0]
zs=lambda d:d.split(b'\0')[0].decode()
print('magic',hex(u32(b,0)),'source',zs(b[4:68]),'flags',u32(b,72),'hdr',u32(b,76),'updater',u32(b,80),'off',u32(b,84),'size',u32(b,88))
hdr,us,off,size=u32(b,76),u32(b,80),u32(b,84),u32(b,88)
open(out+'/updater.bin','wb').write(b[hdr:off-4])
p=b[off:off+size]
print('pkt magic',hex(u32(p,0)),'target',zs(p[4:68]),'fw',u32(p,72),'res',u32(p,76),'b69',hex(p[69]),'count',p[70])
for i in range(p[70]):
    o=80+i*128
    kind,attr,start,stored=struct.unpack_from('<HHII',p,o); path=zs(p[o+12:o+128])
    z=zipfile.ZipFile(io.BytesIO(p[start:start+stored])); n=z.namelist()[0]; raw=z.read(n)
    fn=out+'/%02d_k%d_'%(i,kind)+path.strip('/').replace('/','__')
    open(fn,'wb').write(raw)
    print(i,kind,hex(attr),path,'zipname=',n,len(raw),hashlib.sha256(raw).hexdigest()[:16])
