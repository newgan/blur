#include "../ui.h"
#include "../../render/render.h"
#include "../keys.h"
#include "../helpers/video.h"

constexpr gfx::Size LOADER_SIZE(20, 20);
constexpr gfx::Size LOADER_PAD(5, 5);
constexpr size_t MAX_BACKGROUND_VIDEOS = 10;

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

void ui::render_videos(const Container& container, const AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;

	int alpha = anim * 255;

	if (video_data.loading) {
		int loader_alpha = anim * 155;

		gfx::Rect loader_rect = element.element->rect.shrink(LOADER_PAD, true);
		render::loader(loader_rect, gfx::Color::white(loader_alpha));
		return;
	}

	auto usable_rect = element.element->rect.shrink(3); // account for border

	const float START_FADE = 0.5f;
	float fade_step = START_FADE / video_data.players.size();

	int offset = 0;

	for (auto [i, player] : u::enumerate(video_data.players)) {
		float fade = i == 0 ? 0.f : START_FADE + (fade_step * i);
		if (fade >= 1.f)
			continue;

		auto player_size = get_size_from_dimensions(container, player);
		gfx::Rect player_rect(element.element->rect.origin().offset_x(offset), player_size);

		float player_alpha = alpha * (1.f - fade);

		if (player && player->is_video_ready()) {
			bool to_render = player->render(player_rect.w, player_rect.h);

			if (to_render) {
				// TODO: render::image
				render::imgui.drawlist->AddImage(
					player->get_frame_texture_for_render(),
					player_rect.origin(),
					player_rect.max(),
					ImVec2(0, 0),
					ImVec2(1, 1),
					IM_COL32(255, 255, 255, player_alpha) // apply alpha for fade animations
				);
			}
		}
		// else {
		// 	auto thumbnail_texture = get_thumbnail(video_data.video_paths[i]); // TEMPORARY [i] ACCESS TODO: PROPER
		// 	if (thumbnail_texture)
		// 		render::image(player_rect, *thumbnail_texture.value());
		// }

		render::borders(player_rect, gfx::Color(50, 50, 50, alpha), gfx::Color(15, 15, 15, alpha));

		offset += player_rect.w + 30;
	}
}

bool ui::update_videos(const Container& container, AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);

	if (video_data.players.size() == 0)
		return false;

	const auto& player = video_data.players[0];
	if (!player)
		return false;

	bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);

	if (hovered) {
		set_cursor(SDL_SYSTEM_CURSOR_POINTER);

		while (!event_queue.empty()) {
			auto& event = event_queue.front();

			bool blah;
			player->handle_mpv_event(event, blah);

			event_queue.erase(event_queue.begin());
		}
	}

	return false;
}

void ui::remove_videos(AnimatedElement& element) {
	const auto& video_data = std::get<VideoElementData>(element.element->data);

	for (const auto& video_path : video_data.video_paths) {
		auto video_player_it = video_players.find(video_path.string());

		if (video_player_it != video_players.end()) {
			video_players.erase(video_player_it);
			u::log("Removed video player for {}", video_path.string());
		}
		else {
			u::log("No video player found for {}", video_path.string());
		}
	}
}

std::optional<ui::AnimatedElement*> ui::add_videos(
	const std::string& id, Container& container, const std::vector<std::filesystem::path>& video_paths
) {
	if (video_paths.empty())
		return {};

	gfx::Size elem_size = LOADER_SIZE;
	std::vector<std::shared_ptr<VideoPlayer>> players;

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
		players.push_back(player);

		if (!player_res || i > 0)
			continue;

		if (player->is_video_ready()) {
			elem_size = get_size_from_dimensions(container, player);
		}
	}

	Element element(
		id,
		ElementType::VIDEO,
		gfx::Rect(container.current_position, elem_size),
		VideoElementData{
			.video_paths = video_paths,
			.players = std::move(players),
		},
		render_videos,
		update_videos,
		remove_videos
	);

	return add_element(container, std::move(element), container.element_gap);
}
