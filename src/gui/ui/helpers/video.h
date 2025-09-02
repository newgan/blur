#pragma once

#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <utility>
#include <optional>

struct Seek {
	float time;
	bool exact;

	bool operator==(const Seek& other) const = default;
};

class VideoPlayer {
public:
	VideoPlayer()
		: m_wakeup_on_mpv_render_update(SDL_RegisterEvents(1)), m_wakeup_on_mpv_events(SDL_RegisterEvents(1)) {
		if (m_wakeup_on_mpv_render_update == static_cast<Uint32>(-1) ||
		    m_wakeup_on_mpv_events == static_cast<Uint32>(-1))
		{
			throw std::runtime_error("Could not register SDL events");
		}

		initialize_mpv();
		gen_fbo_texture();
	}

	VideoPlayer(const VideoPlayer&) = delete; // should not be copyable
	VideoPlayer(VideoPlayer&&) = delete;
	VideoPlayer& operator=(const VideoPlayer&) = delete;
	VideoPlayer& operator=(VideoPlayer&&) = delete;

	~VideoPlayer();

	void handle_key_press(SDL_Keycode key);

	void load_file(const std::filesystem::path& file_path);

	bool render(int w, int h);

	[[nodiscard]] GLuint get_frame_texture_for_render() const {
		return m_tex;
	}

	void handle_mpv_event(const SDL_Event& event, bool& redraw);

	[[nodiscard]] std::optional<std::pair<int, int>> get_video_dimensions() const;
	[[nodiscard]] std::optional<float> get_percent_pos() const;
	[[nodiscard]] std::optional<float> get_duration() const;

	[[nodiscard]] std::optional<bool> get_paused() const {
		return get_property<bool>("pause", MPV_FORMAT_FLAG);
	}

	[[nodiscard]] bool is_video_ready() const;

	void seek(float time, bool exact, bool pause) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queued_seek = Seek{
			.time = time,
			.exact = exact,
		};
	}

	void set_paused(bool paused) {
		run_command_async({ "set", "pause", paused ? "yes" : "no" });
	}

	void resume() {
		run_command_async({ "set", "pause", "no" });
	}

	void set_end(float percent) {
		m_end_percent = percent;
	}

	void set_start(float percent) {
		m_start_percent = percent;
	}

	std::optional<Seek> get_queued_seek() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_queued_seek;
	}

	// std::optional<Seek> get_seek() {
	// 	std::lock_guard<std::mutex> lock(m_mutex);
	// 	if (m_queued_seek)
	// 		return m_queued_seek;
	// 	return m_last_seek;
	// }

private:
	mpv_handle* m_mpv = nullptr;
	mpv_render_context* m_mpv_gl = nullptr;
	Uint32 m_wakeup_on_mpv_render_update;
	Uint32 m_wakeup_on_mpv_events;

	GLuint m_fbo{};
	GLuint m_tex{};

	int m_current_width{};
	int m_current_height{};

	bool m_video_loaded{};

	std::mutex m_mutex;
	std::optional<Seek> m_queued_seek;
	std::optional<Seek> m_last_seek;

	std::thread m_mpv_thread;
	std::atomic<bool> m_thread_exit{ false };

	float m_end_percent{};
	float m_start_percent{};

	void initialize_mpv();

	void mpv_thread();

	void on_mpv_events();

	void on_mpv_render_update();

	void process_mpv_events();

	void gen_fbo_texture();

	void setup_fbo_texture(int w, int h);

	template<typename VariableType>
	std::optional<VariableType> get_property(const std::string& key, mpv_format variable_format) const {
		if (!m_mpv || !m_video_loaded)
			return {};

		VariableType data = 0;
		int res = mpv_get_property(m_mpv, key.c_str(), variable_format, &data);

		if (res != 0)
			return {};

		return data;
	}

	void run_command_impl(const std::vector<std::string>& command, bool async) {
		std::vector<const char*> cmd;
		cmd.reserve(command.size() + 1);

		for (const auto& s : command) {
			cmd.push_back(s.c_str());
		}
		cmd.push_back(nullptr);

		if (async) {
			mpv_command_async(m_mpv, 0, cmd.data());
		}
		else {
			mpv_command(m_mpv, cmd.data());
		}
	}

	void run_command_async(const std::vector<std::string>& command) {
		run_command_impl(command, true);
	}

	void run_command(const std::vector<std::string>& command) {
		run_command_impl(command, false);
	}
};
