import os
import json
import functools
import subprocess
import argparse
import collections
import tempfile
import urllib.parse

# Try to import yaml, but don't crash if it's missing unless the user tries to use a YAML file
try:
    import yaml
except ImportError:
    yaml = None

# --- CONFIGURATION & SMART GRID AUTHENTICATION ---
DEFAULT_OUTPUT_DIR = os.path.abspath("./")
HYPERLOOP_API = "https://alimonitor.cern.ch/alihyperloop-data"

# Check for a temporary active Grid token/proxy.
# Respect $TMPDIR (on macOS this is NOT /tmp, e.g. /var/folders/.../T/),
# and check for the JAliEn python-client token files (tokencert/tokenkey)
# in addition to the legacy VOMS proxy filename (x509up_u<uid>).
uid = os.getuid()
tmpdir = tempfile.gettempdir()

token_cert = os.path.join(tmpdir, f"tokencert_{uid}.pem")
token_key = os.path.join(tmpdir, f"tokenkey_{uid}.pem")
voms_proxy = os.path.join(tmpdir, f"x509up_u{uid}")

if os.path.exists(token_cert) and os.path.exists(token_key):
    # JAliEn python client token (from 'alien.py token-init')
    CERT_FILE = token_cert
    KEY_FILE = token_key
elif os.path.exists(voms_proxy):
    # Legacy VOMS/Globus proxy (from 'voms-proxy-init' / 'grid-proxy-init')
    KEY_FILE = voms_proxy
    CERT_FILE = voms_proxy
else:
    # No proxy/token found: fallback to original keys (will request the PEM passphrase)
    print("⚠️  WARNING: No active Grid token/proxy found.")
    print("You will be prompted for your PEM passphrase on every curl connection.")
    print("Tip: Run 'alien.py token-init' before running this script.\n")
    KEY_FILE = os.path.expanduser("~/.globus/userkey.pem")
    CERT_FILE = os.path.expanduser("~/.globus/usercert.pem")

# --- FUNCTIONS ---
def parse_arguments():
    """Parses command-line arguments."""
    parser = argparse.ArgumentParser(description="Download merged files from the Grid (Hyperloop/JAliEn).")
    # Batch option (can be JSON or YAML)
    parser.add_argument("--batch-file", default=None,
                        help="Path to a JSON or YAML file containing a list of trains to process.")
    # Single mode options (required only if --batch-file is not used)
    parser.add_argument("--train-id", default=None, help="The train ID to download files for.")
    parser.add_argument("--target-file", choices=["AnalysisResults.root", "AO2D.root"], default="AnalysisResults.root",
                        help="Specify which file to download: 'AnalysisResults.root' or 'AO2D.root'.")
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR,
                        help="Directory to save downloaded files. Defaults to the current directory.")
    parser.add_argument("--output-name", default=None,
                        help="Custom name for the downloaded file (only applied in --unified mode).")
    parser.add_argument("--per-run", action="store_true",
                    help="Disable unified download and download files for each individual run.")
    parser.add_argument("--run-list", default=None,
                        help="Optional: Comma-separated list of specific run numbers to download (only in --per-run mode).")
    parser.add_argument("--overwrite", action="store_true",
                        help="Force download and merge even if the output file already exists.")
    parser.add_argument("--sums-only", action="store_true",
                        help="With --batch-file: make only the 'sum' entries, from parts already on disk, without the Grid.")
    return parser.parse_args()

def run_cmd(cmd):
    """Executes a shell command and returns the result."""
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)

def get_hyperloop_json(endpoint, params):
    """GETs one endpoint of the Hyperloop API; returns the parsed JSON, or None with the reason printed."""
    url = f"{HYPERLOOP_API}/{endpoint}?{urllib.parse.urlencode(params)}"
    # --fail: curl exits 0 on an HTTP error otherwise, and an error page or error object would pass for an answer
    res = subprocess.run(["curl", "-s", "--fail", "--key", KEY_FILE, "--cert", CERT_FILE, "--insecure", url],
                         capture_output=True, text=True)
    if res.returncode != 0:
        print(f"  -> Error: request failed (curl exit code {res.returncode}): {url}")
        return None
    try:
        return json.loads(res.stdout)
    except json.JSONDecodeError:
        print(f"  -> Error: not JSON from {url}: {res.stdout[:200]!r}")
        return None

