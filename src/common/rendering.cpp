#include "rendering.h"
#include "config_presets.h"
#include "config_blur.h"
#include "config_app.h"

tl::expected<nlohmann::json, std::string> rendering::detail::merge_settings(
	const BlurSettings& blur_settings, const GlobalAppSettings& app_settings
) {
	auto settings_json = blur_settings.to_json();
	if (!settings_json)
		return settings_json;

	auto app_json = app_settings.to_json();
	if (!app_json)
		return tl::unexpected(app_json.error());

	settings_json->update(*app_json);
	return settings_json;
}

std::vector<std::wstring> rendering::detail::build_base_vspipe_args(
	const std::filesystem::path& input_path, const nlohmann::json& merged_settings
) {
	auto path_str = input_path.wstring();
	std::ranges::replace(path_str, '\\', '/');

	std::vector<std::wstring> args = { L"-p",
		                               L"-c",
		                               L"y4m",
		                               L"-a",
		                               L"video_path=" + path_str,
		                               L"-a",
		                               L"settings=" + u::towstring(merged_settings.dump()) };

#ifdef __APPLE__
	args.insert(args.end(), { L"-a", std::format(L"macos_bundled={}", blur.used_installer ? L"true" : L"false") });
#endif
#ifdef _WIN32
	args.insert(args.end(), { L"-a", L"enable_lsmash=true" });
#endif
#ifdef __linux__
	bool bundled = std::filesystem::exists(blur.resources_path / "vapoursynth-plugins");
	args.insert(args.end(), { L"-a", std::format(L"linux_bundled={}", bundled ? L"true" : L"false") });
#endif

	args.insert(args.end(), { (blur.resources_path / "lib/blur.py").wstring(), L"-" });
	return args;
}

tl::expected<std::filesystem::path, std::string> rendering::detail::create_temp_output_path(
	const std::string& prefix, const std::string& extension
) {
	static std::atomic<size_t> counter = 0;
	auto temp_path = blur.create_temp_path(std::format("{}-{}", prefix, counter++));

	if (!temp_path)
		return tl::unexpected("Failed to create temp path");

	return *temp_path / std::format("output.{}", extension);
}

std::filesystem::path rendering::detail::build_output_filename(
	const std::filesystem::path& input_path, const BlurSettings& settings, const GlobalAppSettings& app_settings
) {
	auto output_folder = input_path.parent_path() / app_settings.output_prefix;
	std::filesystem::create_directories(output_folder);

	std::string base_name = input_path.stem().string() + " - blur";

	if (settings.detailed_filenames) {
		std::string details;
		if (settings.blur && settings.interpolate) {
			details = std::format(
				"{}fps ({}, {})", settings.blur_output_fps, settings.interpolated_fps, settings.blur_amount
			);
		}
		else if (settings.blur) {
			details = std::format("{}fps ({})", settings.blur_output_fps, settings.blur_amount);
		}
		else if (settings.interpolate) {
			details = std::format("{}fps", settings.interpolated_fps);
		}

		if (!details.empty())
			base_name += " ~ " + details;
	}

	// find unique filename
	int counter = 1;
	std::filesystem::path result;
	do {
		std::string filename = base_name;
		if (counter > 1)
			filename += std::format(" ({})", counter);
		filename += "." + settings.advanced.video_container;
		result = output_folder / filename;
		counter++;
	}
	while (std::filesystem::exists(result));

	return result;
}

std::vector<std::wstring> rendering::detail::build_color_metadata_args(const u::VideoInfo& video_info) {
	std::vector<std::string> params;

	if (video_info.color_range) {
		std::string range = *video_info.color_range == "pc" ? "full" : "limited";
		params.emplace_back("range=" + range);
	}
	if (video_info.color_space)
		params.emplace_back("colorspace=" + *video_info.color_space);
	if (video_info.color_transfer)
		params.emplace_back("color_trc=" + *video_info.color_transfer);
	if (video_info.color_primaries)
		params.emplace_back("color_primaries=" + *video_info.color_primaries);

	if (params.empty())
		return std::vector<std::wstring>{};

	std::string filter =
		"setparams=" +
		std::accumulate(
			std::next(params.begin()), params.end(), params[0], [](const std::string& a, const std::string& b) {
				return a + ":" + b;
			}
		);

	std::vector<std::wstring> result = { L"-vf", u::towstring(filter) };

	if (video_info.pix_fmt) {
		result.insert(result.end(), { L"-pix_fmt", u::towstring(*video_info.pix_fmt) });
	}

	return result;
}

