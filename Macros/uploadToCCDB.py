#!/usr/bin/env python3
"""Uploads the efficiency maps made by mc_task to the CCDB, as a YAML file declares, each with metadata saying where
it comes from: the MC train it was made from, the framework commit, when and how it was made.

Dry run by default: it checks everything, then prints what it would upload and every object the CCDB holds at each
path now. Nothing is sent without --upload.

usage, in the O2 environment (enterO2P):
  export DYLD_LIBRARY_PATH=$ROOTSYS/lib     # PyROOT on macOS, see below
  python3 Macros/uploadToCCDB.py HYConfigs/CCDB/h2EffMapsUpload.yaml [--upload]

Run it as `python3 script`, not as `./script`: on macOS PyROOT finds the ROOT libraries only through
DYLD_LIBRARY_PATH (its Python libraries carry no rpath in ROOT 6.36), which macOS strips through /usr/bin/env.

The YAML file:
  ccdb_host: https://alice-ccdb.cern.ch     # required: o2-ccdb-upload's own default is a test server
  maps_dir: /path/to/Corrections/MoreFullDifferential   # optional: where relative sources are
  particles: [Phi, K0S, Xi, Omega, Pi]      # required: the particles of every target that does not list its own
  run_ranges:                               # optional: name: [first run, last run], for run-range validities
    LowIRad: [570609, 570703]
  targets:
    - ccdb_efficiency_path: Users/s/scannito/Efficiencies/pp26_lowIR_updated/h2EffMap
      maps:
        - source: 2026_LowIR_updated_       # mc_task's output_prefix, under maps_dir: + h2EffMap<Particle>.root
          validity: full                    # or the name of a run range
A target is one ccdb_efficiency_path, the string set in the phiStrangeCorrelation wagon: the task reads the path plus
the particle name. Its maps are either one with full validity, read with or without perPeriodEfficiency, or several
with run ranges, read only with perPeriodEfficiency (getForRun). A target is uploaded whole: every map, every
particle.
"""
import argparse
import datetime
import hashlib
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
import uuid

try:
    import yaml
except ImportError:
    yaml = None

# Validity of a map for every run: from 1 ms after the epoch to 1 January 2050, 00:00 CET
FULL_VALIDITY = (1, 2524604400000)
# Added before the start of the first run of a range and after the end of its last, in ms (the owner's choice).
# getForRun looks a run's map up at the middle of the run, so the margin does not change what the task reads.
RUN_MARGIN_MS = 300
# Run start and end times. Always production, even when uploading to a test server, which has no real runs.
RCT_URL = "http://alice-ccdb.cern.ch/RCT/Info/RunInformation"
# Metadata values travel in o2-ccdb-upload's -m "k1=v1;k2=v2" and in the upload URL: an '=' or ';' in a value is
# dropped or split there with no message (UploadTool.cxx), and long values make long URLs.
MAX_METADATA_LENGTH = 200

# Second copies of rules of the framework, each from where it is written. A rename there breaks this script:
MAP_KIND = "h2EffMap"
MAP_FILE = "{}" + MAP_KIND + "{}.root"      # outputDir/<binning_name>/<output_prefix>h2EffMap<Particle>.root, MCTask
CCDB_OBJECT = "ccdb_object"                 # RootIO::WriteCcdbFile (Codes/RootIOHelpers.h)
PROVENANCE_DIR = "Provenance"               # RootIO::Detail::kProvenanceDir, written next to it by WriteCcdbFile
PROVENANCE_KEYS = ("config_block", "git_commit", "produced_at", "sources_fingerprint")  # RootIO::StartProvenance,
                                                                                         # RunEnvironment::Facts
CORRECTION_MODE_KEY = "particle_correction_mode"    # required by MCTask::Init, in config_block
TRAIN_CONFIG_KEY = "train_config_mc_{}"     # TrainConfig::ProvenanceKey("mc_<particle>") in MCTask::Init
NO_TRAIN_CONFIG = "none"                    # TrainConfig::Describe without a record; with one, a list of
                                            # {"hyperloop": {"dataset", "train_id", ...}, "wagon", ...}

