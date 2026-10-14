import lldb, os
OUT = os.environ.get("OBJ_OUT", ".")
state = {"n": 0}
def on_bp(frame, bp_loc, internal_dict):
    # ObjectLinkingLayer::emit(this, unique_ptr<MaterializationResponsibility> R, unique_ptr<MemoryBuffer> O)
    # A unique_ptr passed by value is passed by invisible reference: x2 points at the unique_ptr.
    proc = frame.GetThread().GetProcess()
    err = lldb.SBError()
    up = frame.FindRegister("x2").GetValueAsUnsigned()
    buf = proc.ReadPointerFromMemory(up, err)                     # MemoryBuffer*
    start = proc.ReadPointerFromMemory(buf + 8, err)              # BufferStart
    end = proc.ReadPointerFromMemory(buf + 16, err)               # BufferEnd
    size = end - start
    if err.Fail() or size <= 0 or size > 50_000_000:
        print("[dump] bad buffer: %s size=%d" % (err, size)); return False
    data = proc.ReadMemory(start, size, err)
    path = "%s/obj_%02d.o" % (OUT, state["n"])
    open(path, "wb").write(data)
    print("[dump] object %d: %d bytes -> %s" % (state["n"], size, path))
    state["n"] += 1
    return False