@functools.lru_cache(maxsize=None)
def fetch_train(train_id):
    """The train summary (trains/train.jsp), fetched once per train in a run: a batch item can extract several
    files from one train. The dict is shared between callers, which only read it."""
    train = get_hyperloop_json("trains/train.jsp", {"train_id": train_id})
    return train if isinstance(train, dict) else None

@functools.lru_cache(maxsize=None)
def fetch_wagon_configurations(train_id, wagons_timestamp):
    """{wagon id: configuration} of every wagon of the train, as it was when the train was composed
    (referenceTime = wagons_timestamp), or None if any of them could not be obtained.

    Checked on 12 November 2026 on train 786854 against dpl-config.json of the train's test workdir: the same values
    in every device, producers included, while the wagons' current configuration differed in an option changed
    since. The test workdir is deleted after a few weeks and the wagon history is not, so this is the one source for
    every train. The configurations are kept as Hyperloop returns them: matching a directory of AnalysisResults.root
    to its wagon (the _id<wagon id> suffix, when there is one) is left to the reader."""
    wagons = get_hyperloop_json("trains/wagons_derived_data.jsp",
                                {"train_id": train_id, "wagons_timestamp": wagons_timestamp})
    if not isinstance(wagons, dict) or not wagons or not all(str(w).isdigit() for w in wagons):
        print(f"Warning: no list of wagons for train {train_id}: {str(wagons)[:200]}")
        return None
    configurations = {}
    for wagon_id in wagons:
        config = get_hyperloop_json("analysis/wagon/download-configuration.jsp",
                                    {"wagon_id": wagon_id, "referenceTime": wagons_timestamp})
        # A configuration maps DPL device names to their options; anything else (an error object) is not one. Empty is
        # one: a wagon whose task has no options, as mcCollisionConverter (wagon 15164, a Core Service Wagon of the MC
        # trains of 2023 and 2024), answers {} at every referenceTime and now (checked 16 November 2026). A reader
        # looking for a device in it finds none and stops, as for any wagon without that device.
        if not isinstance(config, dict) or not all(isinstance(v, dict) for v in config.values()):
            print(f"Warning: no configuration for wagon {wagon_id} of train {train_id}: {str(config)[:200]}")
            return None
        configurations[wagon_id] = config
    return configurations

def config_path_for(data_path):
    """Where the configuration of a data file goes: next to it, X.root -> X.train.json. Next to the file so that it
    goes wherever the file is copied (the sandboxes copy DataFile/), and one rule, the same as TrainConfig::RecordPath
    in Codes/TrainConfig.h, which reads it."""
    return f"{os.path.splitext(data_path)[0]}.train.json"