std::vector<std::wstring> rendering::detail::build_audio_filter_args(
	const BlurSettings& settings, const u::VideoInfo& video_info
) {
	if (!settings.timescale)
		return std::vector<std::wstring>{};

	std::vector<std::wstring> filters;
	int sample_rate = video_info.sample_rate != -1 ? video_info.sample_rate : 48000;

	if (settings.input_timescale != 1.0f) {
		filters.push_back(std::format(L"asetrate={}*{}", sample_rate, 1.0f / settings.input_timescale));
		filters.push_back(L"aresample=48000");
	}

	if (settings.output_timescale != 1.0f) {
		if (settings.output_timescale_audio_pitch) {
			filters.push_back(std::format(L"asetrate={}*{}", sample_rate, settings.output_timescale));
			filters.push_back(L"aresample=48000");
		}
		else {
			filters.push_back(std::format(L"atempo={}", settings.output_timescale));
		}
	}

	if (filters.empty())
		return std::vector<std::wstring>{};

	std::wstring combined = std::accumulate(
		std::next(filters.begin()), filters.end(), filters[0], [](const std::wstring& a, const std::wstring& b) {
			return a + L"," + b;
		}
	);

	return std::vector<std::wstring>{ L"-af", combined };
}

std::vector<std::wstring> rendering::detail::build_encoding_args(
	const BlurSettings& settings, const GlobalAppSettings& app_settings
) {
	if (!settings.advanced.ffmpeg_override.empty()) {
		auto args = u::ffmpeg_string_to_args(settings.advanced.ffmpeg_override);
		std::vector<std::wstring> result;
		result.reserve(args.size());
		for (const auto& arg : args) {
			result.push_back(u::towstring(arg));
		}
		return result;
	}

	auto preset_args = config_presets::get_preset_params(
		settings.gpu_encoding ? app_settings.gpu_type : "cpu",
		u::to_lower(settings.encode_preset.empty() ? "h264" : settings.encode_preset),
		settings.quality
	);

	std::vector<std::wstring> result;
	result.reserve(preset_args.size());
	for (const auto& arg : preset_args) {
		result.push_back(u::towstring(arg));
	}

	result.insert(result.end(), { L"-c:a", L"aac", L"-b:a", L"320k", L"-movflags", L"+faststart" });
	return result;
}

void rendering::detail::pause(int pid, const std::shared_ptr<RenderState>& state) {
	if (state->paused)
		return;

	if (pid > 0) {
#ifdef WIN32
		u::windows_toggle_suspend_process(pid, true);
#else
		kill(pid, SIGSTOP);
#endif
	}
	{
		std::lock_guard lock(state->mutex);
		state->paused = true;
	}

	state->progress.fps_initialised = false;
	state->progress.fps = 0.f;

	u::log("Render paused");
}

void rendering::detail::resume(int pid, const std::shared_ptr<RenderState>& state) {
	if (!state->paused)
		return;

	if (pid > 0) {
#ifdef WIN32
		u::windows_toggle_suspend_process(pid, false);
#else
		kill(pid, SIGCONT);
#endif
	}

	{
		std::lock_guard lock(state->mutex);
		state->paused = false;
	}

	u::log("Render resumed");
}

