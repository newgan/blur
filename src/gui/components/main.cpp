#include "main.h"
#include "../renderer.h"

#include "common/rendering.h"

#include "../tasks.h"
#include "../gui.h"

#include "../ui/ui.h"
#include "../render/render.h"
#include <SDL3/SDL_dialog.h>

namespace main = gui::components::main;

void main::open_files_button(ui::Container& container, const std::string& label) {
	ui::add_button("open file button", container, label, fonts::dejavu, [] {
		static auto file_callback = [](void* userdata, const char* const* files, int filter) {
			if (files && *files) {
				std::vector<std::filesystem::path> wpaths;

				for (const char* const* f = files; *f != nullptr; ++f) {
					wpaths.emplace_back(u::string_to_path(*f));
				}

				tasks::add_files(wpaths);
			}
		};

		const SDL_DialogFileFilter filters[] = {
			{ "Video files",
			  "webm;mkv;flv;vob;ogv;ogg;rrc;gifv;mng;mov;avi;qt;wmv;yuv;rm;rmvb;asf;amv;mp4;m4p;m4v;mpg;mp2;mpeg;mpe;"
			  "mpv;svi;3gp;3g2;mxf;roq;nsv;f4v;f4p;f4a;f4b;mod;ts;m2ts;mts;divx;bik;wtv;drc" }
		};

		SDL_ShowOpenFileDialog(
			file_callback, // Properly typed callback function
			nullptr,       // userdata
			nullptr,       // parent window (nullptr for default)
			filters,       // file filters
			1,             // number of filters
			"",            // default path
			true           // allow multiple files
		);
	});
};

void main::render_progress(
	ui::Container& container,
	const rendering::VideoRenderDetails& render,
	size_t render_index,
	bool current,
	float delta_time,
	bool& is_progress_shown,
	float& bar_percent
) {
	// todo: ui concept
	// screen start|      [faded]last_video current_video [faded]next_video next_video2 next_video3 (+5) |
	// screen end animate sliding in as it moves along the queue

	std::string render_title_text = u::path_to_string(render.input_path.stem());

	if (current) {
		int queue_size = rendering::video_render_queue.size() + tasks::finished_renders;
		if (queue_size > 1) {
			render_title_text = std::format("{} ({}/{})", render_title_text, tasks::finished_renders + 1, queue_size);
		}
	}

	ui::add_text(
		std::format("video {} name text", render_index),
		container,
		render_title_text,
		gfx::Color(255, 255, 255, (current ? 255 : 100)),
		fonts::smaller_header_font,
		FONT_CENTERED_X
	);

	if (!current)
		return;

	int bar_width = 300;

	std::string preview_path = u::path_to_string(render.state->get_preview_path());
	auto progress = render.state->get_progress();

	if (!preview_path.empty() && progress.current_frame > 0) {
		auto element = ui::add_image(
			"preview image",
			container,
			preview_path,
			gfx::Size(container.get_usable_rect().w, container.get_usable_rect().h / 2),
			std::to_string(progress.current_frame)
		);
		if (element) {
			bar_width = (*element)->element->rect.w;
		}
	}

	if (progress.rendered_a_frame) {
		float render_progress = (float)progress.current_frame / (float)progress.total_frames;
		bar_percent = u::lerp(bar_percent, render_progress, 5.f * delta_time, 0.005f);

		ui::add_bar(
			"progress bar",
			container,
			bar_percent,
			gfx::Color(51, 51, 51, 255),
			gfx::Color::white(),
			bar_width,
			std::format("{:.1f}%", render_progress * 100),
			gfx::Color::white(),
			&fonts::dejavu
		);

		container.push_element_gap(6);

		if (render.state->is_paused()) {
			ui::add_text(
				"paused text",
				container,
				"Paused",
				gfx::Color::white(renderer::MUTED_SHADE),
				fonts::dejavu,
				FONT_CENTERED_X
			);
		}

		bool status_fps_init = progress.fps != 0.f;

		if (!status_fps_init)
			container.pop_element_gap();

		ui::add_text(
			"progress text",
			container,
			std::format("frame {}/{}", progress.current_frame, progress.total_frames),
			gfx::Color::white(renderer::MUTED_SHADE),
			fonts::dejavu,
			FONT_CENTERED_X
		);

		if (status_fps_init) {
			ui::add_text(
				"progress text fps",
				container,
				std::format("{:.2f} frames per second", progress.fps),
				gfx::Color::white(renderer::MUTED_SHADE),
				fonts::dejavu,
				FONT_CENTERED_X
			);

			container.pop_element_gap();

			int remaining_frames = progress.total_frames - progress.current_frame;
			int eta_seconds = static_cast<int>(remaining_frames / progress.fps);

			int hours = eta_seconds / 3600;
			int minutes = (eta_seconds % 3600) / 60;
			int seconds = eta_seconds % 60;

			std::ostringstream eta_stream;
			if (hours > 0)
				eta_stream << hours << " hour" << (hours > 1 ? "s " : " ");
			if (minutes > 0)
				eta_stream << minutes << " minute" << (minutes > 1 ? "s " : " ");
			if (seconds > 0 || (hours == 0 && minutes == 0))
				eta_stream << seconds << " second" << (seconds != 1 ? "s" : "");

			ui::add_text(
				"progress text eta",
				container,
				std::format("~{} left", eta_stream.str()),
				gfx::Color::white(renderer::MUTED_SHADE),
				fonts::dejavu,
				FONT_CENTERED_X
			);
		}

		is_progress_shown = true;
	}
	else {
		if (render.state->is_paused()) {
			ui::add_text(
				"paused text",
				container,
				"Paused",
				gfx::Color::white(renderer::MUTED_SHADE),
				fonts::dejavu,
				FONT_CENTERED_X
			);
		}
		else {
			ui::add_text(
				"initialising render text",
				container,
				"Initialising render...",
				gfx::Color::white(),
				fonts::dejavu,
				FONT_CENTERED_X
			);
		}
	}
}

