"""Check the candidate compile graph and archive against admitted subsystems."""
import json
from pathlib import Path
import subprocess
import sys


def main():
    directory = Path(sys.argv[1]) if len(sys.argv) == 2 else Path("build/candidate")
    commands = json.loads((directory / "compile_commands.json").read_text())
    rejected = []
    forbidden_paths = ["/src/gpu/", "/src/codec/", "/src/encode/", "/src/android/",
                       "SkImageGeneratorCG.cpp", "SkFontMgr_mac", "SkTypeface_mac",
                       "SkTypeface_FreeType", "SkOpts_ml3", "SkOpts_ml4",
                       "skcms_TransformHsw", "skcms_TransformSkx",
                       "SkExecutor.cpp", "SkTaskGroup.cpp"]
    for command in commands:
        name = command["file"]
        for pattern in forbidden_paths:
            if pattern in name:
                rejected.append(name)
    symbols = subprocess.check_output(["llvm-nm", "--defined-only", "-C",
                                      str(directory / "libstx_skia.a")], text=True)
    for pattern in ["SkCodec::", "SkImageGeneratorCG::", "SkPngCodec::",
                    "SkFontMgr_New_CoreText", "SkTypeface_FreeType::",
                    "SkExecutor::", "SkTaskGroup::", "SkThreadPool<"]:
        if pattern in symbols:
            rejected.append(pattern)
    if rejected:
        raise RuntimeError("Outside compiled scope: " + repr(rejected))
    print("CPU pixel-only build scope: PASS")


if __name__ == "__main__":
    main()