def download_train_configuration(train_id, data_file, overwrite=False):
    """Saves the configuration the train ran with, at config_path_for(data file).
    Returns True when that file is on disk for this train.

    data_file is what the download functions return: the path of the data file, whether it was downloaded now
    (otherwise it was already on disk, and nothing here verifies that it comes from this train) and the runs it holds
    (None when not known). All three go into the record with the train, so that a reader can check the association.
    """
    data_path = data_file["path"]
    config_path = config_path_for(data_path)
    if not overwrite and os.path.isfile(config_path):
        try:
            with open(config_path) as f:
                existing_train = json.load(f)["hyperloop"]["train_id"]
        except (OSError, ValueError, KeyError, TypeError):
            existing_train = None  # unreadable (an interrupted write, say): replaced below
        if existing_train == train_id:
            print(f"-> Configuration '{config_path}' already exists, skipping.")
            return True
        if existing_train is not None:
            print(f"Error: '{config_path}' holds the configuration of train {existing_train}, not of train {train_id}: "
                  f"left as it is. Check which train '{os.path.basename(data_path)}' comes from, then rerun with "
                  f"overwrite.")
            return False

    print(f"Fetching the configuration of train {train_id}...")
    train = fetch_train(train_id)
    if train is None or not train.get("wagons_timestamp"):
        print(f"Warning: no wagons_timestamp for train {train_id}: configuration not saved.")
        return False
    configurations = fetch_wagon_configurations(train_id, train["wagons_timestamp"])
    if configurations is None:
        print(f"Warning: configuration of train {train_id} not saved.")
        return False
    for key in ("dataset_name", "package_tag"):
        if not train.get(key):
            print(f"Warning: no '{key}' in the summary of train {train_id}: recorded as null.")

    record = {
        "hyperloop": {
            "train_id": train_id,
            "wagons_timestamp": train["wagons_timestamp"],
            "dataset": train.get("dataset_name"),
            "package_tag": train.get("package_tag"),
            "source": "analysis/wagon/download-configuration.jsp, referenceTime = wagons_timestamp",
        },
        "data_file": {
            "name": os.path.basename(data_path),
            "downloaded_with_this_record": data_file["downloaded"],
            "runs": data_file["runs"],
        },
        "wagons": configurations,
    }
    write_record(config_path, record)
    print(f"-> Configuration saved: {config_path} (wagons {', '.join(configurations)})")
    return True

def write_record(path, record):
    """Writes a record to a temporary file and renames it, so that an interrupted run never leaves a truncated one."""
    tmp_path = f"{path}.tmp"
    with open(tmp_path, "w") as f:
        json.dump(record, f, indent=1, sort_keys=True)
    os.replace(tmp_path, path)

def hadd(target, sources):
    """hadd of sources into target, through a temporary file renamed only when hadd succeeded: a failed or
    interrupted merge never leaves a truncated target. Returns True on success; the error is printed otherwise."""
    tmp_path = f"{os.path.splitext(target)[0]}.tmp.root"
    res = subprocess.run(["hadd", "-f", tmp_path, *sources], capture_output=True, text=True)
    if res.returncode != 0:
        print(f"-> Error: hadd into '{target}' failed (exit code {res.returncode}):\n{res.stderr}")
        if os.path.exists(tmp_path):
            os.remove(tmp_path)
        return False
    os.replace(tmp_path, target)
    return True

def download_train_json(train_id, output_dir):
    """The train summary from Hyperloop, and a copy of it in output_dir, kept for debugging when something fails.
    Returns (summary, path of the copy), or (None, None)."""
    data = fetch_train(train_id)
    if data is None:
        print(f"Error: unable to get the summary of train {train_id}")
        return None, None
    json_path = os.path.join(output_dir, f"HyperloopID_{train_id}.json")
    with open(json_path, "w") as f:
        json.dump(data, f)
    return data, json_path

def get_alien_file_paths(alien_dir, target_file):
    """
    Finds the exact paths on the Grid depending on the file type.
    Handles the structural differences between AnalysisResults and AO2D.
    """
    if target_file == "AnalysisResults.root":
        return [f"alien://{alien_dir}/AnalysisResults.root"]
    
    elif target_file == "AO2D.root":
        print(f"  -> Searching for {target_file} in alien://{alien_dir}/AOD/ ...")
        cmd = f"alien.py find alien://{alien_dir}/AOD/ {target_file}"
        res = run_cmd(cmd)
        
        if res.returncode != 0 or not res.stdout.strip():
            return []
        
        paths = []
        for line in res.stdout.strip().split('\n'):
            line = line.strip()
            if line:
                if not line.startswith("alien://"):
                    line = f"alien://{line}"
                paths.append(line)
        return paths
    
    return []

def output_name_for(train_id, target_file, custom_name=None):
    """The name of the file a download writes: the one given, or Train_<id>_<target file>."""
    return custom_name or f"Train_{train_id}_{target_file}"

