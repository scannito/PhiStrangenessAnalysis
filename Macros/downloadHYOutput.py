import os
import json
import functools
import subprocess
import argparse
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
# The train configurations go to TrainConfigs/, which mirrors DataFile/ (see config_path_for); both are in the
# repository, one level above this script, and TrainConfigs/ is ignored by git as the data files are
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
DATA_ROOT = os.path.join(REPO_ROOT, "DataFile")
CONFIG_ROOT = os.path.join(REPO_ROOT, "TrainConfigs")

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
        # A configuration maps DPL device names to their options; anything else (an error object) is not one
        if not isinstance(config, dict) or not config or not all(isinstance(v, dict) for v in config.values()):
            print(f"Warning: no configuration for wagon {wagon_id} of train {train_id}: {str(config)[:200]}")
            return None
        configurations[wagon_id] = config
    return configurations

def config_path_for(data_path):
    """Where the configuration of a data file goes: TrainConfigs/ at the data file's place under DataFile/, with the
    data file's name and .json. The same name keeps the two tied, and the directories of DataFile/ (pp/DeltaY/Data,
    MC, MCClosure, pp_ref/...) sort the records the same way. None for a data file outside DataFile/."""
    relative = os.path.relpath(os.path.realpath(data_path), os.path.realpath(DATA_ROOT))
    if relative == os.pardir or relative.startswith(os.pardir + os.sep):
        return None
    return os.path.join(CONFIG_ROOT, f"{os.path.splitext(relative)[0]}.json")

def download_train_configuration(train_id, data_file, overwrite=False):
    """Saves the configuration the train ran with, at config_path_for(data file).
    Returns True when that file is on disk for this train.

    data_file is what the download functions return: the path of the data file, whether it was downloaded now
    (otherwise it was already on disk, and nothing here verifies that it comes from this train) and the runs it holds
    (None when not known). All three go into the record with the train, so that a reader can check the association.
    """
    data_path = data_file["path"]
    config_path = config_path_for(data_path)
    if config_path is None:
        print(f"Error: '{data_path}' is not under {DATA_ROOT}: its configuration has no place in {CONFIG_ROOT}, "
              f"not saved.")
        return False
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
    # Written to a temporary file and renamed, so that an interrupted run never leaves a truncated record
    os.makedirs(os.path.dirname(config_path), exist_ok=True)
    tmp_path = f"{config_path}.tmp"
    with open(tmp_path, "w") as f:
        json.dump(record, f, indent=1, sort_keys=True)
    os.replace(tmp_path, config_path)
    print(f"-> Configuration saved: {config_path} (wagons {', '.join(configurations)})")
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

