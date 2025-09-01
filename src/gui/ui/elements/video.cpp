#include "../ui.h"
#include "../../render/render.h"
#include "../keys.h"
#include "../helpers/video.h"

constexpr gfx::Size LOADER_SIZE(20, 20);
constexpr gfx::Size LOADER_PAD(5, 5);

namespace {
	std::unordered_map<std::string, std::shared_ptr<VideoPlayer>> video_players;

	tl::expected<std::shared_ptr<VideoPlayer>, std::string> get_or_add_player(const std::filesystem::path& video_path) {
		auto key = video_path.string();

		try {
			auto it = video_players.find(key);

			if (it == video_players.end()) {
				auto player = std::make_shared<VideoPlayer>();
				player->load_file(key.c_str());
				u::log("loaded video from {}", key);

				auto insert_result = video_players.insert({ key, player });
				it = insert_result.first; // safe because insert always returns valid iterator
			}

			return it->second;
		}
		catch (const std::exception& e) {
			u::log_error("failed to load video from {} ({})", key, e.what());
			return tl::unexpected("failed to load video");
		}
	}
}

void ui::render_videos() {
	for (auto& [id, player] : video_players) {
		if (player && player->is_video_ready()) {
			auto dimensions = player->get_video_dimensions();
			if (dimensions) {
				auto [width, height] = *dimensions;
				player->render(width, height);
			}
		}
	}
}

void ui::handle_videos_event(const SDL_Event& event, bool& to_render) {
	for (auto& [id, player] : video_players) {
		if (player) {
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

void ui::render_video(const Container& container, const AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;

	int alpha = anim * 255;
	int stroke_alpha = anim * 125;

	if (video_data.loading) {
		int loader_alpha = anim * 155;

		gfx::Rect loader_rect = element.element->rect.shrink(LOADER_PAD, true);
		render::loader(loader_rect, gfx::Color::white(loader_alpha));
		return;
	}

	auto usable_rect = element.element->rect.shrink(2); // account for border

	// TODO: render::image
	render::imgui.drawlist->AddImage(
		video_data.player->get_frame_texture_for_render(),
		usable_rect.origin(),
		usable_rect.max(),
		ImVec2(0, 0),
		ImVec2(1, 1),
		IM_COL32(255, 255, 255, alpha) // apply alpha for fade animations
	);

	auto frame_data = video_data.player->get_video_frame_data();

	if (frame_data) {
		render::text(
			{ usable_rect.center().x + 30, usable_rect.y - fonts::dejavu.height() },
			gfx::Color::white(),
			std::format("{}/{}", frame_data->current_frame, frame_data->total_frames),
			fonts::dejavu
		);
	}
}

bool ui::update_video(const Container& container, AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);

	auto video_player_it = video_players.find(video_data.video_path.string());
	if (video_player_it != video_players.end()) {
		auto& video_player = video_player_it->second;

		bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);

		if (hovered) {
			set_cursor(SDL_SYSTEM_CURSOR_POINTER);

			while (!event_queue.empty()) {
				auto& event = event_queue.front();

				bool blah;
				video_player->handle_mpv_event(event, blah);

				event_queue.erase(event_queue.begin());
			}
		}
	}

	return false;
}

void ui::remove_video(AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);
	auto video_player_it = video_players.find(video_data.video_path.string());
	if (video_player_it != video_players.end()) {
		video_players.erase(video_player_it);
		u::log("Removed video player for {}", video_data.video_path.string());
	}
	else {
		u::log("No video player found for {}", video_data.video_path.string());
	}
}

std::optional<ui::AnimatedElement*> ui::add_video(
	const std::string& id, Container& container, const std::filesystem::path& video_path, const gfx::Size& max_size
) {
	if (video_path.empty())
		return {};

	auto player_res = get_or_add_player(video_path);
	if (!player_res)
		return {};

	auto player = *player_res;

	gfx::Size elem_size = LOADER_SIZE;
	bool loading = !player->is_video_ready();

	if (!loading) {
		auto dimensions = player->get_video_dimensions();

		if (dimensions) {
			// we have valid dimensions, calculate proper aspect ratio
			auto [video_width, video_height] = *dimensions;
			float aspect_ratio = static_cast<float>(video_width) / static_cast<float>(video_height);

			elem_size = max_size;

			// maintain aspect ratio while fitting within max_size
			float target_width = elem_size.h * aspect_ratio;
			float target_height = elem_size.w / aspect_ratio;

			if (target_width <= max_size.w) {
				elem_size.w = static_cast<int>(target_width);
			}
			else {
				elem_size.h = static_cast<int>(target_height);
			}

			// ensure we don't exceed max dimensions
			if (elem_size.h > max_size.h) {
				elem_size.h = max_size.h;
				elem_size.w = static_cast<int>(max_size.h * aspect_ratio);
			}

			if (elem_size.w > max_size.w) {
				elem_size.w = max_size.w;
				elem_size.h = static_cast<int>(max_size.w / aspect_ratio);
			}
		}
	}

	Element element(
		id,
		ElementType::VIDEO,
		gfx::Rect(container.current_position, elem_size),
		VideoElementData{
			.video_path = video_path,
			.player = std::move(player),
			.loading = loading,
		},
		render_video,
		update_video,
		remove_video
	);

	return add_element(container, std::move(element), container.element_gap);
}
