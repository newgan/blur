#pragma once

namespace render {
	class Texture;
}

namespace gui_utils {
	std::shared_ptr<render::Texture> get_video_thumbnail(
		const std::filesystem::path& path, int width, int height, double timestamp
	);
}
