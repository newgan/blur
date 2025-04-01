#include "input.h"
#include "../render.h"
#include "clip/clip.h"
#include "../utils.h"

TextInput::TextInput(std::string* text_pointer, const TextInputConfig& config)
	: text_pointer(text_pointer), config(config) {
	reset_selection();
	last_blink_time = std::chrono::system_clock::now();
}

void TextInput::insert_text(const std::string& input) {
	// handle potential filters
	std::string filtered_input = input;
	if (config.numbers_only) {
		filtered_input.erase(
			std::remove_if(
				filtered_input.begin(),
				filtered_input.end(),
				[](char c) {
					return !std::isdigit(c);
				}
			),
			filtered_input.end()
		);
	}
	else if (!config.allow_special_chars) {
		filtered_input.erase(
			std::remove_if(
				filtered_input.begin(),
				filtered_input.end(),
				[](char c) {
					return !std::isalnum(c) && !std::isspace(c);
				}
			),
			filtered_input.end()
		);
	}

	if (filtered_input.empty())
		return;

	// handle max length constraint
	if (config.max_length > 0) {
		size_t remaining_space = config.max_length - (text_pointer->length() - get_selection_length());
		if (filtered_input.length() > remaining_space) {
			filtered_input = filtered_input.substr(0, remaining_space);
		}
		if (filtered_input.empty())
			return;
	}

	// delete selected text if there's a selection
	if (has_selection)
		delete_selected_text();

	// insert the new text
	text_pointer->insert(cursor_position, filtered_input);
	cursor_position += filtered_input.length();
	reset_selection();
}

void TextInput::backspace() {
	if (has_selection) {
		delete_selected_text();
	}
	else if (cursor_position > 0) {
		text_pointer->erase(cursor_position - 1, 1);
		cursor_position--;
	}
}

void TextInput::delete_word_backward() {
	if (has_selection) {
		delete_selected_text();
		return;
	}

	if (cursor_position == 0)
		return;

	// skip any spaces at current position
	size_t pos = cursor_position;
	while (pos > 0 && std::isspace((*text_pointer)[pos - 1])) {
		pos--;
	}

	// skip until we find a space or start of text
	while (pos > 0 && !std::isspace((*text_pointer)[pos - 1])) {
		pos--;
	}

	text_pointer->erase(pos, cursor_position - pos);
	cursor_position = pos;
}

void TextInput::delete_word_forward() {
	if (has_selection) {
		delete_selected_text();
		return;
	}

	if (cursor_position == text_pointer->length())
		return;

	// skip current word
	size_t pos = cursor_position;
	while (pos < text_pointer->length() && !std::isspace((*text_pointer)[pos])) {
		pos++;
	}

	// skip any spaces
	while (pos < text_pointer->length() && std::isspace((*text_pointer)[pos])) {
		pos++;
	}

	text_pointer->erase(cursor_position, pos - cursor_position);
}

void TextInput::delete_all_backward() {
	if (has_selection) {
		delete_selected_text();
		return;
	}

	text_pointer->erase(0, cursor_position);
	cursor_position = 0;
}

void TextInput::delete_all_forward() {
	if (has_selection) {
		delete_selected_text();
		return;
	}

	text_pointer->erase(cursor_position, text_pointer->length() - cursor_position);
}

void TextInput::delete_forward() {
	if (has_selection) {
		delete_selected_text();
	}
	else if (cursor_position < text_pointer->length()) {
		text_pointer->erase(cursor_position, 1);
	}
}

void TextInput::delete_selected_text() {
	if (!has_selection)
		return;

	size_t start = std::min(cursor_position, selection_start);
	size_t end = std::max(cursor_position, selection_start);
	text_pointer->erase(start, end - start);
	cursor_position = start;
	reset_selection();
}

void TextInput::cut(const std::function<void(const std::string&)>& set_clipboard_func) {
	if (!has_selection)
		return;

	copy(set_clipboard_func);
	delete_selected_text();
}

void TextInput::copy(const std::function<void(const std::string&)>& set_clipboard_func) {
	if (!has_selection)
		return;

	size_t start = std::min(cursor_position, selection_start);
	size_t end = std::max(cursor_position, selection_start);
	std::string selected_text = text_pointer->substr(start, end - start);
	set_clipboard_func(selected_text);
}

