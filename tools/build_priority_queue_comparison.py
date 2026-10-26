"""Replace only the queue header for an attribution-controlled native comparison."""
import json
from pathlib import Path
import shlex
import subprocess


def main():
    root = Path.cwd()
    destination = root / "build/priority-queue-prior"
    override = destination / "include/src/core"
    override.mkdir(parents=True, exist_ok=True)
    original = root / "baseline/skia/src/core/SkTDPQueue.h"
    (override / original.name).write_bytes(original.read_bytes())
    commands = json.loads((root / "build/candidate/compile_commands.json").read_text())
    recorded = []
    for source, name in (("/tests/priority_queue.cpp", "test.o"),
                         ("/skia/src/utils/SkPolyUtils.cpp", "polygon.o")):
        entry = next(row for row in commands if row["file"].endswith(source))
        args = shlex.split(entry["command"])
        output_index = args.index("-o") + 1
        args[output_index] = str(destination / name)
        args[1:1] = ["-I" + str(destination / "include"), "-DSTX_SKIA_REFERENCE=1"]
        subprocess.run(args, cwd=entry["directory"], check=True)
        recorded.append(args)
    lines = subprocess.check_output(
        ["ninja", "-C", "build/candidate", "-t", "commands", "stx_skia_priority_queue_tests"],
        text=True).splitlines()
    args = shlex.split(lines[-1])
    assert args[:2] == [":", "&&"] and args[-2:] == ["&&", ":"]
    args = args[2:-2]
    test_object = "CMakeFiles/stx_skia_priority_queue_tests.dir/tests/priority_queue.cpp.o"
    position = args.index(test_object)
    args[position:position + 1] = [str(destination / "test.o"), str(destination / "polygon.o")]
    args[args.index("-o") + 1] = str(destination / "stx_skia_priority_queue_tests")
    subprocess.run(args, cwd=root / "build/candidate", check=True)
    recorded.append(args)
    (destination / "commands.json").write_text(json.dumps(recorded, indent=2) + "\n")
    prior = subprocess.check_output([str(destination / "stx_skia_priority_queue_tests")], text=True)
    current = subprocess.check_output(["build/candidate/stx_skia_priority_queue_tests"], text=True)
    assert prior == current
    print(prior.strip())
    print("Controlled comparison: only queue header differs; candidate array and polygon source retained")


if __name__ == "__main__":
    main()
