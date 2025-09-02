#include "../ui.h"
#include "../../render/render.h"
#include "../keys.h"
#include "../helpers/video.h"

// videos
constexpr gfx::Size LOADER_SIZE(20, 20);
constexpr gfx::Size LOADER_PAD(5, 5);
constexpr size_t MAX_BACKGROUND_VIDEOS = 100; // TODO MR: HANDLE
constexpr float START_FADE = 0.5f;
constexpr int VIDEO_GAP = 30;

// track
constexpr int TRACK_GAP = 10;
constexpr int MIN_TRACK_WIDTH = 250;
constexpr int TRACK_HEIGHT = 40;
constexpr int GRABS_THICKNESS = 1;
constexpr int GRABS_LENGTH = 5;
constexpr gfx::Color GRABS_COLOR(80, 80, 80);
constexpr gfx::Color GRABS_ACTIVE_COLOR(175, 175, 175);
constexpr gfx::Size GRAB_CLICK_EXPANSION(15, 5);

namespace {
	std::unordered_map<std::string, std::shared_ptr<VideoPlayer>> video_players;

	// std::mutex thumbnail_mutex;
	// std::unordered_map<std::string, std::shared_ptr<render::Texture>> thumbnails;

	tl::expected<std::shared_ptr<VideoPlayer>, std::string> get_or_add_player(const std::filesystem::path& video_path) {
		auto key = video_path.string();

		try {
			auto it = video_players.find(key);

			if (it == video_players.end()) {
				auto player = std::make_shared<VideoPlayer>();
				player->load_file(key);
				u::log("loaded video from {}", key);

				auto insert_result = video_players.insert({ key, player });
				it = insert_result.first;
			}

			return it->second;
		}
		catch (const std::exception& e) {
			u::log_error("failed to load video from {} ({})", key, e.what());
			return tl::unexpected("failed to load video");
		}
	}

	// void create_thumbnail_async(const std::filesystem::path& video_path) {
	// 	auto key = video_path.string();
	// 	{
	// 		std::lock_guard<std::mutex> lock(thumbnail_mutex);

	// 		if (thumbnails.contains(key))
	// 			return;
	// 	}

	// 	std::thread([video_path, key] {
	// 		std::lock_guard<std::mutex> lock(thumbnail_mutex);

	// 		auto thumbnail = gui_utils::get_video_thumbnail(video_path, 100, 100, 0.0);

	// 		thumbnails.insert({ key, thumbnail });
	// 	}).detach();
	// }

	// std::optional<std::shared_ptr<render::Texture>> get_thumbnail(const std::filesystem::path& video_path) {
	// 	auto key = video_path.string();

	// 	std::lock_guard<std::mutex> lock(thumbnail_mutex);

	// 	if (thumbnails.contains(key))
	// 		return {};

	// 	return thumbnails[key];
	// }

	gfx::Size get_size_from_dimensions(const ui::Container& container, const std::shared_ptr<VideoPlayer>& player) {
		gfx::Size size = LOADER_SIZE;

		if (!player)
			return size;

		auto dimensions = player->get_video_dimensions();
		if (!dimensions)
			return size;

		// we have valid dimensions, calculate proper aspect ratio
		auto [video_width, video_height] = *dimensions;
		float aspect_ratio = static_cast<float>(video_width) / static_cast<float>(video_height);

		gfx::Size max_size(container.get_usable_rect().w, container.get_usable_rect().h / 2);
		size = max_size;

		// maintain aspect ratio while fitting within max_size
		float target_width = size.h * aspect_ratio;
		float target_height = size.w / aspect_ratio;

		if (target_width <= max_size.w) {
			size.w = static_cast<int>(target_width);
		}
		else {
			size.h = static_cast<int>(target_height);
		}

		// ensure we don't exceed max dimensions
		if (size.h > max_size.h) {
			size.h = max_size.h;
			size.w = static_cast<int>(max_size.h * aspect_ratio);
		}

		if (size.w > max_size.w) {
			size.w = max_size.w;
			size.h = static_cast<int>(max_size.w / aspect_ratio);
		}

		return size;
	}

	std::vector<gfx::Rect> get_video_rects(const ui::AnimatedElement& element, const gfx::Rect& rect) {
		const auto& video_data = std::get<ui::VideoElementData>(element.element->data);

		std::vector<gfx::Rect> rects;

		float offset = element.animations.at(ui::hasher("video_offset")).current;

		for (auto [i, video] : u::enumerate(video_data.videos)) {
			gfx::Rect player_rect(rect.origin().offset_x(offset), video.size);

			rects.push_back(player_rect);

			offset += player_rect.w + VIDEO_GAP;
		}

		return rects;
	}

