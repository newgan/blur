#pragma once

#include "common/rendering.h"
#include "../ui/ui.h"

namespace gui::components::main {
	inline std::optional<Render> current_render_copy;

	void open_files_button(ui::Container& container, const std::string& label);

	void render_screen(
		ui::Container& container,
		Render& render,
		bool current,
		float delta_time,
		bool& is_progress_shown,
		float& bar_percent
	);

	void home_screen(ui::Container& container, float delta_time);
}