def download_and_merge_per_run_files(train_id, target_file, output_base_dir, custom_name=None, run_list=None, overwrite=False):
    """Reads the JSON and downloads the files PER RUN for successfully merged runs."""
    os.makedirs(output_base_dir, exist_ok=True)

    final_filename = output_name_for(train_id, target_file, custom_name)

    merged_file_path = os.path.join(output_base_dir, final_filename)

    if not overwrite and os.path.isfile(merged_file_path) and os.path.getsize(merged_file_path) > 0:
        print(f"-> The merged file '{merged_file_path}' already exists. Skipping download.")
        return {"path": merged_file_path, "downloaded": False, "runs": None}
    
    data, json_path = download_train_json(train_id, output_base_dir)
    if data is None:
        return None

    jobs = data.get("jobResults", [])
    if not jobs:
        print(f"Error: No runs found. Kept {json_path} for debugging.")
        return None

    has_errors = False

    # A run of the run list that the train does not have is a mistake in the list (a misspelled run, the run list of
    # another period): it stops the download before anything is fetched. A run the train did not merge is skipped, as
    # Hyperloop leaves it, and the result says which ones (a batch lists them at its end). A run without its file, or
    # whose copy fails, is an error further down. Checked on a new download only: a merged file already on disk is
    # taken as it is (above).
    run_set = set(run_list) if run_list is not None else None
    if run_set is not None:
        absent = sorted(run_set - {job.get("run") for job in jobs})
        if absent:
            print(f"Error: runs of the run list that train {train_id} does not have: {absent}")
            print(f"Nothing downloaded. Kept {json_path} for debugging.")
            return None
    files_to_merge = []
    merged_runs = set()
    not_merged = []

    for job in jobs:
        run_number = job.get("run")
        if run_set is not None and run_number not in run_set:
            print(f"Skipping Run {run_number}: not in the specified run list.")
            continue

        merge_state = job.get("merge_state")
        if merge_state != "done":
            print(f"Skipping Run {run_number}: merge_state is '{merge_state}'")
            not_merged.append(run_number)
            continue

        alien_dir = job.get("outputdir")
        alien_paths = get_alien_file_paths(alien_dir, target_file)
        if not alien_paths:
            print(f"  -> No '{target_file}' file found on the Grid for this run.")
            has_errors = True
            continue

        print(f"\n[Run {run_number}] Processing...")

        for i, alien_file_path in enumerate(alien_paths):
            suffix = f"_{i+1}" if len(alien_paths) > 1 else ""
            unique_filename = f"run_{run_number}{suffix}_{target_file}"
            local_file_path = os.path.join(output_base_dir, unique_filename)

            if not (os.path.isfile(local_file_path) and os.path.getsize(local_file_path) > 0):
                print(f"  -> Downloading {unique_filename}...")
                cmd = f"alien.py cp {alien_file_path} file:{local_file_path}"
                res = run_cmd(cmd)
            
                if res.returncode != 0:
                    print(f"  -> Error downloading {unique_filename}")
                    has_errors = True
                    continue

            files_to_merge.append(local_file_path)
            merged_runs.add(run_number)

    if files_to_merge and not has_errors:
        print(f"\n-> Starting the merge of {len(files_to_merge)} files into {merged_file_path}...")

        if hadd(merged_file_path, files_to_merge):
            print("-> Merge completed successfully!")
            print("-> Cleanup: removing intermediate root files...")
            for f in files_to_merge:
                if os.path.exists(f):
                    os.remove(f)
        else:
            has_errors = True
    elif not files_to_merge:
        print("\n-> No files downloaded for merging.")

    if not has_errors:
        print("\n-> Process completed without errors. Removing JSON...")
        if os.path.exists(json_path):
            os.remove(json_path)
    else:
        print(f"\n-> Errors occurred (download or merge). JSON kept at: {json_path}")
        print("-> The downloaded files have not been removed to allow retrying.")

    if has_errors or not files_to_merge:
        return None
    return {"path": merged_file_path, "downloaded": True, "runs": sorted(merged_runs), "not_merged": sorted(not_merged)}