	// track
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
		const auto& video_data = std::get<ui::VideoElementData>(element.element->data);

		if (video_data.videos.size() == 0 || *video_data.index < 0 || *video_data.index >= video_data.videos.size())
			return;

		const auto& video = video_data.videos[*video_data.index];
		if (!video.player)
			return;

		auto& progress_anim = element.animations.at(ui::hasher("progress"));

		auto progress_percent = video.player->get_percent_pos();
		if (progress_percent)
			progress_anim.set_goal(*progress_percent / 100.f);
	}

	struct GrabRects {
		gfx::Rect left;
		gfx::Rect right;
	};

	GrabRects get_grab_rects(const ui::AnimatedElement& element, const gfx::Rect& rect) {
		auto& video_data = std::get<ui::VideoElementData>(element.element->data);

		auto left_grab_rect = rect;
		left_grab_rect.x += left_grab_rect.w * *video_data.start;
		left_grab_rect.w = GRABS_LENGTH;

		auto right_grab_rect = rect;
		right_grab_rect.x += right_grab_rect.w * *video_data.end - GRABS_LENGTH;
		right_grab_rect.w = GRABS_LENGTH;

		return { .left = left_grab_rect, .right = right_grab_rect };
	}

	// both
	struct Positions {
		gfx::Rect videos_rect;
		gfx::Rect track_rect;
	};

	Positions get_positions(const gfx::Point& origin, const gfx::Size& video_size) {
		auto videos_rect = gfx::Rect{ origin, video_size };
		auto track_rect = gfx::Rect{
			videos_rect.bottom_left().offset_y(TRACK_GAP),
			gfx::Size(std::max(videos_rect.w, MIN_TRACK_WIDTH), TRACK_HEIGHT),
		};

		return { .videos_rect = videos_rect, .track_rect = track_rect };
	}
}

void ui::handle_videos_event(const SDL_Event& event, bool& to_render) {
	for (auto& [id, player] : video_players) {
		if (player->is_focused_player() || !player->is_video_ready()) {
			switch (event.type) {
				case SDL_EVENT_KEY_DOWN:
					player->handle_key_press(event.key.key);
					break;

				default:
					player->handle_mpv_event(event, to_render);
					break;
			}
		}
	}
}

void render_videos_actual(const ui::Container& container, const ui::AnimatedElement& element, gfx::Rect rect) {
	const auto& video_data = std::get<ui::VideoElementData>(element.element->data);

	float anim = element.animations.at(ui::hasher("main")).current;

	int alpha = anim * 255;

	float fade_step = START_FADE / video_data.videos.size();

	std::vector<gfx::Rect> rects = get_video_rects(element, rect);

	for (auto [i, video] : u::enumerate(video_data.videos)) {
		float fade = i == *video_data.index ? 0.f : START_FADE + (fade_step * i);
		if (fade >= 1.f)
			continue;

		auto video_rect = rects[i];
		auto inner_rect = video_rect.shrink(1);

		float player_alpha = alpha * (1.f - fade);

		if (video.player && video.player->is_video_ready() && video.player->render(inner_rect.w, inner_rect.h)) {
			// TODO: render::image
			render::imgui.drawlist->AddImage(
				video.player->get_frame_texture_for_render(),
				inner_rect.origin(),
				inner_rect.max(),
				ImVec2(0, 0),
				ImVec2(1, 1),
				IM_COL32(255, 255, 255, player_alpha) // apply alpha for fade animations
			);
		}
		else {
			gfx::Rect loader_rect = inner_rect.shrink(LOADER_PAD, true);
			render::loader(loader_rect, gfx::Color::white(155 * anim));
			return;
		}

		// else {
		// 	auto thumbnail_texture = get_thumbnail(video_data.videos[i]); // TEMPORARY [i] ACCESS TODO: PROPER
		// 	if (thumbnail_texture)
		// 		render::image(video_rect, *thumbnail_texture.value());
		// }

		render::borders(video_rect, gfx::Color(50, 50, 50, alpha), gfx::Color(15, 15, 15, alpha));
	}
}

