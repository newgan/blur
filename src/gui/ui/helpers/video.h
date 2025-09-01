#pragma once

#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <utility>
#include <optional>

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

	// returns true if render was successful, false if video not ready
	bool render(int w, int h);

	[[nodiscard]] GLuint get_frame_texture_for_render() const {
		return m_tex;
	}

	void handle_mpv_event(const SDL_Event& event, bool& redraw);

	// get actual video dimensions - returns nullopt if video not ready
	[[nodiscard]] std::optional<std::pair<int, int>> get_video_dimensions() const;

	// check if video is loaded and ready to render
	[[nodiscard]] bool is_video_ready() const;

private:
	mpv_handle* m_mpv = nullptr;
	mpv_render_context* m_mpv_gl = nullptr;
	Uint32 m_wakeup_on_mpv_render_update;
	Uint32 m_wakeup_on_mpv_events;

	GLuint m_fbo;
	GLuint m_tex;

	// cache current texture dimensions to avoid unnecessary recreations
	int m_current_width;
	int m_current_height;

	// track if video is loaded and ready to render
	bool m_video_loaded;

	void initialize_mpv();

	void on_mpv_events();

	void on_mpv_render_update();

	void process_mpv_events();

	void gen_fbo_texture();

	void setup_fbo_texture(int w, int h);
};
