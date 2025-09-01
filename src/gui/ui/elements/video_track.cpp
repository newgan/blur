#include "../ui.h"
#include "../keys.h"

constexpr int MIN_TRACK_WIDTH = 250;
constexpr int TRACK_HEIGHT = 40;
constexpr int GRABS_THICKNESS = 1;
constexpr int GRABS_LENGTH = 5;
constexpr gfx::Color GRABS_COLOR(80, 80, 80);
constexpr gfx::Color GRABS_ACTIVE_COLOR(175, 175, 175);
constexpr gfx::Size GRAB_CLICK_EXPANSION(15, 5);

namespace {
	std::unordered_map<std::string, std::vector<int16_t>> waveforms;

	tl::expected<std::vector<int16_t>*, std::string> get_waveform(const std::filesystem::path& video_path) {
		auto key = video_path.string();

		try {
			auto it = waveforms.find(key);

			if (it == waveforms.end()) {
				auto waveform = u::get_video_waveform(key);

				auto insert_result = waveforms.insert({ key, waveform });
				it = insert_result.first;
			}

			return &it->second;
		}
		catch (const std::exception& e) {
			u::log_error("failed to load video from {} ({})", key, e.what());
			return tl::unexpected("failed to load video");
		}
	}

	void update_progress(ui::AnimatedElement& element) {
		auto& video_track_data = std::get<ui::VideoTrackElementData>(element.element->data);
		auto& progress_anim = element.animations.at(ui::hasher("progress"));

		auto progress_percent = video_track_data.video_data.player->get_percent_pos();
		if (progress_percent)
			progress_anim.set_goal(*progress_percent / 100.f);
	}

	struct GrabRects {
		gfx::Rect left;
		gfx::Rect right;
	};

	GrabRects get_grab_rects(const ui::AnimatedElement& element) {
		float left_grab_percent = element.animations.at(ui::hasher("left_grab_percent")).current;
		float right_grab_percent = element.animations.at(ui::hasher("right_grab_percent")).current;

		auto left_grab_rect = element.element->rect;
		left_grab_rect.x += left_grab_rect.w * left_grab_percent;
		left_grab_rect.w = GRABS_LENGTH;

		auto right_grab_rect = element.element->rect;
		right_grab_rect.x += right_grab_rect.w * right_grab_percent - GRABS_LENGTH;
		right_grab_rect.w = GRABS_LENGTH;

		return { .left = left_grab_rect, .right = right_grab_rect };
	}
}

void ui::render_video_track(const Container& container, const AnimatedElement& element) {
	const auto& video_track_data = std::get<VideoTrackElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;
	float progress = element.animations.at(hasher("progress")).current;
	float seeking = element.animations.at(hasher("seeking")).current;
	float seek = element.animations.at(hasher("seek")).current;
	float left_grab = element.animations.at(hasher("left_grab")).current;
	float right_grab = element.animations.at(hasher("right_grab")).current;

	int stroke_alpha = 125;

	render::rect_filled(element.element->rect, gfx::Color::black(stroke_alpha * anim));
	render::rect_stroke(element.element->rect, gfx::Color(155, 155, 155, stroke_alpha * anim));

	auto grab_rects = get_grab_rects(element);

	render::rect_side(
		grab_rects.left,
		gfx::Color::lerp(GRABS_COLOR, GRABS_ACTIVE_COLOR, left_grab).adjust_alpha(anim),
		render::RectSide::LEFT,
		GRABS_THICKNESS
	);

	render::rect_side(
		grab_rects.right,
		gfx::Color::lerp(GRABS_COLOR, GRABS_ACTIVE_COLOR, right_grab).adjust_alpha(anim),
		render::RectSide::RIGHT,
		GRABS_THICKNESS
	);

	// dont show progress when grabbing, its implied that you're at where you're grabbing
	anim *= (1.f - left_grab);
	anim *= (1.f - right_grab);

	if (!video_track_data.video_data.player || !video_track_data.video_data.player->is_video_ready())
		return;

	auto rect = element.element->rect.shrink(1);

	auto active_rect = rect;
	active_rect.x = grab_rects.left.x;
	active_rect.w = grab_rects.right.x2() - active_rect.x;

	gfx::Point progress_point = rect.origin();
	progress_point.x = rect.x + (progress * rect.w);

	render::line(progress_point, progress_point.offset_y(rect.h), gfx::Color::white(anim * 255), false, 2.f);

	if (seeking > 0.f) {
		gfx::Point seek_point = rect.origin();
		seek_point.x = rect.x + (seek * rect.w);

		render::line(seek_point, seek_point.offset_y(rect.h), gfx::Color::white(75 * anim * seeking), false, 2.f);
	}

	render::waveform(rect, active_rect, gfx::Color::white(100), *video_track_data.waveform);
}

