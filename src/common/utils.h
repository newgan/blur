#pragma once

#ifdef _DEBUG
#	define DEBUG_LOG(...) u::log(__VA_ARGS__)
#else
#	define DEBUG_LOG(...) ((void)0)
#endif

#ifndef M_PI
#	define M_PI 3.1415926535897932384626433832
#endif

#define TRY(expr)                                                                                                      \
	({                                                                                                                 \
		auto _res = (expr);                                                                                            \
		if (!_res)                                                                                                     \
			return _res.error();                                                                                       \
		*_res;                                                                                                         \
	})

namespace u {
	inline void log(const std::string& msg) {
		std::cout << msg << '\n';
	}

	inline void log(const std::wstring& msg) {
		std::wcout << msg << L'\n';
	}

	template<typename... Args>
	void log(const std::format_string<Args...> format_str, Args&&... args) {
		std::cout << std::format(format_str, std::forward<Args>(args)...) << '\n';
	}

	template<typename... Args>
	void log(const std::wformat_string<Args...> format_str, Args&&... args) {
		std::wcout << std::format(format_str, std::forward<Args>(args)...) << L'\n';
	}

	inline void log_error(const std::string& msg) {
		std::cerr << msg << '\n';
	}

	inline void log_error(const std::wstring& msg) {
		std::wcerr << msg << L'\n';
	}

	template<typename... Args>
	void log_error(const std::format_string<Args...> format_str, Args&&... args) {
		std::cerr << std::format(format_str, std::forward<Args>(args)...) << '\n';
	}

	template<typename... Args>
	void log_error(const std::wformat_string<Args...> format_str, Args&&... args) {
		std::wcerr << std::format(format_str, std::forward<Args>(args)...) << L'\n';
	}

	// NOLINTBEGIN not my code bud
	template<typename container_type>
	struct enumerate_wrapper {
		using iterator_type = std::conditional_t<
			std::is_const_v<container_type>,
			typename container_type::const_iterator,
			typename container_type::iterator>;
		using pointer_type = std::conditional_t<
			std::is_const_v<container_type>,
			typename container_type::const_pointer,
			typename container_type::pointer>;
		using reference_type = std::conditional_t<
			std::is_const_v<container_type>,
			typename container_type::const_reference,
			typename container_type::reference>;

		constexpr enumerate_wrapper(container_type& c) : container(c) {}

		struct enumerate_wrapper_iter {
			size_t index;
			iterator_type value;

			constexpr bool operator!=(const iterator_type& other) const {
				return value != other;
			}

			constexpr enumerate_wrapper_iter& operator++() {
				++index;
				++value;
				return *this;
			}

			constexpr std::pair<size_t, reference_type> operator*() {
				return std::pair<size_t, reference_type>{ index, *value };
			}
		};

		constexpr enumerate_wrapper_iter begin() {
			return { 0, std::begin(container) };
		}

		constexpr iterator_type end() {
			return std::end(container);
		}

		container_type& container;
	};

	template<typename container_type>
	constexpr auto enumerate(container_type& c) {
		return enumerate_wrapper(c);
	}

	template<typename container_type>
	constexpr auto const_enumerate(const container_type& c) {
		return enumerate_wrapper(c);
	}

	// NOLINTEND

	template<typename Container>
	auto join(const Container& container, const typename Container::value_type::value_type* delimiter) {
		using CharT = typename Container::value_type::value_type;
		std::basic_ostringstream<CharT> result;

		auto it = container.begin();
		if (it != container.end()) {
			result << *it++; // Add the first element without a delimiter
		}
		while (it != container.end()) {
			result << delimiter << *it++;
		}
		return result.str();
	}

	template<std::ranges::range Container, typename T>
	requires(!std::same_as<std::remove_cvref_t<Container>, std::string>)
	bool contains(const Container& container, const T& value) {
		return std::ranges::find(container, value) != std::ranges::end(container);
	}

	inline bool contains(const std::string& str, std::string_view value) {
		return str.find(value) != std::string::npos;
	}

	inline std::string replace_all(std::string str, const std::string& from, const std::string& to) {
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length();
		}
		return str;
	}

	template<typename T>
	static constexpr T rad_to_deg(T radian) {
		return radian * (180.f / M_PI);
	}

	template<typename T>
	static constexpr T deg_to_rad(T degree) {
		return static_cast<T>(degree * (M_PI / 180.f));
	}

	std::string trim(std::string_view str);
	std::string random_string(int len);
	std::vector<std::string> split_string(std::string str, const std::string& delimiter);
	std::wstring towstring(const std::string& str);
	std::string tostring(const std::wstring& wstr);
	std::string to_lower(const std::string& str);
	std::string truncate_with_ellipsis(const std::string& input, std::size_t max_length);

	std::optional<std::filesystem::path> get_program_path(const std::string& program_name);

	std::string get_executable_path();

	float lerp(
		float value, float target, float speed, float snap_offset = 0.01f
	); // if animations are jumping at the end then lower snap offset. todo: maybe dynamically generate it somehow

	void sleep(double seconds); // https://blog.bearcats.nl/perfect-sleep-function/ kill windows

	std::filesystem::path get_resources_path();
	std::filesystem::path get_settings_path();

	struct VideoInfo {
		bool has_video_stream = false;
		std::optional<std::string> color_range;
		int sample_rate = -1;
		int fps_num = -1;
		int fps_den = -1;
	};

	VideoInfo get_video_info(const std::filesystem::path& path);

	struct EncodingDevice {
		std::string type;   // "nvidia", "amd", "intel", "mac"
		std::string method; // Specific encoding method (e.g., "nvenc", "amf", "qsv", "videotoolbox")
		bool is_primary;    // Whether this is likely the primary GPU
	};

	std::vector<EncodingDevice> get_hardware_encoding_devices();
	std::vector<std::string> get_available_gpu_types();
	std::string get_primary_gpu_type();

	std::vector<std::string> get_supported_presets(bool gpu_encoding, const std::string& gpu_type);

	std::vector<std::wstring> ffmpeg_string_to_args(const std::wstring& str);

	std::map<int, std::string> get_rife_gpus();
	int get_fastest_rife_gpu_index(
		const std::map<int, std::string>& gpu_map,
		const std::filesystem::path& rife_model_path,
		const std::filesystem::path& benchmark_video_path
	);

#ifdef WIN32
	bool windows_toggle_suspend_process(DWORD pid, bool to_suspend);
#endif
}
