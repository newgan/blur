#include "tasks.h"

#include "common/rendering.h"
#include "common/config_app.h"

#include "gui.h"
#include "gui/renderer.h"
#include "gui/ui/ui.h"

#include "components/main.h"
#include "components/notifications.h"
#include "components/configs/configs.h"

namespace {
	size_t pending_index = 0;
	std::vector<std::shared_ptr<tasks::PendingVideo>> pending_videos;
	std::mutex pending_videos_mutex;
}

void tasks::run(const std::vector<std::string>& arguments) {
	gui::initialisation_res = blur.initialise(false, true);

	auto update_res = Blur::check_updates();
	if (update_res && !update_res->is_latest) {
		static const auto update_notification_duration = std::chrono::duration<float>(15.f);

#if defined(WIN32) || defined(__APPLE__)
		gui::components::notifications::add(
			std::format("There's a newer version ({}) available! Click to run the installer.", update_res->latest_tag),
			ui::NotificationType::INFO,
			[&](const std::string& id) {
				gui::components::notifications::close(id);

				const static std::string update_notification_id = "update progress notification";

				gui::components::notifications::add(
					update_notification_id,
					"Downloading update...",
					ui::NotificationType::INFO,
					{},
					std::chrono::duration<float>(gui::components::notifications::NOTIFICATION_LENGTH),
					false
				);

				std::thread([update_res] {
					Blur::update(update_res->latest_tag, [](const std::string& text, bool done) {
						gui::components::notifications::add(
							update_notification_id,
							text,
							ui::NotificationType::INFO,
							{},
							std::chrono::duration<float>(gui::components::notifications::NOTIFICATION_LENGTH),
							done
						);
					});

					blur.exiting = true;
				}).detach();
			},
			update_notification_duration
		);
#else
		gui::components::notifications::add(
			std::format(
				"There's a newer version ({}) available! Click to go to the download page.", update_res->latest_tag
			),
			ui::NotificationType::INFO,
			[&](const std::string& id) {
				SDL_OpenURL(update_res->latest_tag_url.c_str());
			},
			update_notification_duration
		);
#endif
	}

	std::vector<std::filesystem::path> paths;
	paths.reserve(arguments.size());
	for (const auto& argument : arguments) {
		paths.emplace_back(u::string_to_path(argument));
	}

	add_files(paths); // todo: mac packaged app support (& linux? does it work?)

	std::thread video_info_thread([] {
		while (!blur.exiting) {
			process_pending_files();

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	});

	while (!blur.exiting) {
		if (!rendering::video_render_queue.process_next()) {
			finished_renders = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		else {
			finished_renders++;
		}
	}

	video_info_thread.join();
}

void tasks::add_files(const std::vector<std::filesystem::path>& path_strs) {
	std::lock_guard<std::mutex> lock(pending_videos_mutex);

	for (const auto& path_str : path_strs) {
		std::filesystem::path path = std::filesystem::canonical(path_str);
		if (path.empty() || !std::filesystem::exists(path))
			continue;

		u::log("queueing {}", path);

		if (gui::renderer::screen != gui::renderer::Screens::MAIN) {
			gui::components::notifications::add(
				std::format("Queued '{}' for rendering", path.filename()), ui::NotificationType::INFO
			);
		}

		pending_videos.push_back(
			std::make_shared<PendingVideo>(PendingVideo{
				.video_path = path,
			})
		);
	}
}

void tasks::process_pending_files() {
	std::unique_lock<std::mutex> lock(pending_videos_mutex);

	// Find first video without info
	auto it = std::ranges::find_if(pending_videos, [](const auto& pv) {
		return !pv->video_info.has_value();
	});

	if (it == pending_videos.end()) {
		return; // No videos need processing
	}

	auto video_path = (*it)->video_path; // Copy the path
	auto index = std::ranges::distance(pending_videos.begin(), it);
	lock.unlock(); // Release lock while doing expensive I/O

	// Get video info (this is the slow operation)
	auto video_info = u::get_video_info(video_path);

	// Lock again to update the result
	lock.lock();

	// Make sure the vector hasn't been modified in a way that invalidates our index
	if (index < pending_videos.size() && pending_videos[index]->video_path == video_path) {
		if (!video_info.has_video_stream) {
			gui::components::notifications::add(
				std::format("File is not a valid video or is unreadable: {}", video_path.filename()),
				ui::NotificationType::NOTIF_ERROR
			);
			// Remove invalid video from queue
			pending_videos.erase(pending_videos.begin() + index);
		}
		else {
			pending_videos[index]->video_info = video_info;
		}
	}
}

void tasks::add_sample_video(const std::filesystem::path& path_str) {
	std::filesystem::path path = std::filesystem::canonical(path_str);
	if (path.empty() || !std::filesystem::exists(path))
		return;

	auto sample_video_path = blur.settings_path / "sample_video.mp4";

	// todo: reencode?
	std::filesystem::copy(path, sample_video_path);

	gui::components::notifications::add("Added sample video", ui::NotificationType::SUCCESS);

	gui::components::configs::just_added_sample_video = true;
}

void tasks::start_pending_video(size_t index) {
	std::lock_guard<std::mutex> lock(pending_videos_mutex);

	if (index >= pending_videos.size()) {
		// TODO MR: handle
		return;
	}

	auto pending_video = std::move(pending_videos[index]);
	pending_videos.erase(pending_videos.begin() + index);

	if (!pending_video->video_info)
		return;

	auto app_config = config_app::get_app_config();

	auto queue_config_res = rendering::video_render_queue.add(
		pending_video->video_path,
		*pending_video->video_info,
		{},
		app_config,
		{},
		pending_video->start,
		pending_video->end,
		{},
		[](const rendering::VideoRenderDetails& render,
	       const tl::expected<rendering::RenderResult, std::string>& result) {
			gui::renderer::on_render_finished(render, result);
		}
	);

	// TODO MR: show this in pending screen
	if (app_config.notify_about_config_override) {
		if (!queue_config_res.is_global_config)
			gui::components::notifications::add("Using override config from video folder", ui::NotificationType::INFO);
	}
}

std::vector<std::shared_ptr<tasks::PendingVideo>> tasks::get_pending_copy() {
	return pending_videos;
}
