import lldb, os, struct
n = {"i": 0}
def hit(frame, bp_loc, internal_dict):
    n["i"] += 1
    proc = frame.GetThread().GetProcess(); err = lldb.SBError()
    start = frame.FindRegister("x1").GetValueAsUnsigned(); end = frame.FindRegister("x2").GetValueAsUnsigned()
    size = end - start
    data = proc.ReadMemory(start, size, err)
    print("[raw] registerEHFrames #%d range=[%#x,%#x) size=%d" % (n["i"], start, end, size))
    # walk records
    off = 0; k = 0
    while off + 8 <= len(data) and k < 400:
        ln = struct.unpack_from("<I", data, off)[0]
        cid = struct.unpack_from("<I", data, off + 4)[0] if off + 8 <= len(data) else -1
        if k < 6 or ln == 0:
            print("[raw]   rec %d @+%#x len=%d cie_id=%#x" % (k, off, ln, cid))
        if ln == 0: break
        off += 4 + ln; k += 1
    print("[raw]   walked %d record(s), stopped at +%#x of %d%s" % (k, off, len(data), "  (terminator reached)" if off+4<=len(data) and struct.unpack_from('<I',data,off)[0]==0 else ""))
    open("%s/ehframe_%s_%d.bin" % (os.environ.get("EH_OUT", "."), n.get("tag", "x"), n["i"]), "wb").write(data)
    return False
