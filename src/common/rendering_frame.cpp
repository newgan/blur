#include "rendering_frame.h"

RenderCommands FrameRender::build_render_commands(
	const std::filesystem::path& input_path, const std::filesystem::path& output_path, const BlurSettings& settings
) {
	RenderCommands commands;

	std::wstring path_string = input_path.wstring();
	std::ranges::replace(path_string, '\\', '/');

	std::wstring blur_script_path = (blur.resources_path / "lib/blur.py").wstring();

	// Build vspipe command
	commands.vspipe = { L"-p",
		                L"-c",
		                L"y4m",
		                L"-a",
		                L"video_path=" + path_string,
		                L"-a",
		                L"settings=" + u::towstring(settings.to_json().dump()),
#if defined(__APPLE__)
		                L"-a",
		                std::format(L"macos_bundled={}", blur.used_installer ? L"true" : L"false"),
#elif defined(__linux__)
		                L"-a",
		                std::format(L"linux_bundled={}", true ? L"true" : L"false"), // todo: fix used_installer on linux
#endif
#ifdef WIN32
		                L"-a",
		                L"lsmash=true",
#endif
		                blur_script_path,
		                L"-" };

	// Build ffmpeg command
	// clang-format off
	commands.ffmpeg = {
		L"-loglevel",
		L"error",
		L"-hide_banner",
		L"-stats",
		L"-ss",
		L"00:00:00.200", // skip forward a bit because blur needs context todo: how low can this go?
		L"-y",
		L"-i",
		L"-", // piped output from video script
		L"-vframes",
		L"1", // render 1 frame
		L"-y",
		output_path.wstring(),
	};
	// clang-format on

	return commands;
}

FrameRender::DoRenderResult FrameRender::do_render(RenderCommands render_commands, const BlurSettings& settings) {
	namespace bp = boost::process;

	std::ostringstream vspipe_stderr_output;

	try {
		boost::asio::io_context io_context;
		bp::pipe vspipe_stdout;
		bp::ipstream vspipe_stderr;

		if (settings.advanced.debug) {
			u::log(L"VSPipe command: {} {}", blur.vspipe_path.wstring(), u::join(render_commands.vspipe, L" "));
			u::log(L"FFmpeg command: {} {}", blur.ffmpeg_path.wstring(), u::join(render_commands.ffmpeg, L" "));
		}

		bp::environment env = boost::this_process::environment();

		if (blur.used_installer) {
#if defined(__APPLE__)
			env["PYTHONHOME"] = (blur.resources_path / "python").string();
			env["PYTHONPATH"] = (blur.resources_path / "python/lib/python3.12/site-packages").string();
#elif defined(__linux__)
			env["LD_LIBRARY_PATH"] = (blur.resources_path / "../lib").string();
			env["PYTHONHOME"] = (blur.resources_path / "python").string();
			env["PYTHONPATH"] = (blur.resources_path / "python/lib/python3.12/site-packages").string();
#endif
		}

		// Declare as local variables first, then move or assign
		auto vspipe_process = bp::child(
			blur.vspipe_path.wstring(),
			bp::args(render_commands.vspipe),
			bp::std_out > vspipe_stdout,
			bp::std_err > vspipe_stderr,
			env,
			io_context
#ifdef _WIN32
			,
			bp::windows::create_no_window
#endif
		);

		auto ffmpeg_process = bp::child(
			blur.ffmpeg_path.wstring(),
			bp::args(render_commands.ffmpeg),
			bp::std_in < vspipe_stdout,
			bp::std_out.null(),
			bp::std_err.null(),
			env,
			io_context
#ifdef _WIN32
			,
			bp::windows::create_no_window
#endif
		);

		std::thread vspipe_stderr_thread([&]() {
			std::string line;
			while (std::getline(vspipe_stderr, line)) {
				vspipe_stderr_output << line << '\n';
			}
		});

		vspipe_process.detach();
		ffmpeg_process.detach();

		while (vspipe_process.running() || ffmpeg_process.running()) {
			if (m_to_kill) {
				ffmpeg_process.terminate();
				vspipe_process.terminate();
				u::log("frame render: killed processes early");
				m_to_kill = false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		if (vspipe_stderr_thread.joinable()) {
			vspipe_stderr_thread.join();
		}

		if (settings.advanced.debug)
			u::log(
				"vspipe exit code: {}, ffmpeg exit code: {}", vspipe_process.exit_code(), ffmpeg_process.exit_code()
			);

		bool success =
			ffmpeg_process.exit_code() == 0; // vspipe_process.exit_code() == 0 && ffmpeg_process.exit_code() == 0;

		// todo: check why vspipe isnt returning 0

		if (!success)
			remove_temp_path();

		return {
			.success = success,
			.error_message = vspipe_stderr_output.str(),
		};
	}
	catch (const boost::system::system_error& e) {
		u::log_error("Process error: {}", e.what());

		return {
			.success = false,
			.error_message = e.what(),
		};
	}
}

bool FrameRender::create_temp_path() {
	static std::atomic<size_t> id = 0;
	auto temp_path = blur.create_temp_path(std::format("config-preview-{}", id++));

	if (!temp_path)
		return false;

	m_temp_path = *temp_path;
	return true;
}

bool FrameRender::remove_temp_path() {
	bool res = Blur::remove_temp_path(m_temp_path);
	if (res)
		m_temp_path.clear();
	return res;
}

FrameRender::RenderResponse FrameRender::render(const std::filesystem::path& input_path, const BlurSettings& settings) {
	if (!blur.initialised)
		return {
			.success = false,
			.error_message = "Blur not initialised",
		};

	if (!std::filesystem::exists(input_path)) {
		return {
			.success = false,
			.error_message = "Input path does not exist",
		};
	}

	if (!create_temp_path()) {
		u::log("failed to make temp path");
		return {
			.success = false,
			.error_message = "Failed to make temp path",
		};
	}

	std::filesystem::path output_path = m_temp_path / "render.png";

	// render
	RenderCommands render_commands = build_render_commands(input_path, output_path, settings);

	auto render_res = do_render(render_commands, settings);

	return {
		.success = render_res.success,
		.output_path = output_path,
		.error_message = render_res.error_message,
	};
}
