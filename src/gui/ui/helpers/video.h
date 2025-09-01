#pragma once

#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <utility>
#include <optional>

struct FrameData {
	int64_t current_frame = 0;
	int64_t total_frames = 0;
};

class VideoPlayer {
public:
	VideoPlayer()
		: m_wakeup_on_mpv_render_update(SDL_RegisterEvents(1)), m_wakeup_on_mpv_events(SDL_RegisterEvents(1)), m_fbo(0),
		  m_tex(0), m_current_width(0), m_current_height(0), m_video_loaded(false) {
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

	void load_file(const char* file_path);

	bool render(int w, int h);

	[[nodiscard]] GLuint get_frame_texture_for_render() const {
		return m_tex;
	}

	void handle_mpv_event(const SDL_Event& event, bool& redraw);

	[[nodiscard]] std::optional<std::pair<int, int>> get_video_dimensions() const;
	[[nodiscard]] std::optional<FrameData> get_video_frame_data() const;

	[[nodiscard]] bool is_video_ready() const;

private:
	mpv_handle* m_mpv = nullptr;
	mpv_render_context* m_mpv_gl = nullptr;
	Uint32 m_wakeup_on_mpv_render_update;
	Uint32 m_wakeup_on_mpv_events;

	GLuint m_fbo;
	GLuint m_tex;

	int m_current_width;
	int m_current_height;

	bool m_video_loaded;

	void initialize_mpv();

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
};
