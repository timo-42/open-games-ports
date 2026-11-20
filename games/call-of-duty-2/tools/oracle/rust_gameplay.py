#!/usr/bin/env python3
"""Run the native Rust server against the existing 1.3 multiplayer gameplay captures.

Requires Linux, bubblewrap, the stock IWDs and the three original captures per case.
Use gameplay.py run CASE N --server original to generate the reference runs.
Use --case repeatedly for a subset. Each invocation creates a fresh output directory.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))  # Include the adjacent harness when invoked with Python's isolated mode.
import gameplay  # noqa: E402

REPO = HERE.parents[3]
DATA = Path(os.environ["COD2_DATA_MAIN"]) if os.environ.get("COD2_DATA_MAIN") else None


def sha256(path):
    with path.open("rb") as content:
        return hashlib.file_digest(content, "sha256").hexdigest()


def validate_reference(directory, case):
    runs = sorted(path for path in directory.glob("run*") if path.is_dir())
    if len(runs) < 3:
        raise ValueError(f"{case} needs at least three original captures in {directory}")
    for run in runs:
        hashes = dict(line.split("  ", 1)[::-1] for line in (run / "SHA256SUMS").read_text().splitlines())
        for name in ("summary.json", "server.log", "games_mp.log"):
            if sha256(run / name) != hashes.get(name):
                raise ValueError(f"checksum mismatch for {run / name}")
        with (run / "summary.json").open() as content:
            summary = json.load(content)
        if (
            summary.get("case") != case
            or str(summary.get("seed")) != str(gameplay.DEFAULT_SEED)
            or not summary.get("clientA", {}).get("records")
        ):
            raise ValueError(f"{run} did not capture the requested seeded game")


def run_case(case, references, work, environment):
    refs = references / f"gameplay_{case}"
    report = {"case": case, "passed": False}
    try:
        validate_reference(refs, case)
        command = [sys.executable, "-I", str(HERE / "gameplay.py")]
        with (work / f"{case}.log").open("w") as log:
            capture = subprocess.run(
                command + ["run", case, "--server", "rust"], stdout=log, stderr=subprocess.STDOUT, env=environment
            )
            report["capture_exit_code"] = capture.returncode
            candidate = work / "rust-run" / "captures" / f"gameplay_{case}"
            result = candidate / "run1" / "summary.json"
            if not result.is_file():
                raise ValueError("Rust capture ended before the client wrote a summary")
            with result.open() as content:
                summary = json.load(content)
            report["client_records"] = {
                key: len(value["records"]) for key, value in summary.items() if key.startswith("client")
            }
            if "result" in summary:
                report["client_result"] = summary["result"]
            counts = work / f"{case}.counts.json"
            compare = subprocess.run(
                command + ["compare", str(refs), str(candidate), "--counts-json", str(counts)],
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            if not counts.is_file():
                raise ValueError("comparison ended before writing counts")
            report.update(json.loads(counts.read_text()))
            harness = (candidate / "run1" / "harness.log").read_text().splitlines()
            report["clean_exit"] = "server_exit_status=0" in harness
            report["passed"] = (
                capture.returncode == 0
                and compare.returncode == 0
                and report["clean_exit"]
                and report["client_records"].get("clientA", 0) > 0
            )
    except (KeyError, ValueError, OSError) as error:
        report["error"] = str(error)
    print(json.dumps(report), flush=True)
    return report


def main():
    gameplay.load_plugins()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="list all registered scenarios")
    parser.add_argument("--case", choices=sorted(gameplay.CASES), action="append")
    parser.add_argument("--binary", type=Path, default=REPO / "target/release/cod2-server")
    parser.add_argument("--main", type=Path, default=DATA)
    parser.add_argument(
        "--reference", type=Path, default=Path(os.environ.get("COD2_LOCAL", REPO / ".local/cod2")) / "oracle/captures"
    )
    parser.add_argument("--output", type=Path, default=REPO / ".local/cod2-gameplay")
    parser.add_argument("--jobs", type=int, default=1, help="max concurrent cases, each in a private network namespace")
    args = parser.parse_args()
    if args.list:
        print("\n".join(sorted(gameplay.CASES)))
        return 0
    if args.main is None:
        parser.error("set --main or COD2_DATA_MAIN to the original game's main directory")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if not args.binary.is_file() or not os.access(args.binary, os.X_OK):
        parser.error(f"missing server executable {args.binary}")
    if not (args.main / "iw_15.iwd").is_file() or not args.reference.is_dir() or not shutil.which("bwrap"):
        parser.error("the stock main directory, original captures and bubblewrap are required")
    cases = list(dict.fromkeys(args.case or sorted(gameplay.CASES)))
    args.output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="suite-", dir=args.output.resolve()))
    binary = work / "cod2-server"
    shutil.copy2(args.binary, binary)
    environment = dict(
        os.environ, COD2_LOCAL=str(work), COD2_RUST_BIN=str(binary), COD2_DATA_MAIN=str(args.main.resolve())
    )
    print(f"Captures and comparisons in {work}", flush=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        reports = list(pool.map(lambda case: run_case(case, args.reference.resolve(), work, environment), cases))
    report = {
        "binary_sha256": sha256(binary),
        "game_seed": gameplay.DEFAULT_SEED,
        "runs": reports,
        "passed": sum(item["passed"] for item in reports),
        "total": len(reports),
    }
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{report['passed']}/{report['total']} scenarios matched")
    return int(report["passed"] != len(reports))


if __name__ == "__main__":
    sys.exit(main())
