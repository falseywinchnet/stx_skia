"""Emit small native probes for the retained CPU priority queue."""
import json
from pathlib import Path
import shlex
import subprocess

SOURCE = '''#include "src/core/SkTDPQueue.h"
#include <cstdint>
struct Entry { int priority; int identity; uint64_t payload; };
static bool less(const Entry& a, const Entry& b) {
    const bool result = a.priority < b.priority;
    return result;
}
using Queue = SkTDPQueue<Entry, less>;
extern "C" void queue_insert(Queue& queue, Entry entry) { queue.insert(entry); }
extern "C" void queue_pop(Queue& queue) { queue.pop(); }
'''


def main():
    source = Path("build/priority_queue-probe.cpp").resolve()
    source.write_text(SOURCE)
    for profile in ("baseline", "candidate"):
        commands = json.loads(Path(f"build/{profile}/compile_commands.json").read_text())
        entry = next(row for row in commands if row["file"].endswith("/tests/priority_queue.cpp"))
        args = shlex.split(entry["command"])
        output = args.index("-o")
        del args[output:output + 2]
        args.remove("-c")
        args[args.index(entry["file"])] = str(source)
        args += ["-S", "-o", str(Path(f"build/priority_queue-{profile}.s").resolve())]
        subprocess.run(args, cwd=entry["directory"], check=True)
        print(profile, "native priority_queue probes retained")


if __name__ == "__main__":
    main()
