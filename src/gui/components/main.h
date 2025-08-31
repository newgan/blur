#pragma once

#include "common/rendering.h"
#include "../ui/ui.h"

namespace gui::components::main {
	void open_files_button(ui::Container& container, const std::string& label);

	void render_screen(
		ui::Container& container,
		const rendering::VideoRenderDetails& render,
		size_t render_index,
		bool current,
		float delta_time,
		bool& is_progress_shown,
		float& bar_percent
	);

	void home_screen(ui::Container& container, float delta_time);
}