void TextInput::paste(const std::function<std::string()>& get_clipboard_func) {
	std::string clipboard_text = get_clipboard_func();
	if (!clipboard_text.empty()) {
		insert_text(clipboard_text);
	}
}

void TextInput::reset_selection() {
	has_selection = false;
	selection_start = cursor_position;
	selection_end = cursor_position;
}

void TextInput::select_all() {
	has_selection = true;
	selection_start = 0;
	selection_end = text_pointer->length();
	cursor_position = text_pointer->length();
}

void TextInput::set_selection(size_t start, size_t end) {
	has_selection = true;
	selection_start = std::min(start, text_pointer->length());
	selection_end = std::min(end, text_pointer->length());
	cursor_position = selection_end;
}

bool TextInput::has_text_selected() const {
	return has_selection;
}

size_t TextInput::get_selection_length() const {
	if (!has_selection)
		return 0;
	return std::abs(static_cast<int>(selection_end) - static_cast<int>(selection_start));
}

void TextInput::set_cursor_position(size_t pos, bool update_selection) {
	size_t new_pos = std::min(pos, text_pointer->length());

	if (update_selection) {
		if (!has_selection) {
			has_selection = true;
			selection_start = cursor_position;
		}
		selection_end = new_pos;
	}
	else {
		reset_selection();
	}

	cursor_position = new_pos;

	// Reset blink state when cursor moves
	show_cursor = true;
	last_blink_time = std::chrono::system_clock::now();
}

size_t TextInput::get_cursor_position() const {
	return cursor_position;
}

void TextInput::move_cursor_left(bool update_selection) {
	if (cursor_position > 0) {
		set_cursor_position(cursor_position - 1, update_selection);
	}
}

void TextInput::move_cursor_right(bool update_selection) {
	if (cursor_position < text_pointer->length()) {
		set_cursor_position(cursor_position + 1, update_selection);
	}
}

void TextInput::move_cursor_home(bool update_selection) {
	set_cursor_position(0, update_selection);
}

void TextInput::move_cursor_end(bool update_selection) {
	set_cursor_position(text_pointer->length(), update_selection);
}

void TextInput::move_cursor_word_left(bool update_selection) {
	if (cursor_position == 0)
		return;

	// skip any spaces at current position
	size_t pos = cursor_position;
	while (pos > 0 && std::isspace((*text_pointer)[pos - 1])) {
		pos--;
	}

	// skip until we find a space or start of text
	while (pos > 0 && !std::isspace((*text_pointer)[pos - 1])) {
		pos--;
	}

	set_cursor_position(pos, update_selection);
}

void TextInput::move_cursor_word_right(bool update_selection) {
	if (cursor_position == text_pointer->length())
		return;

	size_t pos = cursor_position;

	// skip any spaces
	while (pos < text_pointer->length() && std::isspace((*text_pointer)[pos])) {
		pos++;
	}

	// skip current word
	while (pos < text_pointer->length() && !std::isspace((*text_pointer)[pos])) {
		pos++;
	}

	set_cursor_position(pos, update_selection);
}

