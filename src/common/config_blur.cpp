#include "config_blur.h"
#include "config_base.h"

void config_blur::create(const std::filesystem::path& filepath, const BlurSettings& current_settings) {
	std::ofstream output(filepath);

	output << "[blur v" << BLUR_VERSION << "]" << "\n";

	output << "\n";
	output << "- blur" << "\n";
	output << "blur: " << (current_settings.blur ? "true" : "false") << "\n";
	output << "blur amount: " << current_settings.blur_amount << "\n";
	output << "blur output fps: " << current_settings.blur_output_fps << "\n";
	output << "blur weighting: " << current_settings.blur_weighting << "\n";

	output << "\n";
	output << "- interpolation" << "\n";
	output << "interpolate: " << (current_settings.interpolate ? "true" : "false") << "\n";
	output << "interpolated fps: " << current_settings.interpolated_fps << "\n";

	output << "\n";
	output << "- rendering" << "\n";
	output << "quality: " << current_settings.quality << "\n";
	output << "deduplicate: " << (current_settings.deduplicate ? "true" : "false") << "\n";
	output << "preview: " << (current_settings.preview ? "true" : "false") << "\n";
	output << "detailed filenames: " << (current_settings.detailed_filenames ? "true" : "false") << "\n";

	output << "\n";
	output << "- gpu acceleration" << "\n";
	output << "gpu interpolation: " << (current_settings.gpu_interpolation ? "true" : "false") << "\n";
	output << "gpu rendering: " << (current_settings.gpu_rendering ? "true" : "false") << "\n";
	output << "gpu type (nvidia/amd/intel): " << current_settings.gpu_type << "\n";

	output << "\n";
	output << "- timescale" << "\n";
	output << "timescale: " << (current_settings.timescale ? "true" : "false") << "\n";
	output << "input timescale: " << current_settings.input_timescale << "\n";
	output << "output timescale: " << current_settings.output_timescale << "\n";
	output << "adjust timescaled audio pitch: " << (current_settings.output_timescale_audio_pitch ? "true" : "false")
		   << "\n";

	output << "\n";
	output << "- filters" << "\n";
	output << "filters: " << (current_settings.filters ? "true" : "false") << "\n";
	output << "brightness: " << current_settings.brightness << "\n";
	output << "saturation: " << current_settings.saturation << "\n";
	output << "contrast: " << current_settings.contrast << "\n";

	output << "\n";
	output << "[advanced options]" << "\n";
	output << "advanced: " << (current_settings.advanced ? "true" : "false") << "\n";

	output << "\n";
	output << "- advanced rendering" << "\n";
	output << "video container: " << current_settings.video_container << "\n";
	output << "deduplicate range: " << current_settings.deduplicate_range << "\n";
	output << "deduplicate threshold: " << current_settings.deduplicate_threshold << "\n";
	output << "custom ffmpeg filters: " << current_settings.ffmpeg_override << "\n";
	output << "debug: " << (current_settings.debug ? "true" : "false") << "\n";

	output << "\n";
	output << "- advanced blur" << "\n";
	output << "blur weighting gaussian std dev: " << current_settings.blur_weighting_gaussian_std_dev << "\n";
	output << "blur weighting triangle reverse: "
		   << (current_settings.blur_weighting_triangle_reverse ? "true" : "false") << "\n";
	output << "blur weighting bound: " << current_settings.blur_weighting_bound << "\n";

	output << "\n";
	output << "- advanced interpolation" << "\n";
	output << "interpolation preset: " << current_settings.interpolation_preset << "\n";
	output << "interpolation algorithm: " << current_settings.interpolation_algorithm << "\n";
	output << "interpolation block size: " << current_settings.interpolation_blocksize << "\n";
	output << "interpolation mask area: " << current_settings.interpolation_mask_area << "\n";

	if (current_settings.manual_svp) {
		output << "\n";
		output << "- manual svp override" << "\n";
		output << "manual svp: " << (current_settings.manual_svp ? "true" : "false") << "\n";
		output << "super string: " << current_settings.super_string << "\n";
		output << "vectors string: " << current_settings.vectors_string << "\n";
		output << "smooth string: " << current_settings.smooth_string << "\n";
	}
}

