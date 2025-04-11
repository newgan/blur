#include "utils.h"

std::string u::trim(std::string_view str) {
	str.remove_prefix(std::min(str.find_first_not_of(" \t\r\v\n"), str.size()));
	str.remove_suffix(std::min(str.size() - str.find_last_not_of(" \t\r\v\n") - 1, str.size()));

	return std::string(str);
}

std::string u::random_string(int len) {
	std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	std::random_device rd;
	std::mt19937 generator(rd());

	std::shuffle(str.begin(), str.end(), generator);

	return str.substr(0, len);
}

std::vector<std::string> u::split_string(std::string str, const std::string& delimiter) {
	std::vector<std::string> output;

	size_t pos = 0;
	while ((pos = str.find(delimiter)) != std::string::npos) {
		std::string token = str.substr(0, pos);
		output.push_back(token);
		str.erase(0, pos + delimiter.length());
	}

	output.push_back(str);

	return output;
}

// NOLINTBEGIN gpt ass code
std::wstring u::towstring(const std::string& str) {
	if (str.empty())
		return std::wstring();

#ifdef _WIN32
	// Windows-specific implementation
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size_needed);
	return result;
#else
	// POSIX systems (Linux, macOS, etc.)
	std::vector<wchar_t> buf(str.size() + 1);
	std::mbstowcs(&buf[0], str.c_str(), str.size() + 1);
	return std::wstring(&buf[0]);
#endif
}

std::string u::tostring(const std::wstring& wstr) {
	if (wstr.empty()) {
		return std::string();
	}

#ifdef _WIN32
	// Windows-specific implementation
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, nullptr, nullptr);
	return result;
#else
	// POSIX systems (Linux, macOS, etc.)
	std::vector<char> buf((wstr.size() + 1) * MB_CUR_MAX);
	size_t converted = std::wcstombs(&buf[0], wstr.c_str(), buf.size());
	if (converted == static_cast<size_t>(-1)) {
		return std::string(); // Conversion failed
	}
	return std::string(&buf[0], converted);
#endif
}

// NOLINTEND

std::string u::to_lower(const std::string& str) {
	std::string out = str;

	std::ranges::for_each(out, [](char& c) {
		c = std::tolower(c);
	});

	return out;
}

std::optional<std::filesystem::path> u::get_program_path(const std::string& program_name) {
	namespace bp = boost::process;
	namespace fs = boost::filesystem;

	fs::path program_path = bp::search_path(program_name);

	std::filesystem::path path(program_path.string());

	if (!std::filesystem::exists(path))
		return {};

	return path;
}

// NOLINTBEGIN gpt ass code
std::string u::get_executable_path() {
#if defined(_WIN32)
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	return std::string(path);
#elif defined(__linux__)
	char path[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
	return std::string(path, (count > 0) ? count : 0);
#elif defined(__APPLE__)
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size); // Get the required size
	std::vector<char> path(size);
	if (_NSGetExecutablePath(path.data(), &size) == 0) {
		return std::string(path.data());
	}
	return "";
#else
#	error "Unsupported platform"
#endif
}

// NOLINTEND

float u::lerp(float value, float target, float reset_speed, float snap_offset) {
	value = std::lerp(value, target, reset_speed);

	if (std::abs(value - target) < snap_offset) // todo: is this too small
		value = target;

	return value;
}

constexpr int64_t PERIOD = 1;
constexpr int64_t TOLERANCE = 1'020'000;
constexpr int64_t MAX_TICKS = PERIOD * 9'500;

void u::sleep(double seconds) {
#ifndef WIN32
	std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
#else // KILLLLL WINDOWS
	using namespace std;
	using namespace chrono;

	auto t = high_resolution_clock::now();
	auto target = t + nanoseconds(int64_t(seconds * 1e9));

	static HANDLE timer;
	if (!timer)
		timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

	int64_t maxTicks = PERIOD * 9'500;
	for (;;) {
		int64_t remaining = (target - t).count();
		int64_t ticks = (remaining - TOLERANCE) / 100;
		if (ticks <= 0)
			break;
		if (ticks > maxTicks)
			ticks = maxTicks;

		LARGE_INTEGER due;
		due.QuadPart = -ticks;
		SetWaitableTimerEx(timer, &due, 0, NULL, NULL, NULL, 0);
		WaitForSingleObject(timer, INFINITE);
		t = high_resolution_clock::now();
	}

	// spin
	while (high_resolution_clock::now() < target)
		YieldProcessor();
#endif
}

std::filesystem::path u::get_resources_path() {
#ifdef __APPLE__
	// Resources path if part of a macos bundle
	CFBundleRef bundle = CFBundleGetMainBundle();
	if (bundle) {
		CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(bundle);
		if (resources_url) {
			std::array<char, PATH_MAX> path{};
			if (CFURLGetFileSystemRepresentation(
					resources_url, static_cast<Boolean>(true), (UInt8*)path.data(), path.size() // NOLINT
				))
			{
				CFRelease(resources_url);
				return path.data();
			}
			CFRelease(resources_url);
		}
	}
#endif

	// binary path otherwise
	return std::filesystem::path(u::get_executable_path()).parent_path();
}

std::filesystem::path u::get_settings_path() {
	std::filesystem::path settings_path;
	const std::string app_name = "blur";

#if defined(_WIN32) || defined(_WIN64)
	// Windows: %APPDATA%\blur
	const char* app_data = std::getenv("APPDATA");
	if (app_data) {
		settings_path = std::filesystem::path(app_data) / app_name;
	}
	else {
		// Fallback if APPDATA is not available
		const char* user_profile = std::getenv("USERPROFILE");
		if (user_profile) {
			settings_path = std::filesystem::path(user_profile) / "AppData" / "Roaming" / app_name;
		}
	}
#elif defined(__APPLE__)
	// macOS: ~/Library/Application Support/blur
	const char* home = std::getenv("HOME");
	if (home) {
		settings_path = std::filesystem::path(home) / "Library" / "Application Support" / app_name;
	}
#else
	// Linux/Unix: ~/.config/blur (XDG convention)
	const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
	if (xdg_config_home && xdg_config_home[0] != '\0') {
		settings_path = std::filesystem::path(xdg_config_home) / app_name;
	}
	else {
		const char* home = std::getenv("HOME");
		if (home) {
			settings_path = std::filesystem::path(home) / ".config" / app_name;
		}
	}
#endif

	// Create directories if they don't exist
	if (!settings_path.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(settings_path, ec);
	}

	return settings_path;
}

bool u::is_video_file(const std::filesystem::path& path) {
	namespace bp = boost::process;

	std::stringstream output;
	std::stringstream error;
	
	bp::environment env = boost::this_process::environment();

	if (blur.used_installer) {
#ifdef __linux__
		env["LD_LIBRARY_PATH"] = (blur.resources_path / "../lib").string();
#endif
	}

	bp::ipstream pipe_stream;
	bp::child c(
		blur.ffmpeg_path.wstring(),
		"-i",
		path.wstring(),
		bp::std_out > bp::null,
		bp::std_err > pipe_stream,
		env
#ifdef _WIN32
		,
		bp::windows::create_no_window
#endif
	);

	std::string line;
	bool has_video_stream = false;

	while (pipe_stream && std::getline(pipe_stream, line)) {
		boost::algorithm::to_lower(line);
		if (line.find("video:") != std::string::npos) {
			has_video_stream = true;
			break;
		}
	}

	c.wait(); // wait for ffmpeg to finish

	return has_video_stream;
}