def download_unified_file(train_id, target_file, output_base_dir, custom_name=None, overwrite=False):
    """Reads the JSON 'mergeResults' and downloads the SINGLE UNIFIED global file."""
    os.makedirs(output_base_dir, exist_ok=True)
    data, json_path = download_train_json(train_id, output_base_dir)
    if data is None:
        return None

    merge_results = data.get("mergeResults", [])
    if not merge_results:
        print(f"Error: No 'mergeResults' found in the JSON. Kept {json_path} for debugging.")
        return None

    merge_data = merge_results[0]
    merge_state = merge_data.get("merge_state")
    alien_dir = merge_data.get("outputdir")

    if merge_state != "done":
        print(f"Error: Global merge state is '{merge_state}'. Wait until it is 'done'.")
        print(f"Kept {json_path} for debugging.")
        return None

    if not alien_dir:
        print(f"Error: Could not retrieve 'outputdir'. Kept {json_path} for debugging.")
        return None

    clean_alien_dir = alien_dir.rstrip('/')
    alien_file_path = f"alien://{clean_alien_dir}/{target_file}"
    
    final_filename = output_name_for(train_id, target_file, custom_name)
    local_file_path = os.path.join(output_base_dir, final_filename)

    print(f"\nTargeting UNIFIED file at: {alien_file_path}")

    if not overwrite and os.path.isfile(local_file_path) and os.path.getsize(local_file_path) > 0:
        print(f"File '{final_filename}' already exists locally, skipping.")
        os.remove(json_path)
        return {"path": local_file_path, "downloaded": False, "runs": None}

    print(f"Downloading unified file to {local_file_path} ...")
    cmd = f"alien.py cp {alien_file_path} file:{local_file_path}"
    res = run_cmd(cmd)
    
    if res.returncode == 0:
        print(f"-> Successfully downloaded: {local_file_path}")
        print("-> Cleaning up temporary JSON file...")
        os.remove(json_path)
        merged_runlist = merge_data.get("merged_runlist") or ""
        runs = sorted(int(run) for run in str(merged_runlist).split(",") if run.strip()) or None
        return {"path": local_file_path, "downloaded": True, "runs": runs}
    print(f"-> Error: Could not download the unified file from {alien_file_path}")
    print(f"-> The JSON file has been kept for debugging: {json_path}")
    return None

def process_single_train(train_id, target_file, output_dir, output_name, per_run=False, run_list=None, overwrite=False):
    """Dispatches the execution to either unified or per-run mode, then saves the train's configuration next to the
    data file. Returns (problem, not_merged): what was not done, which fails a batch, or None; and the runs left out
    because the train did not merge them, or None. run_list is the list of runs from parse_run_list, or None for every
    run of the train."""
    # An integer whether it comes from the command line or from a batch file: it is compared with the train
    # recorded in an existing configuration
    train_id = int(train_id)
    if not per_run:
        print(f"\n--- MODE: UNIFIED DOWNLOAD [Train ID: {train_id}] ---")
        data_file = download_unified_file(train_id, target_file, output_dir, output_name, overwrite)
    else:
        print(f"\n--- MODE: PER-RUN DOWNLOAD [Train ID: {train_id}] ---")
        data_file = download_and_merge_per_run_files(train_id, target_file, output_dir, output_name, run_list, overwrite)
    if data_file is None:
        # The download said why; a configuration with no data file next to it is useless
        return "data file not downloaded", None
    not_merged = data_file.get("not_merged") or None
    # Also for a data file already on disk, so that running a batch again adds the configuration to old downloads
    if not download_train_configuration(train_id, data_file, overwrite):
        return "configuration not saved", not_merged
    return None, not_merged

