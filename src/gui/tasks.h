#pragma once

namespace tasks {
	inline int finished_renders = 0;

	struct PendingVideo {
		std::filesystem::path video_path;
		std::optional<u::VideoInfo> video_info;
		float start = 0.f;
		float end = 1.f;
	};

	void run(const std::vector<std::string>& arguments);

	void add_files(const std::vector<std::filesystem::path>& path_strs);
	void add_sample_video(const std::filesystem::path& path_str);
	void process_pending_files();

	void start_pending_video(size_t index);

	std::vector<std::shared_ptr<tasks::PendingVideo>> get_pending_copy();
}