bool ui::update_video_track(const Container& container, AnimatedElement& element) {
	auto& video_track_data = std::get<VideoTrackElementData>(element.element->data);
	auto rect = element.element->rect;

	// Get animations once
	auto get_anim = [&](const char* name) -> auto& {
		return element.animations.at(ui::hasher(name));
	};

	auto& progress_anim = get_anim("progress");
	auto& seeking_anim = get_anim("seeking");
	auto& seek_anim = get_anim("seek");

	// Grab handle data
	struct GrabHandle {
		const char* name{};
		gfx::Rect rect;
		ui::AnimationState& anim;
		ui::AnimationState& percent_anim;

		bool hovered = false;
	};

	auto grab_rects = get_grab_rects(element);

	std::array grabs = {
		GrabHandle{
			.name = "left",
			.rect = grab_rects.left.expand(GRAB_CLICK_EXPANSION),
			.anim = get_anim("left_grab"),
			.percent_anim = get_anim("left_grab_percent"),
		},
		GrabHandle{
			.name = "right",
			.rect = grab_rects.right.expand(GRAB_CLICK_EXPANSION),
			.anim = get_anim("right_grab"),
			.percent_anim = get_anim("right_grab_percent"),
		},
	};

	// Process both grab handles
	for (auto& grab : grabs) {
		std::string action = "video track grab " + std::string(grab.name);

		grab.hovered = grab.rect.contains(keys::mouse_pos) && set_hovered_element(element);

		if (grab.hovered) {
			set_cursor(SDL_SYSTEM_CURSOR_POINTER);

			if (!get_active_element() && keys::is_mouse_down()) {
				set_active_element(element, action);
			}
		}

		if (is_active_element(element, action)) {
			if (keys::is_mouse_down()) {
				grab.anim.set_goal(1.f);

				float mouse_percent =
					rect.mouse_percent_x(); // TODO: when you initially click it if you arent exactly at the right spot
				                            // itll shift the grab a little which is annoying

				grab.percent_anim.set_goal(mouse_percent);

				// video_track_data.video_data.player->seek(mouse_percent, true, true);
				video_track_data.video_data.player->set_end(mouse_percent * 10.f);

				return true;
			}
			else {
				video_track_data.video_data.player->set_paused(true);

				reset_active_element();
			}
		}

		grab.anim.set_goal(grab.hovered ? 0.5f : 0.f);
	}

	bool hovered =
		!grabs[0].hovered && !grabs[1].hovered && rect.contains(keys::mouse_pos) && set_hovered_element(element);
	bool active = get_active_element() == &element;

	if (hovered) {
		// set_cursor(SDL_SYSTEM_CURSOR_POINTER);
		if (keys::is_mouse_down())
			set_active_element(element, "video track");
	}

	if (is_active_element(element, "video track")) {
		if (keys::is_mouse_down()) {
			seeking_anim.set_goal(1.f);

			float mouse_percent = rect.mouse_percent_x();

			video_track_data.video_data.player->seek(mouse_percent, true, false);

			if (seeking_anim.current == 0.f) // new seek, start from current. otherwise smoothly animate from last seek
				seek_anim.current = progress_anim.current;

			seek_anim.set_goal(mouse_percent);

			return true;
		}
		else {
			reset_active_element();
		}
	}

	if (!video_track_data.video_data.player->get_queued_seek())
		seeking_anim.set_goal(0.f);

	return false;
}

ui::AnimatedElement* ui::add_video_track(
	const std::string& id, Container& container, int width, const VideoElementData& video_data
) {
	gfx::Size size(std::max(MIN_TRACK_WIDTH, width), TRACK_HEIGHT);

	auto waveform = get_waveform(video_data.video_path);
	if (!waveform)
		return {};

	Element element(
		id,
		ElementType::VIDEO_TRACK,
		gfx::Rect(container.current_position, size),
		VideoTrackElementData{
			.video_data = video_data,
			.waveform = *waveform,
		},
		render_video_track,
		update_video_track
	);

	auto* elem = add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), AnimationState(25.f) },
			{ hasher("progress"), AnimationState(70.f) },
			{ hasher("seeking"), AnimationState(70.f) },
			{ hasher("seek"), AnimationState(70.f) },
			{ hasher("left_grab"), AnimationState(150.f) },
			{ hasher("left_grab_percent"), AnimationState(150.f) },
			{ hasher("right_grab"), AnimationState(150.f) },
			{ hasher("right_grab_percent"), AnimationState(150.f, 1.f) },
		}
	);

	update_progress(*elem);

	return elem;
}
