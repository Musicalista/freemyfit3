"""patch_lang.py: relabel the Settings "tutorials" entry as "Apps extras" in selected language packs (same byte length!).
Each component is a ZIP (language_xx.res) holding one deflated file with a NUL-separated string table.
Writes patched component files to ./lang/<index>.bin (to be passed to repack.py as index=file)."""
import zipfile, io, os, sys
S = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "work", "out") + os.sep
H = os.path.dirname(os.path.abspath(__file__)); os.makedirs(os.path.join(H, 'lang'), exist_ok=True)
# component index -> (file, old text, new text)  (new text padded with spaces to the old length)
JOBS = {
    68: ('68_k0_nand__system__font__language_pt_rBR.res', 'Dicas e tutoriais', 'Apps extras'),
    52: ('52_k0_nand__system__font__language_pt_rPT.res', None, 'Apps extras'),
    30: ('30_k0_nand__system__font__language_en.res', 'Tips and tutorials', 'Extra apps'),
    13: ('13_k0_nand__system__font__language_en_rUS.res', 'Tips and tutorials', 'Extra apps'),
}
out = []
for idx, (fn, old, new) in JOBS.items():
    zin = zipfile.ZipFile(S + fn); name = zin.namelist()[0]; data = bytearray(zin.read(name))
    if old is None:                                    # find the pt_PT wording from the table
        for cand in ('Dicas e tutoriais', 'Sugest\u00f5es e tutoriais', 'Dicas e tutoriais'):
            if cand.encode() in data: old = cand; break
    if old is None: print(idx, 'pattern not found, skipped'); continue
    ob = old.encode('utf-8'); nb = new.encode('utf-8').ljust(len(ob), b' ')
    assert len(nb) == len(ob), (old, new)
    needle = b'\0' + ob + b'\0'; n = data.count(needle)
    if not n: print(idx, 'needle not found'); continue
    data = bytes(data).replace(needle, b'\0' + nb + b'\0')
    bio = io.BytesIO()
    with zipfile.ZipFile(bio, 'w', zipfile.ZIP_DEFLATED) as z:
        zi = zipfile.ZipInfo(name, zin.getinfo(name).date_time); zi.compress_type = zipfile.ZIP_DEFLATED; zi.external_attr = zin.getinfo(name).external_attr; z.writestr(zi, data)
    open(os.path.join(H, 'lang', '%d.bin' % idx), 'wb').write(bio.getvalue()); out.append(idx)
    print(idx, fn.split('language_')[1], '%d occurrence(s): %r -> %r' % (n, old, nb.decode()))
print('patched components:', out)
