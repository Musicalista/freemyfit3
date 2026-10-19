# usage: mkjar.py out.jar classes_dir midlet_class   (packs compiled classes + a manifest into a MIDlet jar)
import sys, zipfile, os, glob
out, d, main = sys.argv[1:4]
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\r\nMIDlet-1: Test, , %s\r\nMicroEdition-Configuration: CLDC-1.1\r\nMicroEdition-Profile: MIDP-2.0\r\n' % main)
    for p in glob.glob(os.path.join(d, '**', '*'), recursive=True):
        if os.path.isfile(p):
            z.write(p, os.path.relpath(p, d).replace(os.sep, '/'))
