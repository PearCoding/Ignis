from api.utils import get_root_dir
import argparse
import os
import subprocess
import tempfile
import re
import math
import shutil

MSampleRegex = re.compile(r"\# ([0-9.]+)\/([0-9.]+)\/([0-9.]+)", re.M)

TempDir = None


def bench_exe(exe_path, gpu, args):
    tmp_path = os.path.join(TempDir, "_bench.exr")
    call_args = [exe_path, "--spp",
                 str(args.spp), "--gpu" if gpu else "--cpu", "-o", tmp_path, args.scene]

    if args.verbose:
        print(call_args)

    result = subprocess.run(call_args, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"Failed to run process {call_args}")
        exit(-2)

    if not args.quiet:
        print(result.stdout)

    m = MSampleRegex.findall(result.stdout)
    if m is None or len(m) != 1:
        print(result.stderr)
        exit(-3)

    return {"min": float(m[0][0]), "med": float(m[0][1]), "max": float(m[0][2])}


def reduce_avg(bench_results):
    # Apply geometric average
    min = 1
    med = 1
    max = 1

    for res in bench_results:
        min = min * res["min"]
        med = med * res["med"]
        max = max * res["max"]

    min = math.pow(min, 1.0/len(bench_results))
    med = math.pow(med, 1.0/len(bench_results))
    max = math.pow(max, 1.0/len(bench_results))

    return {"min": min, "med": med, "max": max}


def print_table(results):
    def format_f(val):
        if isinstance(val, float):
            return " {:.2f} ".format(val)
        else:
            return str(val)

    rows = len(results.keys())

    data = []  # Column major
    data.append(list(results.keys()))
    data.append([results[k]["min"] for k in results.keys()])
    data.append([results[k]["med"] for k in results.keys()])
    data.append([results[k]["max"] for k in results.keys()])

    col_w = [0, 0, 0, 0]
    for c in range(4):
        for i in range(len(data[c])):
            col_w[c] = max(col_w[c], len(format_f(data[c][i])))

    def print_filled(s, w, c=" "):
        print(format_f(s), end='')
        for _ in range(max(0, int(w) - len(format_f(s)))):
            print(c, end='')

    def col_sep():
        print("|", end='')

    print_filled("Target ", col_w[0])
    col_sep()
    print_filled(" Min ", col_w[1])
    col_sep()
    print_filled(" Med ", col_w[2])
    col_sep()
    print_filled(" Max ", col_w[3])
    print("")

    print_filled("", col_w[0], "-")
    col_sep()
    print_filled("", col_w[1], "-")
    col_sep()
    print_filled("", col_w[2], "-")
    col_sep()
    print_filled("", col_w[3], "-")
    print("")

    for r in range(rows):
        print_filled(data[0][r], col_w[0])
        col_sep()
        print_filled(data[1][r], col_w[1])
        col_sep()
        print_filled(data[2][r], col_w[2])
        col_sep()
        print_filled(data[3][r], col_w[3])
        print("")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--no-gpu', action="store_true",
                        help="Disable gpu evaluation")
    parser.add_argument('--no-cpu', action="store_true",
                        help="Disable cpu evaluation")
    parser.add_argument('-s', '--scene',
                        help="Scene to run benchmark on")
    parser.add_argument('-n', type=int, default=10,
                        help="Number of benchmark iterations")
    parser.add_argument('-w', type=int, default=2,
                        help="Number of warmup iterations")
    parser.add_argument('--spp', type=int, default=64,
                        help="Target spp")
    parser.add_argument('--verbose', action="store_true",
                        help="Set logging verbosity to debug")
    parser.add_argument('-q', '--quiet', action="store_true",
                        help="Make logging quiet")
    parser.add_argument('-e', '--executable', action='append',
                        type=str, help="Add executable to benchmark")

    args = parser.parse_args()

    root_dir = get_root_dir()
    if args.executable is None:
        exe_dir = os.path.join(root_dir, "build", "Release", "bin", "igcli")
        if (os.path.exists(exe_dir)):
            args.executable = [str(exe_dir)]
        else:
            exe_dir = os.path.join(root_dir, "build", "bin", "igcli")
            if (os.path.exists(exe_dir)):
                args.executable = [str(exe_dir)]

    if args.executable is None:
        print("No executable to benchmark given")
        exit(-1)

    if args.scene is None:
        scene_dir = os.path.join(root_dir, "scenes", "diamond_scene.json")
        if (os.path.exists(scene_dir)):
            args.scene = str(scene_dir)

    if args.scene is None:
        print("No scene to benchmark given")
        exit(-1)

    TempDir = tempfile.mkdtemp()

    table = {}
    for exe in args.executable:
        exe = os.path.abspath(exe)

        for gpu in range(2):
            if gpu == 0 and args.no_cpu:
                continue
            if gpu == 1 and args.no_gpu:
                continue

            for _ in range(args.w):
                bench_exe(exe, gpu == 1, args)  # Ignore output

            results = []
            for _ in range(args.n):
                results.append(bench_exe(exe, gpu == 1, args))

            avgs = reduce_avg(results)

            if gpu == 0:
                table[f"{exe} [CPU] "] = avgs
            else:
                table[f"{exe} [GPU] "] = avgs

    shutil.rmtree(TempDir)

    # Dump results
    print_table(table)