def download_and_merge_per_run_files(train_id, target_file, output_base_dir, custom_name=None, run_list=None, overwrite=False):
    """Reads the JSON and downloads the files PER RUN for successfully merged runs."""
    os.makedirs(output_base_dir, exist_ok=True)

    final_filename = custom_name if custom_name else f"Train_{train_id}_{target_file}"

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

    run_set = {int(run) for run in run_list.split(",")} if run_list else None
    files_to_merge = []
    merged_runs = set()

    for job in jobs:
        run_number = job.get("run")
        if run_set is not None and run_number not in run_set:
            print(f"Skipping Run {run_number}: not in the specified run list.")
            continue

        merge_state = job.get("merge_state")
        if merge_state != "done":
            print(f"Skipping Run {run_number}: merge_state is '{merge_state}'")
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

        files_string = " ".join(files_to_merge)
        hadd_cmd_string = f"hadd -f {merged_file_path} {files_string}"
        
        merge_res = run_cmd(hadd_cmd_string)

        if merge_res.returncode == 0:
            print("-> Merge completed successfully!")
            print("-> Cleanup: removing intermediate root files...")
            for f in files_to_merge:
                if os.path.exists(f):
                    os.remove(f)
        else:
            print("-> Error during hadd merge!")
            print(merge_res.stderr)
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
    return {"path": merged_file_path, "downloaded": True, "runs": sorted(merged_runs)}

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
    
    final_filename = custom_name if custom_name else f"Train_{train_id}_{target_file}"
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
    data file. Returns False when the data file is there but its configuration could not be saved."""
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
        return True  # the data download failed and said so; a configuration with no data file next to it is useless
    # Also for a data file already on disk, so that running a batch again adds the configuration to old downloads
    return download_train_configuration(train_id, data_file, overwrite)

def validate_run_list(run_list_input):
    """
    Validates and cleans the run_list input.
    Returns a clean comma-separated string, or None if invalid.
    """
    if not run_list_input:
        return None
        
    try:
        # str() prevents crashes if YAML parses a single run as an int
        # strip() removes accidental spaces like "123, 456"
        runs = [run.strip() for run in str(run_list_input).split(",") if run.strip()]
        
        # Check if every element is a valid number
        if not all(run.isdigit() for run in runs):
            return None
            
        return ",".join(runs) # Returns a perfectly formatted string: "123,456"
    except Exception:
        return None

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
            
        missing_configurations = []
        for index, item in enumerate(trains_list):
            t_id = item.get("train_id")
            o_dir = item.get("output_dir", DEFAULT_OUTPUT_DIR)

            if not t_id:
                print(f"Warning: Missing 'train_id' in batch entry #{index+1}. Skipping.")
                continue

            extractions = item.get("extractions", [item])
            print(f"\n=== Processing Batch Item {index+1}/{len(trains_list)} [Train: {t_id} | {len(extractions)} tasks] ===")

            for task_idx, task in enumerate(extractions):
                t_file = task.get("target_file", item.get("target_file", "AnalysisResults.root"))
                o_name = task.get("output_name", item.get("output_name", None))
                is_per_run = task.get("per_run", item.get("per_run", False))
                run_list = task.get("run_list", item.get("run_list", None))
                overwrite = task.get("overwrite", item.get("overwrite", False))

                if run_list:
                    run_list = validate_run_list(run_list)
                    if not run_list:
                        print(f"Error: Invalid 'run_list' in batch entry #{index+1}. Must be comma-separated integers. Skipping.")
                        continue

                    if not is_per_run:
                        print(f"Warning: 'run_list' found in batch entry #{index+1} but 'per_run' is missing or false.")
                        print("Auto-enabling 'per_run' mode...")
                        is_per_run = True

                print(f"\n--- Task {task_idx+1}/{len(extractions)}: Train {t_id} | Target File: {t_file} | Output Name: {o_name} | Per-Run: {is_per_run} | Run List: {run_list} ---")
                if not process_single_train(t_id, t_file, o_dir, o_name, is_per_run, run_list, overwrite):
                    missing_configurations.append(f"train {t_id} -> {o_name or t_file}")
        print("\n=== BATCH PROCESSING COMPLETED ===")
        # Listed again at the end, and the exit code says it: one warning among a whole batch's output scrolls past
        if missing_configurations:
            print(f"\nConfiguration NOT saved for {len(missing_configurations)} file(s):")
            for entry in missing_configurations:
                print(f"  {entry}")
            exit(1)

    # Standard Mode (Single Train via CLI parameters)
    else:
        if not args.train_id:
            print("Error: You must provide either --train-id or --batch-file to run the script.")
            exit(1)

        if args.run_list:
            args.run_list = validate_run_list(args.run_list)
            if not args.run_list:
                print("Error: Invalid '--run-list' format. Must be comma-separated integers (e.g. '123,456').")
                exit(1)
                
            if not args.per_run:
                print("Warning: '--run-list' was provided, but '--per-run' is missing.")
                print("Auto-enabling '--per-run' mode to process specific runs...")
                args.per_run = True

        print(f"\n--- Single Task: Train {args.train_id} | Target File: {args.target_file} | Output Name: {args.output_name} | Per-Run: {args.per_run} | Run List: {args.run_list} ---")
        if not process_single_train(args.train_id, args.target_file, args.output_dir, args.output_name, args.per_run, args.run_list, args.overwrite):
            exit(1)