bool TextInput::handle_key_input(const keys::KeyPress& key) {
	auto get_clipboard_func = []() -> std::string {
		std::string out;
		clip::get_text(out);
		return out;
	};

	auto set_clipboard_func = [&](const std::string& text) {
		clip::set_text(text);
	};
// Define platform-specific modifier keys
#if defined(__APPLE__)
#	define PLATFORM_CTRL_MODIFIER os::KeyModifiers::kKeyCmdModifier // Command key on Mac
#	define PLATFORM_ALT_MODIFIER  os::KeyModifiers::kKeyAltModifier // Option/Alt key on Mac
#else
#	define PLATFORM_CTRL_MODIFIER os::KeyModifiers::kKeyCtrlModifier // Control key on Windows/Linux
#	define PLATFORM_ALT_MODIFIER  os::KeyModifiers::kKeyAltModifier  // Alt key on Windows/Linux
#endif

	bool shift = (key.modifiers & os::KeyModifiers::kKeyShiftModifier) != 0;
	bool ctrl = (key.modifiers & PLATFORM_CTRL_MODIFIER) != 0;
	bool alt = (key.modifiers & PLATFORM_ALT_MODIFIER) != 0;

	// special key handling
	if (key.unicode_char >= 0x20 && key.unicode_char <= 0x7E && !ctrl) { // ascii printable characters
		insert_text(std::string(1, key.unicode_char));
		return true;
	}

	// handle keyboard shortcuts and navigation
	switch (key.scancode) {
		case os::KeyScancode::kKeyBackspace:
// On Mac, Cmd+Backspace deletes to beginning of line
#if defined(__APPLE__)
			if (ctrl) {
				delete_all_backward();
				return true;
			}
#endif

			if (alt) {
				delete_word_backward();
				return true;
			}

			backspace();
			return true;

		case os::KeyScancode::kKeyEnter:
			return true;

		case os::KeyScancode::kKeyEsc:
			reset_selection();
			return true;

		case os::KeyScancode::kKeyDel:
#if defined(__APPLE__)
			if (ctrl) {
				delete_all_forward();
				return true;
			}
#endif
			if (alt) {
				delete_word_forward();
				return true;
			}

			delete_forward();
			return true;

		case os::KeyScancode::kKeyLeft:
#if defined(__APPLE__)
			if (ctrl) {
				// Cmd+Left on Mac goes to beginning of line
				move_cursor_home(shift);
				return true;
			}
			else if (alt) {
				// Alt/Option+Left on Mac moves by word
				move_cursor_word_left(shift);
				return true;
			}
			else {
				move_cursor_left(shift);
				return true;
			}
#else
			if (ctrl) {
				move_cursor_word_left(shift);
				return true;
			}
			else {
				move_cursor_left(shift);
				return true;
			}
#endif

		case os::KeyScancode::kKeyRight:
#if defined(__APPLE__)
			if (ctrl) {
				// Cmd+Right on Mac goes to end of line
				move_cursor_end(shift);
				return true;
			}
			else if (alt) {
				// Alt/Option+Right on Mac moves by word
				move_cursor_word_right(shift);
				return true;
			}
			else {
				move_cursor_right(shift);
				return true;
			}
#else
			if (ctrl) {
				move_cursor_word_right(shift);
				return true;
			}
			else {
				move_cursor_right(shift);
				return true;
			}
#endif

		case os::KeyScancode::kKeyHome:
			move_cursor_home(shift);
			return true;

		case os::KeyScancode::kKeyEnd:
			move_cursor_end(shift);
			return true;

		// Handle shortcuts
		case os::KeyScancode::kKeyA:
			if (ctrl) {
				select_all();
				return true;
			}
			break;

		case os::KeyScancode::kKeyC:
			if (ctrl) {
				copy(set_clipboard_func);
				return true;
			}
			break;

		case os::KeyScancode::kKeyV:
			if (ctrl) {
				paste(get_clipboard_func);
				return true;
			}
			break;

		case os::KeyScancode::kKeyX:
			if (ctrl) {
				cut(set_clipboard_func);
				return true;
			}
			break;

		case os::KeyScancode::kKeyZ:
			if (ctrl) {
				if (shift) {
					// redo();
				}
				else {
					// undo();
				}
				return true;
			}
			break;

// Handle Y as redo on Windows/Linux, but not needed on Mac (Cmd+Shift+Z is standard)
#if !defined(__APPLE__)
		case os::KeyScancode::kKeyY:
			if (ctrl) {
				// redo();
				return true;
			}
			break;
#endif
	}

	return false;
}

void TextInput::ensure_cursor_visible(const SkFont& font, const gfx::Rect& rect) {
	// Get the cursor position in pixels
	float cursor_x = render::get_text_size(text_pointer->substr(0, cursor_position), font).w;

	// Calculate visible area width (with a small padding)
	float visible_width = rect.w - (2 * config.padding_horizontal); // Account for horizontal padding

	// If cursor is to the left of the visible area, scroll left
	if (cursor_x < scroll_offset + 5.0f) {
		scroll_offset = std::max(0.0f, cursor_x - 5.0f);
	}
	// If cursor is to the right of the visible area, scroll right
	else if (cursor_x > scroll_offset + visible_width - 5.0f) {
		scroll_offset = cursor_x - visible_width + 5.0f;
	}

	// Ensure scroll offset is not negative
	scroll_offset = std::max(0.0f, scroll_offset);

	// Ensure scroll offset doesn't go beyond text width minus visible area
	float text_width = render::get_text_size(*text_pointer, font).w;
	max_scroll_offset = std::max(0.0f, text_width - visible_width);
	scroll_offset = std::min(scroll_offset, max_scroll_offset);
}

