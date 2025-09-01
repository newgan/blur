#include "gui_utils.h"

#include "render/render.h"

std::shared_ptr<render::Texture> gui_utils::get_video_thumbnail(
	const std::filesystem::path& path, int width, int height, double timestamp
) {
	namespace bp = boost::process;

	bp::ipstream pipe_stream;

	auto c = u::run_command(
		blur.ffmpeg_path,
		{
			"-v",
			"error",
			"-ss",
			std::to_string(timestamp), // seek to timestamp
			"-i",
			u::path_to_string(path),
			"-frames:v",
			"1", // extract 1 frame
			"-f",
			"rawvideo", // raw RGB output
			"-pix_fmt",
			"RGB24", // pixel format
			"-",     // stdout
		},
		bp::std_out > pipe_stream,
		bp::std_err > stdout
	);

	const size_t frame_size = width * height * 3; // RGB24: 3 bytes per pixel
	std::vector<unsigned char> buffer(frame_size);
	size_t total_read = 0;

	while (total_read < frame_size &&
	       pipe_stream.read(reinterpret_cast<char*>(buffer.data()) + total_read, frame_size - total_read))
	{
		total_read += static_cast<size_t>(pipe_stream.gcount());
	}

	c.wait();

	if (total_read != frame_size) {
		return nullptr; // Failed to read full frame
	}

	// Create SDL surface from raw RGB data
	SDL_Surface* surface = SDL_CreateSurfaceFrom(
		width,
		height,
		SDL_PIXELFORMAT_RGB24, // RGB24 format
		buffer.data(),
		width * 3 // pitch (bytes per row)
	);

	if (!surface) {
		return nullptr;
	}

	auto tex = std::make_shared<render::Texture>();
	tex->load_from_surface(surface);

	return tex;
}
