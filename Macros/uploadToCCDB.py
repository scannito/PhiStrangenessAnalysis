import argparse
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import ROOT  # pylint: disable=import-error

# Try to import yaml, but don't crash if it's missing unless the user tries to use a YAML file
try:
    import yaml
except ImportError:
    yaml = None

def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="A tool to upload efficiency to CCDB"
    )
    parser.add_argument(
        "--alien-path",
        type=Path,
        help="path to train run's directory in Alien with analysis results "
        "[example: /alice/cern.ch/user/a/alihyperloop/outputs/0033/332611/70301]",
    )
    parser.add_argument(
        "--mc-reco",
        type=str,
        nargs="+",
        help="paths to MC Reco histograms, separated by space [example: task/mcreco_one/hPt task/mcreco_two/hPt]",
        required=True,
    )
    parser.add_argument(
        "--mc-truth",
        type=str,
        nargs="+",
        help="paths to MC Truth histograms, separated by space [example: task/mctruth_one/hPt task/mctruth_one/hPt]",
        required=True,
    )
    parser.add_argument(
        "--ccdb-path",
        type=str,
        help="location in CCDB to where objects will be uploaded",
        required=True,
    )
    parser.add_argument(
        "--ccdb-url",
        type=str,
        help="URL to CCDB",
        default="http://ccdb-test.cern.ch:8080",
    )
    parser.add_argument(
        "--ccdb-labels",
        type=str,
        nargs="+",
        help="custom labels to add to objects' metadata in CCDB [example: label1 label2]",
        default=[],
    )
    parser.add_argument(
        "--ccdb-lifetime",
        type=int,
        help="how long should objects in CCDB remain valid (milliseconds)",
        default=365 * 24 * 60 * 60 * 1000,  # one year
    )
    return parser.parse_args()