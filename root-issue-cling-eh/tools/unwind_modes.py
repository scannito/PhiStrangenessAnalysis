import re, struct, subprocess, sys
from collections import Counter
def sect(path, name):
    out = subprocess.run(["otool","-l",path],capture_output=True,text=True).stdout
    blocks = out.split("Section\n")
    for b in blocks:
        if re.search(r"sectname %s\n" % re.escape(name), b):
            size = int(re.search(r"size 0x([0-9a-f]+)", b).group(1),16); off = int(re.search(r"offset (\d+)", b).group(1))
            return off, size
    return None
def rows(path):
    r = sect(path, "__compact_unwind")
    data = open(path,"rb").read()[r[0]:r[0]+r[1]]
    for i in range(0, len(data), 32):
        fn, ln, enc, pers, lsda = struct.unpack("<QIIQQ", data[i:i+32])
        yield fn, ln, enc, pers, lsda
MODE = {0x02000000:"FRAMELESS", 0x03000000:"DWARF", 0x04000000:"FRAME", 0x00000000:"0 (none)"}
for path in sys.argv[1:]:
    rs = list(rows(path))
    c = Counter(MODE.get(e & 0x0f000000, hex(e & 0x0f000000)) for _,_,e,_,_ in rs)
    # the object is not relocated: personality/lsda hold addends (0) and the relocations carry the targets,
    # so count rows that have a relocation-carried LSDA/personality through the function size instead
    print("%-22s rows=%-3d modes=%s" % (path, len(rs), dict(c)))