tl::expected<rendering::detail::PipelineResult, std::string> rendering::detail::execute_pipeline(
	const RenderCommands& commands,
	const std::shared_ptr<RenderState>& state,
	bool debug,
	const std::function<void()>& progress_callback
) {
	namespace bp = boost::process;

	if (debug) {
		DEBUG_LOG("VSPipe: {} {}", blur.vspipe_path, u::tostring(u::join(commands.vspipe, L" ")));
		DEBUG_LOG("FFmpeg: {} {}", blur.ffmpeg_path, u::tostring(u::join(commands.ffmpeg, L" ")));
	}

	try {
		auto env = u::setup_vspipe_environment();

		bp::pipe vspipe_stdout;
		bp::ipstream vspipe_stderr;
		bp::ipstream ffmpeg_stderr;

		bp::child vspipe_process(
			boost::filesystem::path{ blur.vspipe_path },
			bp::args(commands.vspipe),
			bp::std_out > vspipe_stdout,
			bp::std_err > vspipe_stderr,
			env
#ifdef _WIN32
			,
			bp::windows::create_no_window
#endif
		);

		bp::child ffmpeg_process(
			boost::filesystem::path{ blur.ffmpeg_path },
			bp::args(commands.ffmpeg),
			bp::std_err > ffmpeg_stderr,
			bp::std_in < vspipe_stdout,
			bp::std_out.null(),
			env
#ifdef _WIN32
			,
			bp::windows::create_no_window
#endif
		);

		std::ostringstream vspipe_errors;
		std::ostringstream ffmpeg_errors;

		std::jthread progress_thread([&](const std::stop_token& stop_token) {
			std::string line;

			std::string progress_line;
			char ch = 0;

			while (!stop_token.stop_requested() && ffmpeg_process.running() && vspipe_stderr.get(ch)) {
				if (ch == '\n') {
					// Handle full line for logging
					vspipe_errors << line << '\n';
					line.clear();
				}
				else if (ch == '\r') {
					// Handle progress update
					static std::regex frame_regex(R"(Frame: (\d+)\/(\d+)(?: \((\d+\.\d+) fps\))?)");

					std::smatch match;
					if (std::regex_match(line, match, frame_regex)) {
						{
							std::lock_guard lock(state->mutex);

							state->progress.current_frame = std::stoi(match[1]);
							state->progress.total_frames = std::stoi(match[2]);
							state->progress.rendered_a_frame = true;

							float progress = state->progress.current_frame / (float)state->progress.total_frames;

							if (!state->progress.fps_initialised) {
								state->progress.fps_initialised = true;
								state->progress.start_time = std::chrono::steady_clock::now();
								state->progress.start_frame = state->progress.current_frame;
								state->progress.fps = 0.f;

								state->progress.string = std::format(
									"{:.1f}% complete ({}/{})",
									progress * 100,
									state->progress.current_frame,
									state->progress.total_frames
								);
							}
							else {
								auto current_time = std::chrono::steady_clock::now();
								state->progress.elapsed_time = current_time - state->progress.start_time;

								state->progress.fps = (state->progress.current_frame - state->progress.start_frame) /
								                      state->progress.elapsed_time.count();

								state->progress.string = std::format(
									"{:.1f}% complete ({}/{}, {:.2f} fps)",
									progress * 100,
									state->progress.current_frame,
									state->progress.total_frames,
									state->progress.fps
								);
							}

							u::log(state->progress.string);
						}

						if (progress_callback)
							progress_callback();
					}

					// Don't clear the line for logging purposes
					progress_line = line;
					line.clear();
				}
				else {
					line += ch; // Append character to the line
				}
			}

			// Process any remaining data in the pipe
			std::string remaining;
			while (std::getline(vspipe_stderr, remaining)) {
				vspipe_errors << remaining << '\n';
			}
		});

		std::jthread stderr_thread([&](const std::stop_token& stop_token) {
			std::string line;
			while (!stop_token.stop_requested() && std::getline(ffmpeg_stderr, line)) {
				ffmpeg_errors << line << '\n';
			}
		});

		bool killed = false;
		while (vspipe_process.running() || ffmpeg_process.running()) {
			if (state->to_stop) {
				vspipe_process.terminate();
				ffmpeg_process.terminate();
				killed = true;
				u::debug_log("STOPPING RENDER EARLY");
				break;
			}

			if (state->to_pause != state->paused) {
				auto fn = state->to_pause ? pause : resume;
				fn(vspipe_process.id(), state);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		if (killed)
			return PipelineResult{ .stopped = true };

		if ((!commands.vspipe_will_stop_early && vspipe_process.exit_code() != 0) || ffmpeg_process.exit_code() != 0) {
			return tl::unexpected(vspipe_errors.str() + ffmpeg_errors.str());
		}

		return PipelineResult{ .stopped = false };
	}
	catch (const std::exception& e) {
		return tl::unexpected(std::string(e.what()));
	}
}

void rendering::detail::copy_file_timestamp(const std::filesystem::path& from, const std::filesystem::path& to) {
	try {
		auto timestamp = std::filesystem::last_write_time(from);
		std::filesystem::last_write_time(to, timestamp);
	}
	catch (const std::exception& e) {
		u::log_error("Failed to copy timestamp: {}", e.what());
	}
}

tl::expected<rendering::RenderResult, std::string> rendering::render_frame(
	const std::filesystem::path& input_path,
	const BlurSettings& settings,
	const GlobalAppSettings& app_settings,
	const std::shared_ptr<RenderState>& state
) {
	if (!blur.initialised)
		return tl::unexpected("Blur not initialised");

	if (!std::filesystem::exists(input_path))
		return tl::unexpected("Input path does not exist");

	auto merged_settings = detail::merge_settings(settings, app_settings);
	if (!merged_settings)
		return tl::unexpected(merged_settings.error());

	auto output_path = detail::create_temp_output_path("frame-preview");
	if (!output_path)
		return tl::unexpected(output_path.error());

	RenderCommands commands = { .vspipe = detail::build_base_vspipe_args(input_path, *merged_settings),
		                        .ffmpeg = { L"-loglevel",
		                                    L"error",
		                                    L"-hide_banner",
		                                    L"-stats",
		                                    L"-ss",
		                                    L"00:00:00.200",
		                                    L"-y",
		                                    L"-i",
		                                    L"-",
		                                    L"-vframes",
		                                    L"1",
		                                    L"-q:v",
		                                    L"2",
		                                    L"-y",
		                                    output_path->wstring(), }, .vspipe_will_stop_early = true, };

	auto pipeline_result = detail::execute_pipeline(commands, state, settings.advanced.debug, nullptr);
	if (!pipeline_result)
		return tl::unexpected(pipeline_result.error());

	return RenderResult{ .output_path = *output_path, .stopped = pipeline_result->stopped };
}

tl::expected<rendering::RenderResult, std::string> rendering::detail::render_video(
	const std::filesystem::path& input_path,
	const u::VideoInfo& video_info,
	const BlurSettings& settings,
	const std::shared_ptr<RenderState>& state,
	const GlobalAppSettings& app_settings,
	const std::optional<std::filesystem::path>& output_path_override,
	const std::function<void()>& progress_callback
) {
	if (!blur.initialised)
		return tl::unexpected("Blur not initialised");
	if (!std::filesystem::exists(input_path))
		return tl::unexpected("Input path does not exist");

	auto merged_settings = detail::merge_settings(settings, app_settings);
	if (!merged_settings)
		return tl::unexpected(merged_settings.error());

	auto output_path = output_path_override.value_or(detail::build_output_filename(input_path, settings, app_settings));

	u::log("Rendering '{}'", input_path.stem().string());

	if (blur.verbose) {
		u::log("Source video at {:.2f} timescale", settings.input_timescale);
		if (settings.interpolate) {
			u::log("Interpolated to {}fps with {:.2f} timescale", settings.interpolated_fps, settings.output_timescale);
		}
		if (settings.blur) {
			u::log(
				"Motion blurred to {}fps ({}%)", settings.blur_output_fps, static_cast<int>(settings.blur_amount * 100)
			);
		}
		u::log("Rendered at {:.2f} speed with crf {}", settings.output_timescale, settings.quality);
	}

	// build vspipe command with video info
	auto vspipe_args = detail::build_base_vspipe_args(input_path, *merged_settings);
	vspipe_args.insert(
		vspipe_args.end() - 2,
		{ // insert before script path and "-"
	      L"-a",
	      std::format(L"fps_num={}", video_info.fps_num),
	      L"-a",
	      std::format(L"fps_den={}", video_info.fps_den),
	      L"-a",
	      L"color_range=" + (video_info.color_range ? u::towstring(*video_info.color_range) : L"undefined") }
	);

	// build ffmpeg command
	std::vector<std::wstring> ffmpeg_args = { L"-loglevel",
		                                      L"error",
		                                      L"-hide_banner",
		                                      L"-stats",
		                                      L"-y",
		                                      L"-i",
		                                      L"-", // piped video
		                                      L"-fflags",
		                                      L"+genpts",
		                                      L"-i",
		                                      input_path.wstring(), // original for audio
		                                      L"-map",
		                                      L"0:v",
		                                      L"-map",
		                                      L"1:a?" };

	// add color metadata
	auto color_args = detail::build_color_metadata_args(video_info);
	ffmpeg_args.insert(ffmpeg_args.end(), color_args.begin(), color_args.end());

	// add audio filters
	auto audio_args = detail::build_audio_filter_args(settings, video_info);
	ffmpeg_args.insert(ffmpeg_args.end(), audio_args.begin(), audio_args.end());

	// add encoding settings
	auto encoding_args = detail::build_encoding_args(settings, app_settings);
	ffmpeg_args.insert(ffmpeg_args.end(), encoding_args.begin(), encoding_args.end());

	ffmpeg_args.push_back(output_path.wstring());

	// add preview output if needed
	std::filesystem::path preview_path;
	if (settings.preview && blur.using_preview) {
		auto temp_preview = detail::create_temp_output_path("preview");
		if (temp_preview) {
			{
				std::lock_guard lock(state->mutex);
				state->preview_path = temp_preview.value();
			}

			ffmpeg_args.insert(
				ffmpeg_args.end(),
				{
					L"-map",
					L"0:v",
					L"-q:v",
					L"2",
					L"-update",
					L"1",
					L"-atomic_writing",
					L"1",
					L"-y",
					state->preview_path.wstring(),
				}
			);
		}
	}

	RenderCommands commands = {
		.vspipe = vspipe_args,
		.ffmpeg = ffmpeg_args,
		.vspipe_will_stop_early = false,
	};

	auto pipeline_result = detail::execute_pipeline(commands, state, settings.advanced.debug, progress_callback);
	if (!pipeline_result)
		return tl::unexpected(pipeline_result.error());

	if (pipeline_result->stopped) {
		std::filesystem::remove(output_path);
		u::log("Stopped render '{}'", input_path.stem().string());
	}
	else {
		if (settings.copy_dates) {
			detail::copy_file_timestamp(input_path, output_path);
		}
		if (blur.verbose) {
			u::log("Finished rendering '{}'", input_path.stem().string());
		}
	}

	// clean up preview temp path if created
	if (!preview_path.empty()) {
		Blur::remove_temp_path(preview_path.parent_path());
	}

	return RenderResult{ .output_path = output_path, .stopped = pipeline_result->stopped };
}

rendering::QueueAddRes rendering::RenderQueue::add(
	const std::filesystem::path& input_path,
	const u::VideoInfo& video_info,
	const std::optional<std::filesystem::path>& config_path,
	const GlobalAppSettings& app_settings,
	const std::optional<std::filesystem::path>& output_path_override,
	const std::function<void()>& progress_callback,
	const std::function<
		void(const QueuedRender& render, const tl::expected<rendering::RenderResult, std::string>& result)>&
		finish_callback
) {
	// parse config file (do it now, not when rendering. nice for batch rendering the same file with different
	// settings)
	auto config_res = config_blur::get_config(
		config_path.has_value() ? config_path.value() : config_blur::get_config_filename(input_path.parent_path()),
		!config_path.has_value() // use global only if no config path is specified
	);

	std::lock_guard lock(m_mutex);
	auto added = m_queue.emplace_back(
		input_path,
		video_info,
		config_res.config,
		app_settings,
		output_path_override,
		progress_callback,
		finish_callback
	);

	return {
		.is_global_config = config_res.is_global,
		.state = added.state,
	};
}
