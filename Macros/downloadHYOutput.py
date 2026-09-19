import os
import json
import subprocess
import argparse
import tempfile

# Try to import yaml, but don't crash if it's missing unless the user tries to use a YAML file
try:
    import yaml
except ImportError:
    yaml = None

# --- CONFIGURATION & SMART GRID AUTHENTICATION ---
DEFAULT_OUTPUT_DIR = os.path.abspath("./")

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

def download_train_json(train_id, output_dir):
    """Downloads the train summary JSON file from Hyperloop."""
    json_path = os.path.join(output_dir, f"HyperloopID_{train_id}.json")
    url = f"https://alimonitor.cern.ch/alihyperloop-data/trains/train.jsp?train_id={train_id}"
    
    print(f"Fetching information for train {train_id}...")
    cmd = f"curl --key {KEY_FILE} --cert {CERT_FILE} --insecure {url} -o {json_path}"
    run_cmd(cmd)
    
    if not os.path.exists(json_path):
        print(f"Error: unable to download JSON to {json_path}")
        return None
    return json_path

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
        return
    
    json_path = download_train_json(train_id, output_base_dir)
    if not json_path:
        return

    with open(json_path) as f:
        data = json.load(f)

    jobs = data.get("jobResults", [])
    if not jobs:
        print(f"Error: No runs found. Kept {json_path} for debugging.")
        return

    has_errors = False

    run_set = {int(run) for run in run_list.split(",")} if run_list else None
    files_to_merge = []

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

def download_unified_file(train_id, target_file, output_base_dir, custom_name=None, overwrite=False):
    """Reads the JSON 'mergeResults' and downloads the SINGLE UNIFIED global file."""
    os.makedirs(output_base_dir, exist_ok=True)
    json_path = download_train_json(train_id, output_base_dir)
    if not json_path:
        return

    with open(json_path) as f:
        data = json.load(f)

    merge_results = data.get("mergeResults", [])
    if not merge_results:
        print(f"Error: No 'mergeResults' found in the JSON. Kept {json_path} for debugging.")
        return

    merge_data = merge_results[0]
    merge_state = merge_data.get("merge_state")
    alien_dir = merge_data.get("outputdir")

    if merge_state != "done":
        print(f"Error: Global merge state is '{merge_state}'. Wait until it is 'done'.")
        print(f"Kept {json_path} for debugging.")
        return

    if not alien_dir:
        print(f"Error: Could not retrieve 'outputdir'. Kept {json_path} for debugging.")
        return

    clean_alien_dir = alien_dir.rstrip('/')
    alien_file_path = f"alien://{clean_alien_dir}/{target_file}"
    
    final_filename = custom_name if custom_name else f"Train_{train_id}_{target_file}"
    local_file_path = os.path.join(output_base_dir, final_filename)

    print(f"\nTargeting UNIFIED file at: {alien_file_path}")

    if not overwrite and os.path.isfile(local_file_path) and os.path.getsize(local_file_path) > 0:
        print(f"File '{final_filename}' already exists locally, skipping.")
        os.remove(json_path)
        return

    print(f"Downloading unified file to {local_file_path} ...")
    cmd = f"alien.py cp {alien_file_path} file:{local_file_path}"
    res = run_cmd(cmd)
    
    if res.returncode == 0:
        print(f"-> Successfully downloaded: {local_file_path}")
        print("-> Cleaning up temporary JSON file...")
        os.remove(json_path)
    else:
        print(f"-> Error: Could not download the unified file from {alien_file_path}")
        print(f"-> The JSON file has been kept for debugging: {json_path}")

def process_single_train(train_id, target_file, output_dir, output_name, per_run=False, run_list=None, overwrite=False):
    """Dispatches the execution to either unified or per-run mode."""
    if not per_run:
        print(f"\n--- MODE: UNIFIED DOWNLOAD [Train ID: {train_id}] ---")
        download_unified_file(train_id, target_file, output_dir, output_name, overwrite)
    else:
        print(f"\n--- MODE: PER-RUN DOWNLOAD [Train ID: {train_id}] ---")
        download_and_merge_per_run_files(train_id, target_file, output_dir, output_name, run_list, overwrite)

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
                process_single_train(t_id, t_file, o_dir, o_name, is_per_run, run_list, overwrite)
        print("\n=== BATCH PROCESSING COMPLETED ===")

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
        process_single_train(args.train_id, args.target_file, args.output_dir, args.output_name, args.per_run, args.run_list, args.overwrite)