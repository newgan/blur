import vapoursynth as vs
from vapoursynth import core

import sys
import json
from pathlib import Path

if vars().get("macos_bundled") == "true":
    # load plugins
    plugin_dir = Path("../vapoursynth-plugins")
    ignored = {
        "libbestsource.dylib",
    }

    for dylib in plugin_dir.glob("*.dylib"):
        if dylib.name not in ignored:
            print("loading", dylib.name)
            core.std.LoadPlugin(path=str(dylib))

# add blur.py folder to path so it can reference scripts
sys.path.insert(1, str(Path(__file__).parent))

import blur.interpolate

rife_model = str(vars().get("rife_model"))

video = core.std.BlankClip()

blur.interpolate.interpolate_rife(
    video, video.fps, model_path=rife_model, only_list_gpus=True
)

video.set_output()
