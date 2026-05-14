import argparse
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_INPUT = os.path.join(SCRIPT_DIR, "lcsc.txt")
DEFAULT_OUTPUT_BASE = os.path.join(SCRIPT_DIR, "lib", "lcsc")

def run_easyeda2kicad_from_file(input_file, output_base=DEFAULT_OUTPUT_BASE, python_exec="python"):
    input_file = os.path.expanduser(input_file)
    output_base = os.path.expanduser(output_base)
    python_exec = python_exec or "python"

    if not os.path.isfile(input_file):
        print(f"Error: File not found: {input_file}")
        return 2

    # If a simple name was provided, check PATH; if an absolute path, check that file exists.
    found = shutil.which(python_exec) if os.path.basename(python_exec) == python_exec else os.path.exists(python_exec)
    if not found:
        print(f"Warning: Python executable '{python_exec}' not found in PATH or as given path. Trying anyway.")

    # easyeda2kicad treats --output as <dir>/<basename> and appends .kicad_sym / .pretty/ / .3dshapes/.
    # If <output_base> already exists as a directory, the tool falls back to its default basename
    # ("easyeda2kicad") inside it — so only create the parent.
    os.makedirs(os.path.dirname(os.path.abspath(output_base)), exist_ok=True)

    with open(input_file, "r", encoding="utf-8") as f:
        # ignore blank lines and comments
        lines = [line.strip() for line in f if line.strip() and not line.lstrip().startswith("#")]

    if not lines:
        print("No LCSC IDs found in input file.")
        return 0

    for idx, lcsc_id in enumerate(lines, start=1):
        cmd = [
            python_exec,
            "-m", "easyeda2kicad",
            "--full",
            f"--lcsc_id={lcsc_id}",
            f"--output={output_base}",
        ]
        print(f"[{idx}/{len(lines)}] Running: {' '.join(cmd)}")
        try:
            subprocess.run(cmd, check=True)
        except subprocess.CalledProcessError as e:
            print(f"❌ Error processing {lcsc_id}: {e}")
        except FileNotFoundError as e:
            print(f"❌ Executable not found: {e}")
            return 3

    print("✅ All commands completed.")
    return 0

def main(argv=None):
    parser = argparse.ArgumentParser(description="Run easyeda2kicad for a list of LCSC IDs.")
    parser.add_argument("input_file", nargs="?", default=DEFAULT_INPUT,
                        help=f"Path to file with one LCSC ID per line (default: {DEFAULT_INPUT})")
    parser.add_argument("output_base", nargs="?", default=DEFAULT_OUTPUT_BASE,
                        help=("Output library basename path. easyeda2kicad appends .kicad_sym, "
                              ".pretty/, and .3dshapes/ to this — e.g. ./lib/lcsc produces "
                              f"./lib/lcsc.kicad_sym (default: {DEFAULT_OUTPUT_BASE})"))
    parser.add_argument("--python", dest="python_exec", default="python",
                        help="Python executable to use (default: 'python')")
    args = parser.parse_args(argv)

    return_code = run_easyeda2kicad_from_file(args.input_file, args.output_base, args.python_exec)
    sys.exit(return_code if isinstance(return_code, int) else 0)

if __name__ == "__main__":
    main()