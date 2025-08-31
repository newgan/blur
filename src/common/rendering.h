#pragma once

#include "config_app.h"

struct RenderCommands {
	std::vector<std::wstring> vspipe;
	std::vector<std::wstring> ffmpeg;
	bool vspipe_will_stop_early; // for frame renders, stopping early is expected, so using this to selectively ignore
	                             // vspipe response code not being 0
};

namespace rendering {
	struct RenderResult {
		std::filesystem::path output_path;
		bool stopped = false;
	};

	struct RenderState;

	namespace detail {
		struct PipelineResult;

		tl::expected<PipelineResult, std::string> execute_pipeline(
			const RenderCommands& commands,
			const std::shared_ptr<RenderState>& state,
			bool debug,
			const std::function<void()>& progress_callback
		);

		void pause(int pid, const std::shared_ptr<RenderState>& state);
		void resume(int pid, const std::shared_ptr<RenderState>& state);
	}

	struct RenderState {
		void pause() {
			to_pause = true;
		}

		void resume() {
			to_pause = false;
		}

		void toggle_pause() {
			to_pause = !to_pause;
		}

		void stop() {
			to_stop = true;
		}

		struct Progress {
			bool rendered_a_frame = false;
			int current_frame = 0;
			int total_frames = 0;

			bool fps_initialised = false;
			std::chrono::steady_clock::time_point start_time;
			int start_frame = 0;
			std::chrono::duration<double> elapsed_time;
			float fps = 0.f;

			std::string string;
		};

		friend tl::expected<detail::PipelineResult, std::string> detail::execute_pipeline(
			const RenderCommands& commands,
			const std::shared_ptr<RenderState>& state,
			bool debug,
			const std::function<void()>& progress_callback
		);

		friend void detail::pause(int pid, const std::shared_ptr<RenderState>& state);
		friend void detail::resume(int pid, const std::shared_ptr<RenderState>& state);

		std::mutex mutex;
		std::filesystem::path preview_path;
		Progress progress;
		bool paused = false;

	private:
		std::atomic<bool> to_pause = false;

		std::atomic<bool> to_stop = false;
	};

	struct QueuedRender {
		std::filesystem::path input_path;
		u::VideoInfo video_info;
		BlurSettings settings;
		GlobalAppSettings app_settings;
		std::optional<std::filesystem::path> output_path_override;
		std::function<void()> progress_callback;
		std::function<
			void(const QueuedRender& render, const tl::expected<rendering::RenderResult, std::string>& result)>
			finish_callback;

		std::shared_ptr<RenderState> state = std::make_shared<RenderState>();
	};

	namespace detail {
		struct PipelineResult {
			bool stopped;
		};

		tl::expected<nlohmann::json, std::string> merge_settings(
			const BlurSettings& blur_settings, const GlobalAppSettings& app_settings
		);

		std::vector<std::wstring> build_base_vspipe_args(
			const std::filesystem::path& input_path, const nlohmann::json& merged_settings
		);

		boost::process::native_environment setup_environment();

		tl::expected<std::filesystem::path, std::string> create_temp_output_path(
			const std::string& prefix, const std::string& extension = "jpg"
		);

		std::filesystem::path build_output_filename(
			const std::filesystem::path& input_path, const BlurSettings& settings, const GlobalAppSettings& app_settings
		);

		std::vector<std::wstring> build_color_metadata_args(const u::VideoInfo& video_info);

		std::vector<std::wstring> build_audio_filter_args(const BlurSettings& settings, const u::VideoInfo& video_info);

		std::vector<std::wstring> build_encoding_args(
			const BlurSettings& settings, const GlobalAppSettings& app_settings
		);

		void pause(int pid, const std::shared_ptr<RenderState>& state);
		void resume(int pid, const std::shared_ptr<RenderState>& state);

		tl::expected<PipelineResult, std::string> execute_pipeline(
			const RenderCommands& commands,
			const std::shared_ptr<RenderState>& state,
			bool debug,
			const std::function<void()>& progress_callback
		);

		void copy_file_timestamp(const std::filesystem::path& from, const std::filesystem::path& to);

		tl::expected<RenderResult, std::string> render_video(
			const std::filesystem::path& input_path,
			const u::VideoInfo& video_info,
			const BlurSettings& settings,
			const std::shared_ptr<RenderState>& state,
			const GlobalAppSettings& app_settings = config_app::get_app_config(),
			const std::optional<std::filesystem::path>& output_path_override = {},
			const std::function<void()>& progress_callback = {}
		);
	}

	struct QueueAddRes {
		bool is_global_config;
		std::shared_ptr<rendering::RenderState> state;
	};

	class RenderQueue {
	public:
		QueueAddRes add(
			const std::filesystem::path& input_path,
			const u::VideoInfo& video_info,
			const std::optional<std::filesystem::path>& config_path = {},
			const GlobalAppSettings& app_settings = config_app::get_app_config(),
			const std::optional<std::filesystem::path>& output_path_override = {},
			const std::function<void()>& progress_callback = {},
			const std::function<
				void(const QueuedRender& render, const tl::expected<rendering::RenderResult, std::string>& result)>&
				finish_callback = {}
		);

		bool process_next() {
			if (m_queue.empty() || !m_active)
				return false;

			auto cur = m_queue.front();

			auto res = detail::render_video(
				cur.input_path,
				cur.video_info,
				cur.settings,
				cur.state,
				cur.app_settings,
				cur.output_path_override,
				cur.progress_callback
			);

			if (cur.finish_callback)
				cur.finish_callback(cur, res);

			std::unique_lock lock(m_mutex);
			m_queue.erase(m_queue.begin());

			return true;
		}

		void stop() {
			m_active = false;
			// now no more renders will start. see process_next.
		}

		void stop_and_wait() {
			stop();

			std::lock_guard lock(m_mutex);
			if (is_empty())
				return;

			// still rendering the video at the front, so tell it to stop
			auto cur = m_queue.front();
			cur.state->stop();

			while (!is_empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}

		bool is_empty() const {
			std::lock_guard lock(m_mutex);
			return m_queue.empty();
		}

		size_t size() const {
			std::lock_guard lock(m_mutex);
			return m_queue.size();
		}

		std::optional<QueuedRender> front() {
			std::lock_guard lock(m_mutex);
			if (m_queue.empty())
				return {};
			return m_queue.front();
		}

		std::vector<QueuedRender> get_queue_copy() {
			return m_queue;
		}

	private:
		std::vector<QueuedRender> m_queue;
		mutable std::mutex m_mutex;
		std::atomic<bool> m_active = true;
	};

	inline RenderQueue queue;

	tl::expected<RenderResult, std::string> render_frame(
		const std::filesystem::path& input_path,
		const BlurSettings& settings,
		const GlobalAppSettings& app_settings = config_app::get_app_config(),
		const std::shared_ptr<RenderState>& state = std::make_shared<RenderState>()
	);
}