TOP_KEYS = {"ccdb_host", "maps_dir", "particles", "run_ranges", "targets"}
TARGET_KEYS = {"ccdb_efficiency_path", "particles", "maps"}
MAP_KEYS = {"source", "validity"}


def parse_arguments():
    parser = argparse.ArgumentParser(description="Upload the efficiency maps of mc_task to the CCDB, as a YAML file "
                                                 "declares. Dry run unless --upload.")
    parser.add_argument("config", help="the YAML file of the uploads")
    parser.add_argument("--upload", action="store_true", help="upload, after every check has passed")
    return parser.parse_args()


def utc(ms):
    return datetime.datetime.fromtimestamp(ms / 1000, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def particle_list(value):
    return isinstance(value, list) and value and all(isinstance(p, str) for p in value) and len(set(value)) == len(value)


# --- the YAML -------------------------------------------------------------------------------------------------------

def config_errors(cfg):
    """What is wrong with the YAML itself, one line each, before any file or the network is touched. Overlapping
    validities are found later, on the run times."""
    if not isinstance(cfg, dict):
        return ["the file is not a mapping"]
    errors = []
    if set(cfg) - TOP_KEYS:
        errors.append(f"unknown keys {sorted(set(cfg) - TOP_KEYS)}")
    host = cfg.get("ccdb_host")
    if not isinstance(host, str) or urllib.parse.urlparse(host).scheme not in ("http", "https"):
        errors.append("'ccdb_host' is required, as an http(s) URL")
    if not particle_list(cfg.get("particles")):
        errors.append("'particles' is required, a list of distinct names")
    if not isinstance(cfg.get("maps_dir", ""), str):
        errors.append("'maps_dir' is a directory")
    ranges = cfg.get("run_ranges", {})
    if not isinstance(ranges, dict):
        errors.append("'run_ranges' is a mapping name: [first run, last run]")
        ranges = {}
    for name, runs in ranges.items():
        if name == "full":
            errors.append("run range 'full': the name is taken by the full validity")
        if not (isinstance(runs, list) and len(runs) == 2 and all(isinstance(r, int) for r in runs) and runs[0] <= runs[1]):
            errors.append(f"run range '{name}': [first run, last run], two run numbers, first <= last")
    targets = cfg.get("targets")
    if not isinstance(targets, list) or not targets:
        return errors + ["'targets' is required, a non-empty list"]

    paths = []
    for t_index, target in enumerate(targets, start=1):
        where = f"target #{t_index}"
        if not isinstance(target, dict):
            errors.append(f"{where}: not a mapping")
            continue
        # particles is optional here, so a misspelled key would read as "not given": every key is checked
        if set(target) - TARGET_KEYS:
            errors.append(f"{where}: unknown keys {sorted(set(target) - TARGET_KEYS)}")
        path = target.get("ccdb_efficiency_path")
        if not isinstance(path, str) or not path or path.startswith("/"):
            errors.append(f"{where}: 'ccdb_efficiency_path' is required, a CCDB path without a leading '/'")
        paths.append(path)
        if "particles" in target and not particle_list(target["particles"]):
            errors.append(f"{where}: 'particles' is a list of distinct names")
        maps = target.get("maps")
        if not isinstance(maps, list) or not maps:
            errors.append(f"{where}: 'maps' is required, a non-empty list")
            continue
        for m_index, entry in enumerate(maps, start=1):
            if not isinstance(entry, dict) or set(entry) != MAP_KEYS:
                errors.append(f"{where}, map #{m_index}: exactly the keys {sorted(MAP_KEYS)}")
            elif entry["validity"] != "full" and entry["validity"] not in ranges:
                errors.append(f"{where}, map #{m_index}: validity '{entry['validity']}' is neither 'full' nor a run range")
    for path in {p for p in paths if p and paths.count(p) > 1}:
        errors.append(f"the path '{path}' is the target of two entries")
    return errors


# --- the map files ---------------------------------------------------------------------------------------------------

def import_root():
    try:
        import ROOT  # pylint: disable=import-error,import-outside-toplevel
    except ImportError as error:
        sys.exit(f"Error: PyROOT cannot be imported ({error}). In the O2 environment, run "
                 f"'export DYLD_LIBRARY_PATH=$ROOTSYS/lib' and start the script with python3.")
    ROOT.gROOT.SetBatch(True)
    return ROOT


def content_hash(hist):
    """A hash of what the map holds: class, binning, every bin content and error, flows included. The one link from
    an object in the CCDB back to a local file that depends on neither names nor times."""
    digest = hashlib.sha256(hist.ClassName().encode())
    for axis in (hist.GetXaxis(), hist.GetYaxis()):
        edges = [axis.GetBinLowEdge(b) for b in range(1, axis.GetNbins() + 2)]
        digest.update(struct.pack(f"<{len(edges)}d", *edges))
    for b in range(hist.GetNcells()):
        digest.update(struct.pack("<2d", hist.GetBinContent(b), hist.GetBinError(b)))
    return digest.hexdigest()[:16]


def read_map(ROOT, path):
    """What the upload needs from one map file, or the reason it cannot be uploaded."""
    if not os.path.isfile(path):
        return None, "not on disk"
    f = ROOT.TFile.Open(path)
    if not f or f.IsZombie():
        return None, "cannot be opened"
    try:
        names = {k.GetName() for k in f.GetListOfKeys()}
        if PROVENANCE_DIR not in names:
            return None, (f"has no '{PROVENANCE_DIR}': written before the provenance was stored with the maps, run its "
                          f"mc_task block again")
        if names != {CCDB_OBJECT, PROVENANCE_DIR}:
            return None, f"holds {sorted(names)}, not only '{CCDB_OBJECT}' and its '{PROVENANCE_DIR}'"
        hist = f.Get(CCDB_OBJECT)
        if not hist.InheritsFrom("TH2"):
            return None, f"'{CCDB_OBJECT}' is a {hist.ClassName()}, not a TH2"
        nonfinite = sum(not math.isfinite(hist.GetBinContent(b)) for b in range(hist.GetNcells()))
        if nonfinite:
            return None, f"{nonfinite} bins are not finite"
        provenance_dir = f.Get(PROVENANCE_DIR)
        provenance = {k.GetName(): provenance_dir.Get(k.GetName()).GetTitle() for k in provenance_dir.GetListOfKeys()}
        nx, ny = hist.GetNbinsX(), hist.GetNbinsY()
        empty = sum(hist.GetBinContent(hist.GetBin(x, y)) == 0 for x in range(1, nx + 1) for y in range(1, ny + 1))
        return {"class": hist.ClassName(), "bins": f"{nx} x {ny}", "empty": empty, "hash": content_hash(hist),
                "provenance": provenance}, None
    finally:
        f.Close()


def metadata_of(info, particle, source, validity, runs, upload_id):
    """The metadata of one object, or the reason there are none. Short values only: the full records stay in the map
    file, which these lead back to. A value that is missing or empty refuses the map: the upload exists to make it
    traceable."""
    provenance = info["provenance"]
    train_key = TRAIN_CONFIG_KEY.format(particle)
    missing = [k for k in PROVENANCE_KEYS + (train_key,) if not provenance.get(k)]
    if missing:
        return None, f"its provenance has no {missing}"
    if provenance[train_key] == NO_TRAIN_CONFIG:
        return None, ("its MC input had no train record when the map was made: download it "
                      "(Macros/downloadHYOutput.py) and run the mc_task block again")
    block = json.loads(provenance["config_block"])
    devices = json.loads(provenance[train_key])

    def joined(values):
        values = list(values)
        return "" if any(v in (None, "") for v in values) else ",".join(dict.fromkeys(str(v) for v in values))

    binning_dir, prefix = os.path.split(source)
    meta = {
        "particle": particle,
        "mapKind": MAP_KIND,
        "contentHash": info["hash"],
        "producedAt": provenance["produced_at"],
        "gitCommit": provenance["git_commit"],
        "sourcesFingerprint": provenance["sources_fingerprint"],
        "correctionMode": str(block.get(CORRECTION_MODE_KEY, "")),
        "outputPrefix": prefix,
        "binningName": os.path.basename(binning_dir),
        "mcDataset": joined(d.get("hyperloop", {}).get("dataset") for d in devices),
        "mcTrainIds": joined(d.get("hyperloop", {}).get("train_id") for d in devices),
        "mcWagonIds": joined(d.get("wagon") for d in devices),
        "validity": validity,
        "uploadId": upload_id,
    }
    if runs:
        meta["firstRun"], meta["lastRun"] = str(runs[0]), str(runs[1])
    empty = [k for k, v in meta.items() if not v]
    if empty:
        return None, f"no value for the metadata {empty}"
    bad = [k for k, v in meta.items() if "=" in v or ";" in v or len(v) > MAX_METADATA_LENGTH]
    if bad:
        return None, f"metadata {bad} contain '=' or ';' or are longer than {MAX_METADATA_LENGTH}"
    return meta, None


# --- the CCDB, over HTTP ---------------------------------------------------------------------------------------------

class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None  # the CCDB answers a lookup with a redirect whose headers are the answer


def ccdb_headers(url):
    """The headers of the object a CCDB lookup URL (path/timestamp[/key=value...]) resolves to, looked up without
    regard to case as HTTP wants; None when there is none. Reads are anonymous over http."""
    opener = urllib.request.build_opener(_NoRedirect)
    try:
        return opener.open(urllib.request.Request(url, method="HEAD"), timeout=30).headers
    except urllib.error.HTTPError as error:
        if error.code in (301, 302, 303, 307, 308):
            return error.headers
        if error.code == 404:
            return None
        raise


def read_base(host):
    """The upload host, for reading: same server and port, over http (https asks for a certificate)."""
    return urllib.parse.urlparse(host)._replace(scheme="http").geturl().rstrip("/")


def lookup_url(host, obj):
    """Where the object is looked up: the middle of its validity, as getForRun looks a run's map up."""
    return f"{read_base(host)}/{obj['path']}/{obj['start'] // 2 + obj['end'] // 2}"


def objects_at(host, path):
    """Every object the CCDB holds at a path, newest first."""
    request = urllib.request.Request(f"{read_base(host)}/browse/{path}", headers={"Accept": "application/json"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return sorted(json.load(response).get("objects", []), key=lambda o: -o.get("createTime", 0))


def run_window(first, last):
    """From the start of the first run to the end of the last, as RCT records them, with the margin."""
    times = []
    for run, key in ((first, "SOR"), (last, "EOR")):
        headers = ccdb_headers(f"{RCT_URL}/{run}")
        if not headers or headers.get(key) is None:
            raise LookupError(f"run {run} has no {key} in RCT")
        times.append(int(headers[key]))
    if times[0] >= times[1]:
        raise LookupError(f"runs {first}..{last}: start {times[0]} is not before end {times[1]}")
    return times[0] - RUN_MARGIN_MS, times[1] + RUN_MARGIN_MS


# --- plan, show, upload ---------------------------------------------------------------------------------------------

def plan(cfg, upload_id):
    """Every object to upload, or every reason why not."""
    ROOT = import_root()
    ranges = cfg.get("run_ranges", {})
    maps_dir = cfg.get("maps_dir", "")
    errors = []
    windows = {"full": FULL_VALIDITY}
    for name in {e["validity"] for t in cfg["targets"] for e in t["maps"]} - {"full"}:
        try:
            windows[name] = run_window(*ranges[name])
        except LookupError as error:
            errors.append(f"run range '{name}': {error}")

    objects = []
    for target in cfg["targets"]:
        path = target["ccdb_efficiency_path"]
        # Two maps of one target must not be valid at the same time: getForRun would return whichever was created
        # last, with no error. The full validity overlaps everything, a run range listed twice overlaps itself.
        spans = sorted(windows[e["validity"]] + (e["validity"],) for e in target["maps"] if e["validity"] in windows)
        for (_, end1, name1), (start2, _, name2) in zip(spans, spans[1:]):
            if start2 < end1:
                errors.append(f"target '{path}': the validities '{name1}' and '{name2}' overlap")
        for entry in target["maps"]:
            validity = entry["validity"]
            if validity not in windows:
                continue
            source = os.path.join(maps_dir, entry["source"])
            for particle in target.get("particles", cfg["particles"]):
                local = MAP_FILE.format(source, particle)
                info, problem = read_map(ROOT, local)
                if not problem:
                    meta, problem = metadata_of(info, particle, source, validity, ranges.get(validity), upload_id)
                if problem:
                    errors.append(f"{local}: {problem}")
                    continue
                start, end = windows[validity]
                objects.append({"path": path + particle, "file": local, "start": start, "end": end,
                                "runs": ranges.get(validity), "info": info, "meta": meta})
    return objects, errors


def command(host, obj):
    meta = ";".join(f"{k}={v}" for k, v in obj["meta"].items())
    return ["o2-ccdb-upload", "--host", host, "--path", obj["path"], "--file", obj["file"], "--key", CCDB_OBJECT,
            "--starttimestamp", str(obj["start"]), "--endtimestamp", str(obj["end"]), "--meta", meta]


def show(host, obj):
    runs = f", runs {obj['runs'][0]}..{obj['runs'][1]}" if obj["runs"] else ""
    print(f"\n{host}/{obj['path']}")
    print(f"  file      {obj['file']}  ({obj['info']['class']}, {obj['info']['bins']} bins, "
          f"{obj['info']['empty']} empty)")
    print(f"  validity  {obj['start']} .. {obj['end']}  ({utc(obj['start'])} .. {utc(obj['end'])} UTC{runs})")
    for key, value in obj["meta"].items():
        print(f"  meta      {key} = {value}")
    print(f"  command   {' '.join(command(host, obj))}")
    existing = objects_at(host, obj["path"])
    if not existing:
        print("  now       nothing at this path")
    for o in existing:
        ours = ", ".join(f"{k}={o[k]}" for k in ("validity", "producedAt", "contentHash") if k in o)
        print(f"  now       {o.get('id')} created {utc(o.get('createTime', 0))} UTC, valid {o.get('validFrom')} .. "
              f"{o.get('validUntil')}; {ours or 'no metadata of this script'}")


def upload(host, obj):
    """Uploads one object and reads it back. o2-ccdb-upload exits with 0 whatever happens (a missing key, a refused
    request), so success is the object found at its path with every metadata value that was sent."""
    result = subprocess.run(command(host, obj), capture_output=True, text=True)
    url = f"{lookup_url(host, obj)}/uploadId={obj['meta']['uploadId']}"
    found = ccdb_headers(url)
    if found is None:
        raise RuntimeError(f"not found after the upload ({url}).\n  o2-ccdb-upload said: "
                           f"{(result.stdout + result.stderr).strip()[-500:]}")
    differing = [k for k, v in obj["meta"].items() if found.get(k) != v]
    if differing:
        raise RuntimeError(f"found with other metadata: {differing}")
    print(f"  uploaded  {found.get('ETag')} created {utc(int(found.get('Created', 0)))} UTC")


def main():
    args = parse_arguments()
    if yaml is None:
        sys.exit("Error: PyYAML is not installed (it is in the O2 environment's Python).")
    with open(args.config) as f:
        cfg = yaml.safe_load(f)
    errors = config_errors(cfg)
    if errors:
        sys.exit("Error: the configuration is not valid, nothing done:\n" + "\n".join(f"  {e}" for e in errors))

    upload_id = uuid.uuid4().hex[:12]
    host = cfg["ccdb_host"]
    try:
        objects, errors = plan(cfg, upload_id)
        if errors:
            sys.exit("Error: nothing uploaded, as these cannot be:\n" + "\n".join(f"  {e}" for e in errors))
        for obj in objects:
            show(host, obj)
    except urllib.error.URLError as error:
        sys.exit(f"Error: the CCDB cannot be read ({error.reason}): nothing uploaded.")
    print(f"\n{len(objects)} objects, upload id {upload_id}.")
    if not args.upload:
        print("Dry run: nothing uploaded. Rerun with --upload to send them.")
        return
    if not shutil.which("o2-ccdb-upload"):
        sys.exit("Error: o2-ccdb-upload is not on PATH: run inside the O2 environment.")
    done = []
    for obj in objects:
        print(f"\nUploading {obj['path']} ...")
        try:
            upload(host, obj)
        except (RuntimeError, OSError) as error:
            print(f"Error: {obj['path']}: {error}")
            print(f"Uploaded before it ({len(done)}): {done or 'none'}. The rest was not sent.")
            sys.exit(1)
        done.append(obj["path"])
    print(f"\nAll {len(done)} objects uploaded and read back.")


if __name__ == "__main__":
    main()