config_blur::ConfigValidationResponse config_blur::validate(BlurSettings& config, bool fix) {
	std::set<std::string> errors;

	BlurSettings default_config;

	if (!u::contains(INTERPOLATION_PRESETS, config.interpolation_preset)) {
		errors.insert(std::format("Interpolation preset ({}) is not a valid option", config.interpolation_preset));

		if (fix)
			config.interpolation_preset = default_config.interpolation_preset;
	}

	if (!u::contains(INTERPOLATION_ALGORITHMS, config.interpolation_algorithm)) {
		errors.insert(std::format("Interpolation algorithm ({}) is not a valid option", config.interpolation_algorithm)
		);

		if (fix)
			config.interpolation_algorithm = default_config.interpolation_algorithm;
	}

	if (!u::contains(INTERPOLATION_BLOCK_SIZES, config.interpolation_blocksize)) {
		errors.insert(std::format("Interpolation block size ({}) is not a valid option", config.interpolation_blocksize)
		);

		if (fix)
			config.interpolation_blocksize = default_config.interpolation_blocksize;
	}

	return ConfigValidationResponse{
		.success = errors.empty(),
		.error = u::join(errors, " "),
	};
}

BlurSettings config_blur::parse(const std::filesystem::path& config_filepath) {
	auto config_map = config_base::read_config_map(config_filepath);

	BlurSettings settings;

	config_base::extract_config_value(config_map, "blur", settings.blur);
	config_base::extract_config_value(config_map, "blur amount", settings.blur_amount);
	config_base::extract_config_value(config_map, "blur output fps", settings.blur_output_fps);
	config_base::extract_config_string(config_map, "blur weighting", settings.blur_weighting);

	config_base::extract_config_value(config_map, "interpolate", settings.interpolate);
	config_base::extract_config_string(config_map, "interpolated fps", settings.interpolated_fps);

	config_base::extract_config_value(config_map, "filters", settings.filters);
	config_base::extract_config_value(config_map, "brightness", settings.brightness);
	config_base::extract_config_value(config_map, "saturation", settings.saturation);
	config_base::extract_config_value(config_map, "contrast", settings.contrast);

	config_base::extract_config_value(config_map, "quality", settings.quality);
	config_base::extract_config_value(config_map, "deduplicate", settings.deduplicate);
	config_base::extract_config_value(config_map, "preview", settings.preview);
	config_base::extract_config_value(config_map, "detailed filenames", settings.detailed_filenames);

	config_base::extract_config_value(config_map, "gpu interpolation", settings.gpu_interpolation);
	config_base::extract_config_value(config_map, "gpu rendering", settings.gpu_rendering);
	config_base::extract_config_string(config_map, "gpu type (nvidia/amd/intel)", settings.gpu_type);

	config_base::extract_config_value(config_map, "timescale", settings.timescale);
	config_base::extract_config_value(config_map, "input timescale", settings.input_timescale);
	config_base::extract_config_value(config_map, "output timescale", settings.output_timescale);
	config_base::extract_config_value(
		config_map, "adjust timescaled audio pitch", settings.output_timescale_audio_pitch
	);

	config_base::extract_config_value(config_map, "advanced", settings.advanced);

	config_base::extract_config_value(config_map, "video container", settings.video_container);
	config_base::extract_config_value(config_map, "deduplicate range", settings.deduplicate_range);
	config_base::extract_config_string(config_map, "deduplicate threshold", settings.deduplicate_threshold);
	config_base::extract_config_string(config_map, "custom ffmpeg filters", settings.ffmpeg_override);
	config_base::extract_config_value(config_map, "debug", settings.debug);

	config_base::extract_config_value(
		config_map, "blur weighting gaussian std dev", settings.blur_weighting_gaussian_std_dev
	);
	config_base::extract_config_value(
		config_map, "blur weighting triangle reverse", settings.blur_weighting_triangle_reverse
	);
	config_base::extract_config_string(config_map, "blur weighting bound", settings.blur_weighting_bound);

	config_base::extract_config_string(config_map, "interpolation preset", settings.interpolation_preset);
	config_base::extract_config_string(config_map, "interpolation algorithm", settings.interpolation_algorithm);
	config_base::extract_config_string(config_map, "interpolation block size", settings.interpolation_blocksize);
	config_base::extract_config_value(config_map, "interpolation mask area", settings.interpolation_mask_area);

	config_base::extract_config_value(config_map, "manual svp", settings.manual_svp);
	config_base::extract_config_string(config_map, "super string", settings.super_string);
	config_base::extract_config_string(config_map, "vectors string", settings.vectors_string);
	config_base::extract_config_string(config_map, "smooth string", settings.smooth_string);

	// recreate the config file using the parsed values (keeps nice formatting)
	create(config_filepath, settings);

	return settings;
}