def parse_run_list(value):
    """The runs of a run list, written "123, 456" or as one number (YAML reads a single run as an int). A run that is
    not a number, or one listed twice, raises ValueError: a run listed twice changes nothing downloaded, but it is the
    mark of a list pasted together by hand."""
    runs = [run.strip() for run in str(value).split(",") if run.strip()]
    if not runs or not all(run.isdigit() for run in runs):
        raise ValueError(f"is not a comma-separated list of run numbers: '{value}'")
    counts = collections.Counter(int(run) for run in runs)
    repeated = sorted(run for run, count in counts.items() if count > 1)
    if repeated:
        raise ValueError(f"lists runs twice: {repeated}")
    return list(counts)

# The keys of a batch entry: either a download of one train (one file, or several 'extractions' of it) or the sum of
# files that downloads of the same batch write. Any other key stops the batch before it starts: skipped, a misspelled
# key would silently drop what it was for.
TRAIN_KEYS = {"train_id", "output_dir", "output_name", "target_file", "per_run", "run_list", "overwrite", "extractions"}
EXTRACTION_KEYS = {"output_name", "target_file", "per_run", "run_list", "overwrite"}
SUM_KEYS = {"sum", "output_dir", "parts", "overwrite"}

Task = collections.namedtuple("Task", "target_file output_name per_run run_list overwrite")

def batch_tasks(item):
    """The files a download entry writes, with the entry's values as defaults for its extractions. A run list is
    parsed (parse_run_list; a bad one raises ValueError naming the file) and makes the download per run, the only
    one that can select runs. The command line goes through here too, as an entry of its own."""
    for task in item.get("extractions", [item]):
        target_file = task.get("target_file", item.get("target_file", "AnalysisResults.root"))
        output_name = output_name_for(item["train_id"], target_file, task.get("output_name", item.get("output_name")))
        run_list = task.get("run_list", item.get("run_list"))
        if run_list is not None:
            try:
                run_list = parse_run_list(run_list)
            except ValueError as error:
                raise ValueError(f"'{output_name}': the run list {error}") from None
        yield Task(target_file, output_name, task.get("per_run", item.get("per_run", False)) or run_list is not None,
                   run_list, task.get("overwrite", item.get("overwrite", False)))

def batch_errors(trains_list):
    """What is wrong with a batch, one line each, found before anything is downloaded: an entry that is not a download
    or a sum, unknown keys, and a sum that cannot be made whatever is on disk. A sum needs two parts or more, each
    once (hadd would add one listed twice), each written by a download of this batch into the sum's directory: the
    batch file is then where every part of a sum comes from, and a misspelled part is found before the downloads. The
    sum must not be one of those files either: its record would replace the part's own. Every run list is parsed
    here, so a bad one stops the batch before anything is downloaded."""
    if not isinstance(trains_list, list):
        return ["the batch is not a list of entries"]
    errors = []
    for index, item in enumerate(trains_list, start=1):
        if not isinstance(item, dict) or ("train_id" in item) == ("sum" in item):
            errors.append(f"entry #{index}: it needs exactly one of 'train_id' and 'sum'")
            continue
        allowed = TRAIN_KEYS if "train_id" in item else SUM_KEYS
        if set(item) - allowed:
            errors.append(f"entry #{index}: unknown keys {sorted(set(item) - allowed)}")
        for task_index, task in enumerate(item.get("extractions", []), start=1):
            if not isinstance(task, dict) or set(task) - EXTRACTION_KEYS:
                errors.append(f"entry #{index}, extraction #{task_index}: unknown keys "
                              f"{sorted(set(task) - EXTRACTION_KEYS) if isinstance(task, dict) else task}")
        if "sum" in item and ("output_dir" not in item or not isinstance(item.get("parts"), list)):
            errors.append(f"entry #{index}: a sum needs 'output_dir' and the list 'parts'")
    if errors:
        return errors

    declared = {}
    for index, item in enumerate(trains_list, start=1):
        if "train_id" not in item:
            continue
        try:
            tasks = list(batch_tasks(item))
        except ValueError as error:
            errors.append(f"entry #{index}, {error}")
            continue
        directory = os.path.abspath(item.get("output_dir", DEFAULT_OUTPUT_DIR))
        declared.setdefault(directory, set()).update(task.output_name for task in tasks)
    for index, item in enumerate(trains_list, start=1):
        if "sum" not in item:
            continue
        parts = item["parts"]
        written = declared.get(os.path.abspath(item["output_dir"]), set())
        if len(parts) < 2:
            errors.append(f"entry #{index}: a sum needs at least two parts")
        if len(set(parts)) != len(parts):
            errors.append(f"entry #{index}: parts listed twice: {sorted({p for p in parts if parts.count(p) > 1})}")
        if set(parts) - written:
            errors.append(f"entry #{index}: no download of this batch writes these parts into '{item['output_dir']}': "
                          f"{sorted(set(parts) - written)}")
        if item["sum"] in written:
            errors.append(f"entry #{index}: the sum '{item['sum']}' is a file a download of this batch writes")
    return errors