void render_track(const ui::Container& container, const ui::AnimatedElement& element, gfx::Rect rect) {
	const auto& video_data = std::get<ui::VideoElementData>(element.element->data);

	if (video_data.videos.size() == 0 || *video_data.index < 0 || *video_data.index >= video_data.videos.size())
		return;

	const auto& video = video_data.videos[*video_data.index];
	if (!video.player)
		return;

	float anim = element.animations.at(ui::hasher("main")).current;
	float progress = element.animations.at(ui::hasher("progress")).current;
	float seeking = element.animations.at(ui::hasher("seeking")).current;
	float seek = element.animations.at(ui::hasher("seek")).current;
	float left_grab = element.animations.at(ui::hasher("left_grab")).current;
	float right_grab = element.animations.at(ui::hasher("right_grab")).current;

	int stroke_alpha = 125;

	render::rect_filled(rect, gfx::Color::black(stroke_alpha * anim));
	render::rect_stroke(rect, gfx::Color(155, 155, 155, stroke_alpha * anim));

	auto grab_rects = get_grab_rects(element, rect);

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
	float progress_anim = anim;
	progress_anim *= (1.f - left_grab);
	progress_anim *= (1.f - right_grab);

	if (!video.player || !video.player->is_video_ready())
		return;

	rect = rect.shrink(1);

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

	render::waveform(rect, active_rect, gfx::Color::white(100 * anim), *video.waveform);
}

void ui::render_videos(const Container& container, const AnimatedElement& element) {
	const auto& video_data = std::get<ui::VideoElementData>(element.element->data);

	auto positions = get_positions(element.element->rect.origin(), video_data.active_video_size);

	render_videos_actual(container, element, positions.videos_rect);
	render_track(container, element, positions.track_rect);

	auto grab_rects = get_grab_rects(element, positions.track_rect);
}

bool update_track(
	const ui::Container& container,
	ui::AnimatedElement& element,
	const ui::VideoElementData::Video& video,
	gfx::Rect rect
) {
	const auto& video_data = std::get<ui::VideoElementData>(element.element->data);

	auto& progress_anim = element.animations.at(ui::hasher("progress"));
	auto& seeking_anim = element.animations.at(ui::hasher("seeking"));
	auto& seek_anim = element.animations.at(ui::hasher("seek"));

	// Grab handle data
	struct GrabHandle {
		const char* name{};
		gfx::Rect rect;
		ui::AnimationState& anim;
		float* var_ptr{};

		bool hovered = false;
	};

	auto grab_rects = get_grab_rects(element, rect);

	std::array grabs = {
		GrabHandle{
			.name = "left",
			.rect = grab_rects.left.expand(GRAB_CLICK_EXPANSION),
			.anim = element.animations.at(ui::hasher("left_grab")),
			.var_ptr = video_data.start,
		},
		GrabHandle{
			.name = "right",
			.rect = grab_rects.right.expand(GRAB_CLICK_EXPANSION),
			.anim = element.animations.at(ui::hasher("right_grab")),
			.var_ptr = video_data.end,
		},
	};

	// Process both grab handles
	for (auto& grab : grabs) {
		std::string action = "video track grab " + std::string(grab.name);

		grab.hovered = grab.rect.contains(keys::mouse_pos) && set_hovered_element(element);

		if (grab.hovered) {
			ui::set_cursor(SDL_SYSTEM_CURSOR_POINTER);

			if (!ui::get_active_element() && keys::is_mouse_down()) {
				ui::set_active_element(element, action);
			}
		}

		if (is_active_element(element, action)) {
			if (keys::is_mouse_down()) {
				grab.anim.set_goal(1.f);

				float mouse_percent =
					rect.mouse_percent_x(); // TODO: when you initially click it if you arent exactly at the right spot
				                            // itll shift the grab a little which is annoying

				*grab.var_ptr = mouse_percent;

				if (strcmp(grab.name, "right") == 0) {
					video.player->set_end(mouse_percent);
				}
				else if (strcmp(grab.name, "left") == 0) {
					video.player->set_start(mouse_percent);
				}

				return true;
			}
			else {
				video.player->set_paused(true);

				ui::reset_active_element();
			}
		}

		grab.anim.set_goal(grab.hovered ? 0.5f : 0.f);
	}

	bool hovered =
		!grabs[0].hovered && !grabs[1].hovered && rect.contains(keys::mouse_pos) && set_hovered_element(element);
	bool active = ui::get_active_element() == &element;

	if (hovered) {
		// set_cursor(SDL_SYSTEM_CURSOR_POINTER);
		if (keys::is_mouse_down())
			set_active_element(element, "video track");
	}

	if (is_active_element(element, "video track")) {
		if (keys::is_mouse_down()) {
			seeking_anim.set_goal(1.f);

			float mouse_percent = rect.mouse_percent_x();

			video.player->seek(mouse_percent, true, false);

			if (seeking_anim.current == 0.f) // new seek, start from current. otherwise smoothly animate from last seek
				seek_anim.current = progress_anim.current;

			seek_anim.set_goal(mouse_percent);

			return true;
		}
		else {
			ui::reset_active_element();
		}
	}

	if (!video.player->get_queued_seek())
		seeking_anim.set_goal(0.f);

	return false;
}