BlurSettings config_blur::parse_global_config() {
	return parse(get_global_config_path());
}

std::filesystem::path config_blur::get_global_config_path() {
	return blur.settings_path / CONFIG_FILENAME;
}

std::filesystem::path config_blur::get_config_filename(const std::filesystem::path& video_folder) {
	return video_folder / CONFIG_FILENAME;
}

BlurSettings config_blur::get_global_config() {
	return config_base::load_config<BlurSettings>(get_global_config_path(), create, parse);
}

BlurSettings config_blur::get_config(const std::filesystem::path& config_filepath, bool use_global) {
	bool local_cfg_exists = std::filesystem::exists(config_filepath);

	auto global_cfg_path = get_global_config_path();
	bool global_cfg_exists = std::filesystem::exists(global_cfg_path);

	std::filesystem::path cfg_path;
	if (use_global && !local_cfg_exists && global_cfg_exists) {
		cfg_path = global_cfg_path;

		if (blur.verbose)
			u::log("Using global config");
	}
	else {
		// check if the config file exists, if not, write the default values
		if (!local_cfg_exists) {
			create(config_filepath);

			u::log(L"Configuration file not found, default config generated at {}", config_filepath.wstring());
		}

		cfg_path = config_filepath;
	}

	return parse(cfg_path);
}

nlohmann::json BlurSettings::to_json() const {
	nlohmann::json j;

	j["blur"] = this->blur;
	j["blur_amount"] = this->blur_amount;
	j["blur_output_fps"] = this->blur_output_fps;
	j["blur_weighting"] = this->blur_weighting;

	j["interpolate"] = this->interpolate;
	j["interpolated_fps"] = this->interpolated_fps;

	j["timescale"] = this->timescale;
	j["input_timescale"] = this->input_timescale;
	j["output_timescale"] = this->output_timescale;
	j["output_timescale_audio_pitch"] = this->output_timescale_audio_pitch;

	j["filters"] = this->filters;
	j["brightness"] = this->brightness;
	j["saturation"] = this->saturation;
	j["contrast"] = this->contrast;

	j["quality"] = this->quality;
	j["deduplicate"] = this->deduplicate;
	j["preview"] = this->preview;
	j["detailed_filenames"] = this->detailed_filenames;

	j["gpu_interpolation"] = this->gpu_interpolation;
	j["gpu_rendering"] = this->gpu_rendering;
	j["gpu_type"] = this->gpu_type;
	// j["video_container"] = this->video_container;
	j["deduplicate_range"] = this->deduplicate_range;
	j["deduplicate_threshold"] = this->deduplicate_threshold;
	// j["ffmpeg_override"] = this->ffmpeg_override;
	j["debug"] = this->debug;

	j["blur_weighting_gaussian_std_dev"] = this->blur_weighting_gaussian_std_dev;
	j["blur_weighting_triangle_reverse"] = this->blur_weighting_triangle_reverse;
	j["blur_weighting_bound"] = this->blur_weighting_bound;

	j["interpolation_program"] = this->interpolation_program;
	j["interpolation_preset"] = this->interpolation_preset;
	j["interpolation_algorithm"] = this->interpolation_algorithm;
	j["interpolation_blocksize"] = this->interpolation_blocksize;
	j["interpolation_mask_area"] = this->interpolation_mask_area;

	j["manual_svp"] = this->manual_svp;
	j["super_string"] = this->super_string;
	j["vectors_string"] = this->vectors_string;
	j["smooth_string"] = this->smooth_string;

	return j;
}
