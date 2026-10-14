import lldb, struct
n = {"i": 0, "reg": 0}
def rd(proc, addr, size):
    err = lldb.SBError(); return proc.ReadMemory(addr, size, err)
def hit(frame, bp_loc, internal_dict):
    name = (frame.GetFunctionName() or "?").split("(")[0].replace("llvm::orc::", "").replace("llvm::jitlink::", "")
    n["i"] += 1
    proc = frame.GetThread().GetProcess()
    r = lambda k: frame.FindRegister(k).GetValueAsUnsigned()
    extra = ""
    if "registerEHFrames" in name:
        start, end = r("x1"), r("x2")           # ExecutorAddrRange passed in registers
        extra = " range=[%#x,%#x) size=%d" % (start, end, end - start)
        size = end - start
        if 0 < size < 1_000_000:
            data = rd(proc, start, size)
            # count records: length-prefixed, CIE id 0 = CIE
            off = 0; cies = fdes = 0
            while off + 8 <= len(data):
                ln = struct.unpack_from("<I", data, off)[0]
                if ln == 0: break
                cid = struct.unpack_from("<I", data, off + 4)[0]
                if cid == 0: cies += 1
                else: fdes += 1
                off += 4 + ln
            extra += " records: %d CIE, %d FDE" % (cies, fdes)
    if name == "__register_frame":
        n["reg"] += 1; extra = " fde=%#x (call #%d)" % (r("x0"), n["reg"])
    print("[trace] %02d %s%s" % (n["i"], name, extra))
    return False
