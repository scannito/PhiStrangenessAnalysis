import os
import json
import subprocess
import argparse

# Try to import yaml, but don't crash if it's missing unless the user tries to use a YAML file
try:
    import yaml
except ImportError:
    yaml = None

# --- CONFIGURATION ---
KEY_FILE = os.path.expanduser("~/.globus/userkey.pem")
CERT_FILE = os.path.expanduser("~/.globus/usercert.pem")
DEFAULT_OUTPUT_DIR = os.path.abspath("./")

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

def download_per_run_files(train_id, target_file, output_base_dir):
    """Reads the JSON and downloads the files PER RUN for successfully merged runs."""
    os.makedirs(output_base_dir, exist_ok=True)
    json_path = download_train_json(train_id, output_base_dir)
    if not json_path:
        return

    with open(json_path) as f:
        data = json.load(f)

    jobs = data.get("jobResults", [])
    if not jobs:
        print(f"Error: No runs found. Kept {json_path} for debugging.")
        return
        
    print(f"Found {len(jobs)} runs in the JSON.")
    has_errors = False

    for job in jobs:
        run_number = job.get("run")
        merge_state = job.get("merge_state")
        alien_dir = job.get("outputdir")

        if merge_state != "done":
            print(f"Skipping Run {run_number}: merge_state is '{merge_state}'")
            continue

        local_run_dir = os.path.join(output_base_dir, str(run_number))
        os.makedirs(local_run_dir, exist_ok=True)

        print(f"\n[Run {run_number}] Processing...")
        alien_paths = get_alien_file_paths(alien_dir, target_file)
        
        if not alien_paths:
            print(f"  -> No '{target_file}' file found on the Grid for this run.")
            has_errors = True
            continue

        for i, alien_file_path in enumerate(alien_paths):
            file_name = target_file if len(alien_paths) == 1 else f"{target_file.replace('.root', '')}_{i+1}.root"
            local_file_path = os.path.join(local_run_dir, file_name)

            if os.path.isfile(local_file_path) and os.path.getsize(local_file_path) > 0:
                print(f"  -> File '{file_name}' already exists locally, skipping.")
                continue

            print(f"  -> Downloading: {alien_file_path} \n     to {local_file_path}")
            cmd = f"alien_cp -q {alien_file_path} file:{local_file_path}"
            res = run_cmd(cmd)
            
            if res.returncode == 0:
                print(f"  -> Completed: {file_name}")
            else:
                print(f"  -> Error downloading {file_name}")
                has_errors = True

    if not has_errors:
        print("\n-> All downloads finished successfully. Cleaning up temporary JSON file...")
        os.remove(json_path)
    else:
        print(f"\n-> Some errors occurred during the process.")
        print(f"-> The JSON file has been kept for debugging: {json_path}")

def download_unified_file(train_id, target_file, output_base_dir, custom_name=None):
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

    if os.path.isfile(local_file_path) and os.path.getsize(local_file_path) > 0:
        print(f"File '{final_filename}' already exists locally, skipping.")
        os.remove(json_path)
        return

    print(f"Downloading unified file to {local_file_path} ...")
    cmd = f"alien_cp -q {alien_file_path} file:{local_file_path}"
    res = run_cmd(cmd)
    
    if res.returncode == 0:
        print(f"-> Successfully downloaded: {local_file_path}")
        print("-> Cleaning up temporary JSON file...")
        os.remove(json_path)
    else:
        print(f"-> Error: Could not download the unified file from {alien_file_path}")
        print(f"-> The JSON file has been kept for debugging: {json_path}")

def process_single_train(train_id, target_file, output_dir, output_name, per_run):
    """Dispatches the execution to either unified or per-run mode."""
    if not per_run:
        print(f"\n--- MODE: UNIFIED DOWNLOAD [Train ID: {train_id}] ---")
        download_unified_file(train_id, target_file, output_dir, output_name)
    else:
        print(f"\n--- MODE: PER-RUN DOWNLOAD [Train ID: {train_id}] ---")
        download_per_run_files(train_id, target_file, output_dir)

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
            t_file = item.get("target_file", "AnalysisResults.root")
            o_dir = item.get("output_dir", DEFAULT_OUTPUT_DIR)
            o_name = item.get("output_name", None)
            is_per_run = item.get("per_run", False)
            
            if not t_id:
                print(f"Warning: Missing 'train_id' in batch entry #{index+1}. Skipping.")
                continue
                
            print(f"\nProcessing batch item {index+1}/{len(trains_list)}")
            process_single_train(t_id, t_file, o_dir, o_name, is_per_run)
        print("\n=== BATCH PROCESSING COMPLETED ===")

    # Standard Mode (Single Train via CLI parameters)
    else:
        if not args.train_id:
            print("Error: You must provide either --train-id or --batch-json to run the script.")
            exit(1)
        process_single_train(args.train_id, args.target_file, args.output_dir, args.output_name, args.per_run)