float TextInput::calculate_scroll_for_cursor(const SkFont& font, const gfx::Rect& rect) {
	float cursor_x = render::get_text_size(text_pointer->substr(0, cursor_position), font).w;
	float visible_width = rect.w - (2 * config.padding_horizontal); // Account for horizontal padding

	// If text is shorter than visible area, no need to scroll
	float text_width = render::get_text_size(*text_pointer, font).w;
	if (text_width <= visible_width) {
		return 0.0f;
	}

	// Check if cursor is in visible area
	if (cursor_x >= scroll_offset && cursor_x <= scroll_offset + visible_width) {
		return scroll_offset; // No change needed
	}

	// If cursor is to the left of visible area
	if (cursor_x < scroll_offset) {
		return std::max(0.0f, cursor_x - 5.0f); // Scroll left with padding
	}

	// If cursor is to the right of visible area
	return cursor_x - visible_width + 5.0f; // Scroll right with padding
}

TextInput::ScrollbarMetrics TextInput::calculate_scrollbar_metrics(const gfx::Rect& rect) const {
	ScrollbarMetrics metrics;

	// Determine if scrollbar should be visible
	metrics.visible = max_scroll_offset > 0.0f;

	if (!metrics.visible) {
		return metrics;
	}

	// Calculate scrollbar thumb width and position
	float visible_width = rect.w - (2 * config.padding_horizontal); // Account for horizontal padding
	float total_content_width = visible_width + max_scroll_offset;
	float visible_ratio = visible_width / total_content_width;
	float min_thumb_width = 20.0f; // Minimum thumb width

	metrics.thumb_width = std::max(min_thumb_width, visible_ratio * rect.w);

	// Calculate scrollbar thumb position
	float scroll_ratio = scroll_offset / max_scroll_offset;
	float available_track = rect.w - metrics.thumb_width;
	metrics.thumb_x = rect.x + (scroll_ratio * available_track);

	return metrics;
}

void TextInput::render_scrollbar(os::Surface* surface, const gfx::Rect& rect, float focus) {
	ScrollbarMetrics metrics = calculate_scrollbar_metrics(rect);

	if (!metrics.visible)
		return;

	// Calculate scrollbar rect
	gfx::Rect scrollbar_thumb(
		metrics.thumb_x, rect.y2() + config.scrollbar_gap, metrics.thumb_width, config.scrollbar_height
	);

	// Draw track and thumb with focus-based opacity
	float track_alpha = config.scrollbar_opacity * 128 * focus;
	float thumb_alpha = config.scrollbar_opacity * 255 * focus;

	render::rounded_rect_filled(surface, scrollbar_thumb, gfx::rgba(120, 120, 120, thumb_alpha), 0.f);
}

bool TextInput::handle_scrollbar_drag(int x, int y) {
	if (!is_scrollbar_dragging) {
		return false;
	}

	float drag_delta = x - scrollbar_drag_start_x;

	// Calculate the new scroll offset based on drag delta
	float available_track = max_scroll_offset;
	if (available_track <= 0.0f) {
		return true;
	}

	// Convert drag delta to scroll offset
	float scroll_delta = (drag_delta / available_track) * max_scroll_offset;
	scroll_offset = std::max(0.0f, std::min(max_scroll_offset, scrollbar_drag_start_offset + scroll_delta));

	return true;
}

bool TextInput::handle_mouse_click(int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position) {
	// Check if click is on scrollbar
	ScrollbarMetrics metrics = calculate_scrollbar_metrics(gfx::Rect(x, y, 0, 0)); // Using dummy rect for context

	gfx::Rect scrollbar_area(x, y + y - config.scrollbar_height, x, config.scrollbar_height);

	if (metrics.visible && y >= scrollbar_area.y && y <= scrollbar_area.y + scrollbar_area.h) {
		is_scrollbar_dragging = true;
		scrollbar_drag_start_x = x;
		scrollbar_drag_start_offset = scroll_offset;
		return true;
	}

	// Handle normal text click
	is_scrollbar_dragging = false;
	size_t click_pos = get_char_index_at_position(x + scroll_offset, y);
	click_pos = std::min(click_pos, text_pointer->length());
	set_cursor_position(click_pos);
	return true;
}

