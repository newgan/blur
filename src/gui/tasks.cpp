#include "tasks.h"

#include <common/rendering.h>
#include "gui.h"
#include "gui/renderer.h"
#include "gui/ui/ui.h"
#include "base/launcher.h"

void tasks::run(const std::vector<std::string>& arguments) {
	auto res = blur.initialise(false, true);

	gui::initialisation_res = res;

	rendering.set_progress_callback([] {
		if (!gui::window)
			return;

		std::optional<Render*> current_render_opt = rendering.get_current_render();
		if (current_render_opt) {
			Render& current_render = **current_render_opt;

			RenderStatus status = current_render.get_status();
			if (status.finished) {
				u::log("current render is finished, copying it so its final state can be displayed once by gui");

				// its about to be deleted, store a copy to be rendered at least once
				gui::renderer::current_render_copy = current_render;
				gui::to_render = true;
			}
		}

		// idk what you're supposed to do to trigger a redraw in a separate thread!!! I dont do gui!!! this works
		// tho :  ) todo: revisit this
		os::Event event;
		gui::window->queueEvent(event);
	});

	rendering.set_render_finished_callback([](Render* render, const RenderResult& result) {
		gui::renderer::on_render_finished(render, result);
	});

	add_files(arguments); // todo: mac packaged app support (& linux? does it work?)

	auto update_res = Blur::check_updates();
	if (update_res.success && !update_res.is_latest) {
#ifdef WIN32
		gui::renderer::add_notification(
			std::format("There's a newer version ({}) available! Click to run the installer.", update_res.latest_tag),
			ui::NotificationType::INFO,
			[&] {
				Blur::update();
			}
		);
#else
		gui::renderer::add_notification(
			std::format(
				"There's a newer version ({}) available! Click to go to the download page.", update_res.latest_tag
			),
			ui::NotificationType::INFO,
			[&] {
				base::launcher::open_url(update_res.latest_tag_url);
			}
		);
#endif
	}

	while (true) {
		rendering.render_videos();
	}
}

void tasks::add_files(const std::vector<std::string>& path_strs) {
	for (const std::string& path_str : path_strs) {
		std::filesystem::path path = std::filesystem::canonical(path_str);
		if (path.empty() || !std::filesystem::exists(path))
			continue;

		u::log("queueing {}", path.string());

		Render render(path);

		if (gui::renderer::screen != gui::renderer::Screens::MAIN)
			gui::renderer::add_notification(
				std::format("Queued '{}' for rendering", base::to_utf8(render.get_video_name())),
				ui::NotificationType::INFO
			);

		rendering.queue_render(std::move(render));
	}
}

void tasks::add_sample_video(const std::string& path_str) {
	std::filesystem::path path = std::filesystem::canonical(path_str);
	if (path.empty() || !std::filesystem::exists(path))
		return;

	auto sample_video_path = blur.settings_path / "sample_video.mp4";

	// todo: reencode?
	std::filesystem::copy(path, sample_video_path);

	gui::renderer::add_notification("Added sample video", ui::NotificationType::SUCCESS);
}
