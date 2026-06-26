import json
import os
import copy
import subprocess
import argparse

# ==============================================================================
# GLOBAL SETTINGS & ARGUMENT PARSING
# ==============================================================================
parser = argparse.ArgumentParser(description="Generate and run systematic variations for ALICE analysis.")
parser.add_argument("keyword", help="Configuration keyword (e.g., 'data', 'mcclosure') or direct path to a .json file.")
parser.add_argument("coll_sys", nargs="?", default="", help="Collision system (e.g., 'pp', 'PbPb'). Leave empty if passing a direct .json file.")
parser.add_argument("-o", "--outdir", help="Custom name for the output systematics folder (e.g., 'test_efficiencies')", default=None)

args = parser.parse_args()

keyword = args.keyword
coll_sys = args.coll_sys

# Map the keyword to the specific JSON configuration file (mirroring bash logic)
if keyword.endswith(".json"):
    NOMINAL_JSON_PATH = keyword
    # Extract a clean name for the fallback output folder
    base_name = os.path.basename(NOMINAL_JSON_PATH).replace(".json", "")
    CONFIG_KEYWORD = base_name.replace("globalConfig", "")
else:
    # Standardize keyword to lowercase for checking
    kw_lower = keyword.lower()
    if kw_lower == "data":
        prefix = "Data"
    elif kw_lower == "mcclosure":
        prefix = "MCClosure"
    elif kw_lower == "mcclosurewpdg":
        prefix = "MCClosureWPDG"
    elif kw_lower == "mcclosuregen":
        prefix = "MCClosureGen"
    else:
        # Fallback for custom keywords without .json
        prefix = keyword 
        
    NOMINAL_JSON_PATH = f"../JSONConfigs/globalConfig{prefix}{coll_sys}.json"
    CONFIG_KEYWORD = f"{prefix}{coll_sys}"

# Set the output directory dynamically
if args.outdir:
    # Use the custom directory name provided by the user
    OUTPUT_CONFIG_DIR = f"../JSONConfigs/{args.outdir}"
else:
    # Fallback to the auto-generated name
    OUTPUT_CONFIG_DIR = f"../JSONConfigs/Systematics_{CONFIG_KEYWORD}"

BASH_SCRIPT = "./run_analysis.sh" # Your bash script

# Create the directory for the generated configuration files if it doesn't exist
os.makedirs(OUTPUT_CONFIG_DIR, exist_ok=True)

# ==============================================================================
# DEFINITION OF SYSTEMATICS
# Each dictionary contains the name of the variation and the modifications
# ==============================================================================
systematics = [
    {
        "name": "Nominal",
        "changes": {} # The nominal variation does not alter anything
    },
    {
        "name": "Sys_Extrap_BlastWave",
        "changes": {
            "correlation_task": {"extrap_function": "BlastWave"}
        }
    },
    {
        "name": "Sys_Extrap_FitRange_Wide",
        "changes": {
            "correlation_task": {"extrap_fit_range": [0.3, 8.0]}
        }
    }
    # Add more variations here...
]

# ==============================================================================
# 1. GENERATION OF CONFIGURATIONS
# ==============================================================================
print("======================================================")
print(f" GENERATING CONFIGURATIONS FROM : {NOMINAL_JSON_PATH}")
print(f" OUTPUT DIRECTORY               : {OUTPUT_CONFIG_DIR}")
print("======================================================")

if not os.path.isfile(NOMINAL_JSON_PATH):
    print(f"[FATAL ERROR] The JSON file '{NOMINAL_JSON_PATH}' does not exist!")
    exit(1)

with open(NOMINAL_JSON_PATH, "r") as f:
    nominal_config = json.load(f)

generated_configs = []

for sys_var in systematics:
    sys_name = sys_var["name"]
    
    # Perform a deep copy to avoid polluting the original dictionary
    current_config = copy.deepcopy(nominal_config)
    
    # Apply modifications
    for task_name, modifications in sys_var["changes"].items():
        if task_name in current_config:
            for key, value in modifications.items():
                current_config[task_name][key] = value

    # Update output prefixes (to avoid overwriting ROOT files from previous runs)
    if sys_name != "Nominal":
        for task_name, task_content in current_config.items():
            if isinstance(task_content, dict):
                if "output_prefix" in task_content:
                    task_content["output_prefix"] += f"{sys_name}_"
                if "input_output_prefix" in task_content:
                    task_content["input_output_prefix"] += f"{sys_name}_"

    sys_config_filename = os.path.join(OUTPUT_CONFIG_DIR, f"config_{sys_name}.json")
    with open(sys_config_filename, "w") as f:
        json.dump(current_config, f, indent=4)
        
    print(f"[OK] Generated: {sys_config_filename}")
    generated_configs.append(sys_config_filename)

# ==============================================================================
# 2. EXECUTION OF THE PIPELINE VIA BASH SCRIPT
# ==============================================================================
print("\n======================================================")
print("       STARTING EXECUTION VIA BASH SCRIPT             ")
print("======================================================")

for config_path in generated_configs:
    print(f"\n---> RUNNING VIA BASH: {config_path} <---")
    
    # Build the command that calls your bash script
    bash_command = f"{BASH_SCRIPT} {config_path}"
    
    try:
        subprocess.run(bash_command, shell=True, check=True)
        print(f"\n[SUCCESS] Run associated with '{config_path}' completed.")
        
    except subprocess.CalledProcessError as e:
        print(f"\n[FATAL ERROR] Bash script failed. Exit code: {e.returncode}")
        print("-> Interrupting the entire pipeline.")
        break 

print("\n======================================================")
print(" [INFO] Systematics pipeline finished.")
print("======================================================")