void main::render_pending(ui::Container& container, const std::vector<std::shared_ptr<tasks::PendingVideo>>& pending) {
	size_t pending_index = 0;

	auto& pending_video = pending[pending_index];

	std::string render_title_text = u::path_to_string(pending_video->video_path.stem());

	int queue_size = pending.size() + tasks::finished_renders;
	if (queue_size > 1) {
		render_title_text = std::format("{} ({}/{})", render_title_text, pending_index + 1, queue_size);
	}

	ui::add_text(
		std::format("video {} name text", pending_index),
		container,
		render_title_text,
		gfx::Color::white(),
		fonts::smaller_header_font,
		FONT_CENTERED_X | FONT_OUTLINE
	);

	std::vector<std::filesystem::path> video_paths;
	for (const auto& pv : pending) {
		video_paths.push_back(pv->video_path);
	}

	auto video = ui::add_videos("test video", container, video_paths);

	if (video) {
		auto video_rect = (*video)->element->rect;

		const auto& video_data = std::get<ui::VideoElementData>((*video)->element->data);

		ui::add_video_track(
			"test video track", container, video_rect.w, video_data, pending_video->start, pending_video->end
		);
	}
}

void main::render_home(ui::Container& container) {
	gfx::Point title_pos = container.get_usable_rect().center();
	if (container.rect.h > 275)
		title_pos.y = int(renderer::PAD_Y + fonts::header_font.height());
	else
		title_pos.y = 10 + fonts::header_font.height();

	ui::add_text_fixed(
		"blur title text", container, title_pos, "blur", gfx::Color::white(), fonts::header_font, FONT_CENTERED_X
	);

	if (!initialisation_res) {
		ui::add_text(
			"failed to initialise text",
			container,
			"Failed to initialise",
			gfx::Color::white(),
			fonts::dejavu,
			FONT_CENTERED_X
		);

		ui::add_text(
			"failed to initialise reason",
			container,
			initialisation_res.error(),
			gfx::Color::white(renderer::MUTED_SHADE),
			fonts::dejavu,
			FONT_CENTERED_X
		);

		return;
	}

	open_files_button(container, "Open files");

	ui::add_text(
		"drop file text", container, "or drop them anywhere", gfx::Color::white(), fonts::dejavu, FONT_CENTERED_X
	);
}

main::MainScreen main::screen(ui::Container& container, float delta_time) {
	static float bar_percent = 0.f;

	const auto& pending = tasks::get_pending_copy();

	if (pending.size() > 0) {
		render_pending(container, pending);
		return MainScreen::PENDING;
	}

	const auto& queue = rendering::video_render_queue.get_queue_copy();

	if (!queue.empty()) {
		bool is_progress_shown = false;

		for (const auto [i, render] : u::enumerate(queue)) {
			bool current = i == 0;

			render_progress(container, render, i, current, delta_time, is_progress_shown, bar_percent);
		}

		if (!is_progress_shown)
			bar_percent = 0.f;

		return MainScreen::PROGRESS;
	}

	bar_percent = 0.f;

	render_home(container);

	return MainScreen::HOME;
}