def read_merged_from(record_path):
    """The parts an existing record lists, or None when it is not there or cannot be read. A readable record without
    them is the record of a download, which a sum must not replace: it raises."""
    if not os.path.isfile(record_path):
        return None
    try:
        with open(record_path) as f:
            record = json.load(f)
    except (OSError, ValueError):
        return None  # unreadable (an interrupted write, say): replaced
    if not isinstance(record, dict) or "merged_from" not in record:
        raise ValueError(f"'{record_path}' is the record of a download, not of a sum")
    return record["merged_from"]

def make_sum(item):
    """Makes the sum of a batch entry (checked by batch_errors) and writes its record, the "merged_from" form read by
    TrainConfig::Load (Codes/TrainConfig.h), its list of parts taken from the entry. A part or its record not on disk
    refuses it. A sum already on disk is not made again without 'overwrite': its record then says
    summed_with_this_record false, since nothing shows that it holds these parts, and the reader warns about it; a part
    newer than it refuses instead, as the sum may not hold that part as it is now. The options of the parts are
    compared by the reader, which knows which of them it reads. Returns True when the sum and its record are on disk."""
    output_dir = item["output_dir"]
    sum_name = item["sum"]
    sum_path = os.path.join(output_dir, sum_name)
    part_paths = [os.path.join(output_dir, part) for part in item["parts"]]
    print(f"\n=== Sum '{sum_path}' of {len(part_paths)} parts ===")

    missing = [p for path in part_paths for p in (path, config_path_for(path)) if not os.path.isfile(p)]
    if missing:
        print(f"Error: not on disk: {missing}.")
        return False
    record_path = config_path_for(sum_path)
    merged_from = [os.path.basename(config_path_for(path)) for path in part_paths]
    try:
        existing = read_merged_from(record_path)
    except ValueError as error:
        print(f"Error: {error}: left as it is.")
        return False

    if os.path.isfile(sum_path) and not item.get("overwrite", False):
        newer = [p for p in part_paths if os.path.getmtime(p) > os.path.getmtime(sum_path)]
        if newer:
            print(f"Error: {newer} newer than the sum, which may not hold them as they are now: rerun with overwrite.")
            return False
        if existing == merged_from:
            print(f"-> Record '{record_path}' already exists, skipping.")
            return True
        if existing is not None:
            print(f"Error: '{record_path}' lists other parts ({existing}): left as it is. Check what the sum holds, "
                  f"then rerun with overwrite.")
            return False
        print("-> The sum is already on disk: not made again, and its record says so.")
        summed = False
    else:
        # The old record goes first: interrupted, this leaves a sum without a record, which the reader warns about,
        # never a sum with the record of another
        if os.path.exists(record_path):
            os.remove(record_path)
        if not hadd(sum_path, part_paths):
            return False
        summed = True
    write_record(record_path, {"data_file": {"name": sum_name, "summed_with_this_record": summed},
                               "merged_from": merged_from})
    print(f"-> Record saved: {record_path}")
    return True