bool update_videos_actual(const ui::Container& container, ui::AnimatedElement& element, gfx::Rect rect) {
	const auto& video_data = std::get<ui::VideoElementData>(element.element->data);

	// TODO MR: idk if you even need this, been working fine without (it was bugged)
	for (auto [i, video] : u::enumerate(video_data.videos)) {
		if (i == *video_data.index) {
			while (!ui::event_queue.empty()) {
				auto& event = ui::event_queue.front();

				bool blah;
				video.player->handle_mpv_event(event, blah);

				ui::event_queue.erase(ui::event_queue.begin());
			}
		}
	}

	// update offset
	float offset = 0;
	for (int i = 0; i < *video_data.index; ++i) {
		offset -= video_data.videos[i].size.w + VIDEO_GAP; // shift left by widths + spacing
	}

	auto& offset_anim = element.animations.at(ui::hasher("video_offset"));
	offset_anim.set_goal(offset);

	std::vector<gfx::Rect> rects = get_video_rects(element, rect);

	for (auto [i, video] : u::enumerate(video_data.videos)) {
		if (rects[i].contains(keys::mouse_pos)) {
			if (keys::is_mouse_down()) {
				keys::on_mouse_press_handled(SDL_BUTTON_LEFT);

				*video_data.index = i;

				// Reset focus state on all players
				for (auto [j, video] : u::enumerate(video_data.videos)) {
					if (video.player) {
						video.player->set_focused_player(j == i);
					}
				}
			}
		}
	}

	return false;
}

bool ui::update_videos(const Container& container, AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);

	if (video_data.videos.size() == 0 || *video_data.index < 0 || *video_data.index >= video_data.videos.size())
		return false;

	const auto& video = video_data.videos[*video_data.index];
	if (!video.player)
		return false;

	auto positions = get_positions(element.element->rect.origin(), video_data.active_video_size);

	bool res = false;

	res |= update_videos_actual(container, element, positions.videos_rect);
	res |= update_track(container, element, video, positions.track_rect);

	return res;
}

void ui::remove_videos(AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);

	for (const auto& video : video_data.videos) {
		auto video_player_it = video_players.find(video.path.string());

		if (video_player_it != video_players.end()) {
			video_players.erase(video_player_it);
			u::log("Removed video player for {}", video.path.string());
		}
		else {
			u::log("No video player found for {}", video.path.string());
		}
	}
}

std::optional<ui::AnimatedElement*> ui::add_videos(
	const std::string& id,
	Container& container,
	const std::vector<std::filesystem::path>& video_paths,
	size_t& index,
	float& start,
	float& end
) {
	if (video_paths.empty())
		return {};

	gfx::Size active_video_size = LOADER_SIZE;
	std::vector<VideoElementData::Video> videos;

	for (auto [i, path] : u::enumerate(video_paths)) {
		// if (i != 0) {
		// 	players.push_back(nullptr);
		// 	create_thumbnail_async(path);
		// 	continue;
		// }
		if (i > MAX_BACKGROUND_VIDEOS)
			continue;

		auto player_res = get_or_add_player(path);
		auto player = *player_res;
		auto size = get_size_from_dimensions(container, player);

		auto waveform = get_waveform(path);
		if (!waveform) {
			// TODO MR:
			return {};
		}

		videos.push_back(
			VideoElementData::Video{
				.path = path,
				.player = player,
				.size = size,
				.waveform = *waveform,
			}
		);

		if (i == index) {
			active_video_size = size;
			player->set_focused_player(true);
		}
		else {
			player->set_focused_player(false);
		}
	}

	auto positions = get_positions(container.current_position, active_video_size);

	Element element(
		id,
		ElementType::VIDEO,
		gfx::Rect(positions.videos_rect.origin(), positions.track_rect.max()),
		VideoElementData{
			.videos = std::move(videos),
			.active_video_size = active_video_size,
			.index = &index,
			.start = &start,
			.end = &end,
		},
		render_videos,
		update_videos,
		remove_videos
	);

	auto* elem = add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), AnimationState(25.f) },
			{ hasher("video_offset"), AnimationState(25.f) },
			{ hasher("progress"), AnimationState(70.f) },
			{ hasher("seeking"), AnimationState(70.f) },
			{ hasher("seek"), AnimationState(70.f) },
			{ hasher("left_grab"), AnimationState(150.f) },
			{ hasher("right_grab"), AnimationState(150.f) },
		}
	);

	update_progress(*elem);

	return elem;
}
