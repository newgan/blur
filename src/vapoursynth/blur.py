import vapoursynth as vs
from vapoursynth import core

# from vsrife import RIFE

# import adjust

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

if vars().get("linux_bundled") == "true":
    # load plugins
    plugin_dir = Path("../vapoursynth-plugins")
    ignored = {
        "libbestsource.so",
    }

    for dylib in plugin_dir.glob("*.so"):
        if dylib.name not in ignored:
            print("loading", dylib.name)
            core.std.LoadPlugin(path=str(dylib))

# add blur.py folder to path so it can reference scripts
sys.path.insert(1, str(Path(__file__).parent))

import blur.blending
import blur.deduplicate
import blur.interpolate
import blur.weighting
import blur.adjust
import blur.utils as u


video_path = Path(vars().get("video_path"))

settings = json.loads(vars().get("settings"))

# validate some settings
interpolation_algorithm = u.coalesce(
    u.safe_int(settings["interpolation_algorithm"]),
    blur.interpolate.DEFAULT_ALGORITHM,
)

interpolation_blocksize = u.coalesce(
    u.safe_int(settings["interpolation_blocksize"]),
    blur.interpolate.DEFAULT_BLOCKSIZE,
)

interpolation_mask_area = u.coalesce(
    u.safe_int(settings["interpolation_mask_area"]),
    blur.interpolate.DEFAULT_MASKING,
)

if vars().get("lsmash") == "true":
    video = core.lsmas.LibavSMASHSource(
        source=video_path, prefer_hw=3 if settings["gpu_decoding"] else 0
    )
else:
    video = core.bs.VideoSource(source=video_path, cachemode=0)

if settings["deduplicate"] and settings["deduplicate_range"] != 0:
    deduplicate_range: int | None = int(settings["deduplicate_range"])
    if deduplicate_range == -1:  # -1 = infinite
        deduplicate_range = None

    try:
        deduplicate_threshold = float(settings["deduplicate_threshold"])
    except (ValueError, TypeError, KeyError):
        deduplicate_threshold = 0.001

    match settings["deduplicate_method"]:
        case "old":
            video = blur.deduplicate.fill_drops_old(
                video,
                threshold=deduplicate_threshold,
                debug=settings["debug"],
            )

        case _:
            video = blur.deduplicate.fill_drops_multiple(
                video,
                threshold=deduplicate_threshold,
                max_frames=deduplicate_range,
                debug=settings["debug"],
                svp_preset=settings["interpolation_preset"],
                svp_algorithm=interpolation_algorithm,
                svp_blocksize=interpolation_blocksize,
                svp_masking=interpolation_mask_area,
                svp_gpu=settings["gpu_interpolation"],
            )

# input timescale
if settings["timescale"]:
    if settings["input_timescale"] != 1:
        video = core.std.AssumeFPS(
            video, fpsnum=(video.fps * (1 / settings["input_timescale"]))
        )

# interpolation
if settings["interpolate"]:
    # fix interpolated fps
    interpolated_fps = settings["interpolated_fps"]
    split = settings["interpolated_fps"].split("x")
    if len(split) > 1:
        # contains x, is a multiplier (e.g. 5x)
        interpolated_fps = video.fps * float(split[0])
    else:
        # no x, is an fps (e.g. 600)
        interpolated_fps = int(interpolated_fps)

    # TODO: rife

    orig_format = video.format
    needs_conversion = (
        orig_format.id != vs.YUV420P8
    )  # svp only accepts yv12 (SVSuper: Clip must be YV12)

    if needs_conversion:
        video = core.resize.Bicubic(video, format=vs.YUV420P8)

    if settings["manual_svp"]:
        super = core.svp1.Super(video, settings["super_string"])
        vectors = core.svp1.Analyse(
            super["clip"], super["data"], video, settings["vectors_string"]
        )

        # insert interpolated fps
        smooth_json = json.loads(settings["smooth_string"])
        if "rate" not in smooth_json:
            smooth_json["rate"] = {"num": interpolated_fps, "abs": True}
        smooth_str = json.dumps(smooth_json)

        video = core.svp2.SmoothFps(
            video,
            super["clip"],
            super["data"],
            vectors["clip"],
            vectors["data"],
            smooth_str,
        )
    else:
        video = blur.interpolate.interpolate_svp(
            video,
            new_fps=interpolated_fps,
            preset=settings["interpolation_preset"],
            algorithm=interpolation_algorithm,
            blocksize=interpolation_blocksize,
            overlap=0,
            masking=interpolation_mask_area,
            gpu=settings["gpu_interpolation"],
        )

    if needs_conversion:
        video = core.resize.Bicubic(video, format=orig_format.id)

# output timescale
if settings["timescale"]:
    if settings["output_timescale"] != 1:
        video = core.std.AssumeFPS(
            video, fpsnum=(video.fps * settings["output_timescale"])
        )

# blurring
if settings["blur"]:
    if settings["blur_amount"] > 0:
        frame_gap = int(video.fps / settings["blur_output_fps"])
        blended_frames = int(frame_gap * settings["blur_amount"])

        if blended_frames > 0:
            # number of weights must be odd
            if blended_frames % 2 == 0:
                blended_frames += 1

            def do_weighting_fn(blur_weighting_fn):
                blur_weighting_bound = json.loads(settings["blur_weighting_bound"])

                match blur_weighting_fn:
                    case "gaussian":
                        return blur.weighting.gaussian(
                            blended_frames,
                            settings["blur_weighting_gaussian_std_dev"],
                            blur_weighting_bound,
                        )

                    case "gaussian_sym":
                        return blur.weighting.gaussian_sym(
                            blended_frames,
                            settings["blur_weighting_gaussian_std_dev"],
                            blur_weighting_bound,
                        )

                    case "pyramid":
                        return blur.weighting.pyramid(
                            blended_frames,
                            bool(settings["blur_weighting_triangle_reverse"]),
                        )

                    case "pyramid_sym":
                        return blur.weighting.pyramid_sym(blended_frames)

                    case "custom_weight":
                        return blur.weighting.divide(
                            blended_frames, settings["blur_weighting"]
                        )

                    case "custom_function":
                        return blur.weighting.custom(
                            blended_frames,
                            settings["blur_weighting"],
                            settings["blur_weighting_bound"],
                        )

                    case "equal":
                        return blur.weighting.equal(blended_frames)

                    case _:
                        # check if it's a custom weighting function
                        if blur_weighting_fn[0] == "[" and blur_weighting_fn[-1] == "]":
                            return do_weighting_fn("custom_weight")
                        else:
                            return do_weighting_fn("custom_function")

            weights = do_weighting_fn(settings["blur_weighting"])

            # frame blend
            # if vars().get("macos_bundled") == "true":
            #     video = blur.blending.average_expr1(video, weights)
            # else:
            video = blur.blending.average(video, weights)

    # if frame_gap > 0:
    #     video = core.std.SelectEvery(video, cycle=frame_gap, offsets=0)

    # set exact fps
    video = blur.interpolate.change_fps(video, settings["blur_output_fps"])

# filters
if settings["filters"]:
    if (
        settings["brightness"] != 1
        or settings["contrast"] != 1
        or settings["saturation"] != 1
    ):
        video = blur.adjust.Tweak(
            video,
            bright=settings["brightness"] - 1,
            cont=settings["contrast"],
            sat=settings["saturation"],
        )

video.set_output()