if __name__ == "__main__":
    args = parse_arguments()

    # Batch Mode (JSON or YAML)
    if args.batch_file:
        if not os.path.isfile(args.batch_file):
            print(f"Error: Batch file not found at '{args.batch_file}'")
            exit(1)
            
        file_ext = os.path.splitext(args.batch_file)[1].lower()
        
        # Parse based on file extension
        if file_ext in ['.yaml', '.yml']:
            if yaml is None:
                print("Error: PyYAML is not installed. Run 'pip install pyyaml' to use YAML files.")
                exit(1)
            print(f"=== STARTING BATCH PROCESSING FROM YAML: {args.batch_file} ===")
            with open(args.batch_file) as f:
                trains_list = yaml.safe_load(f)
        else:
            print(f"=== STARTING BATCH PROCESSING FROM JSON: {args.batch_file} ===")
            with open(args.batch_file) as f:
                trains_list = json.load(f)
            
        errors = batch_errors(trains_list)
        if errors:
            print("Error: the batch file is not valid, nothing done:")
            for error in errors:
                print(f"  {error}")
            exit(1)
        downloads = [item for item in trains_list if "train_id" in item]
        sums = [item for item in trains_list if "sum" in item]

        failures = []
        notes = []
        if not args.sums_only:
            for index, item in enumerate(downloads):
                t_id = item["train_id"]
                o_dir = item.get("output_dir", DEFAULT_OUTPUT_DIR)
                extractions = list(batch_tasks(item))
                print(f"\n=== Processing Batch Item {index+1}/{len(downloads)} [Train: {t_id} | {len(extractions)} tasks] ===")

                for task_idx, (t_file, o_name, is_per_run, run_list, overwrite) in enumerate(extractions):
                    print(f"\n--- Task {task_idx+1}/{len(extractions)}: Train {t_id} | Target File: {t_file} | Output Name: {o_name} | Per-Run: {is_per_run} | Run List: {run_list} ---")
                    problem, not_merged = process_single_train(t_id, t_file, o_dir, o_name, is_per_run, run_list, overwrite)
                    label = f"train {t_id} -> {o_name}"
                    if problem:
                        failures.append(f"{label}: {problem}")
                    if not_merged:
                        notes.append(f"{label}: {not_merged}")
        # After every download, so that a sum is made from parts as this run left them
        for item in sums:
            if not make_sum(item):
                failures.append(f"sum -> {item['sum']}: not made, or its record not written")
        print("\n=== BATCH PROCESSING COMPLETED ===")
        if notes:
            print(f"\nMade without the runs the train did not merge ({len(notes)}):")
            for entry in notes:
                print(f"  {entry}")
        # Listed again at the end, and the exit code says it: one warning among a whole batch's output scrolls past
        if failures:
            print(f"\nNot done for {len(failures)} entr{'y' if len(failures) == 1 else 'ies'}:")
            for entry in failures:
                print(f"  {entry}")
            exit(1)

    # Standard Mode (Single Train via CLI parameters)
    else:
        if not args.train_id:
            print("Error: You must provide either --train-id or --batch-file to run the script.")
            exit(1)
        if args.sums_only:
            print("Error: --sums-only makes the sums of a batch: it needs --batch-file.")
            exit(1)

        entry = {"train_id": args.train_id, "target_file": args.target_file, "output_name": args.output_name,
                 "per_run": args.per_run, "run_list": args.run_list, "overwrite": args.overwrite}
        try:
            task = next(batch_tasks(entry))
        except ValueError as error:
            print(f"Error: --run-list {error}")
            exit(1)

        print(f"\n--- Single Task: Train {args.train_id} | Target File: {task.target_file} | Output Name: {task.output_name} | Per-Run: {task.per_run} | Run List: {task.run_list} ---")
        problem, not_merged = process_single_train(args.train_id, task.target_file, args.output_dir, task.output_name,
                                                   task.per_run, task.run_list, task.overwrite)
        if not_merged:
            print(f"\nMade without the runs the train did not merge: {not_merged}")
        if problem:
            exit(1)