bool TextInput::handle_mouse_drag(int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position) {
	// Check if we're dragging the scrollbar
	if (is_scrollbar_dragging) {
		return handle_scrollbar_drag(x, y);
	}

	// If this is the first drag event, start selection
	if (!has_selection) {
		has_selection = true;
		selection_start = cursor_position;
	}

	size_t drag_pos = get_char_index_at_position(x + scroll_offset, y);
	drag_pos = std::min(drag_pos, text_pointer->length());
	selection_end = drag_pos;
	cursor_position = drag_pos;
	return true;
}

bool TextInput::handle_mouse_double_click(
	int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position
) {
	size_t click_pos = get_char_index_at_position(x + scroll_offset, y);
	if (click_pos >= text_pointer->length())
		return true;

	// Find word boundaries
	size_t start = click_pos;
	while (start > 0 && !std::isspace((*text_pointer)[start - 1])) {
		start--;
	}

	size_t end = click_pos;
	while (end < text_pointer->length() && !std::isspace((*text_pointer)[end])) {
		end++;
	}

	set_selection(start, end);
	return true;
}

bool TextInput::handle_mouse_triple_click(
	int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position
) {
	// Select the entire text on triple click
	select_all();
	return true;
}

void TextInput::render(
	os::Surface* surface, const SkFont& font, const gfx::Rect& rect, float anim, float hover_anim, float focus_anim
) {
	const float input_rounding = 5.0f;
	const float cursor_width = 2.0f;

	// Update cursor blink state
	auto current_time = std::chrono::system_clock::now();
	std::chrono::duration<float> elapsed = current_time - last_blink_time;
	if (elapsed.count() >= config.cursor_blink_rate) {
		show_cursor = !show_cursor;
		last_blink_time = current_time;
	}

	// Ensure cursor is visible before rendering
	ensure_cursor_visible(font, rect);

	// Background color
	int base_shade = 30 + (10 * hover_anim);
	gfx::Color bg_color = utils::adjust_color(gfx::rgba(base_shade, base_shade, base_shade, 255), anim);
	gfx::Color border_color = utils::adjust_color(
		gfx::rgba(100 + (50 * focus_anim), 100 + (50 * focus_anim), 100 + (50 * focus_anim), 255), anim
	);

	// Render input background and border
	render::rounded_rect_filled(surface, rect, bg_color, input_rounding);
	render::rounded_rect_stroke(surface, rect, border_color, input_rounding);

	// Calculate text rendering area with padding
	gfx::Rect text_area(
		rect.x + config.padding_horizontal,
		rect.y + config.padding_vertical,
		rect.w - (2 * config.padding_horizontal),
		rect.h - (2 * config.padding_vertical)
	);

	// Adjust text position to account for vertical centering
	int text_height = font.getSize();
	float text_y = text_area.y + (text_area.h - text_height) / 2 + text_height; // Position for baseline

	// Create clipping rectangle to hide overflow
	gfx::Rect clip_rect = text_area;

	// Set clipping region to prevent text overflow
	render::push_clip_rect(surface, clip_rect);

	// Draw selection background if needed
	if (has_selection) {
		size_t start = std::min(selection_start, selection_end);
		size_t end = std::max(selection_start, selection_end);

		float start_x = text_area.x + render::get_text_size(text_pointer->substr(0, start), font).w - scroll_offset;
		float selection_width = render::get_text_size(text_pointer->substr(start, end - start), font).w;

		// Draw selection rectangle
		render::rect_filled(
			surface, gfx::Rect(start_x, text_area.y, selection_width, text_area.h), 0x3399ffaa
		); // Semi-transparent blue
	}

	// Draw text with scroll offset
	render::text(
		surface,
		gfx::Point(text_area.x - scroll_offset, text_y),
		0xffffffff,
		*text_pointer,
		font
	); // White text

	// Draw cursor
	if (show_cursor && focus_anim > 0.f) {
		float cursor_x =
			text_area.x + render::get_text_size(text_pointer->substr(0, cursor_position), font).w - scroll_offset;
		render::rect_filled(
			surface,
			gfx::Rect(cursor_x, text_area.y, cursor_width, text_area.h),
			gfx::rgba(255, 255, 255, focus_anim * 255)
		);
	}

	// Restore clip state
	render::pop_clip_rect(surface);

	// Draw scrollbar if needed
	render_scrollbar(surface, rect, focus_anim);
}
