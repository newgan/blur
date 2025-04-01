#include "input.h"
#include "../render.h"
#include "clip/clip.h"
#include "../utils.h"

TextInput::TextInput(std::string* text_pointer, const TextInputConfig& config)
	: text_pointer(text_pointer), config(config) {
	reset_selection();
	last_blink_time = std::chrono::system_clock::now();
	last_action_time = last_blink_time;
	can_merge_actions = false;
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

	// Store original text for undo if there's a selection
	std::string selected_text;
	size_t insert_position = cursor_position;

	if (has_selection) {
		size_t start = std::min(cursor_position, selection_start);
		size_t end = std::max(cursor_position, selection_start);
		selected_text = text_pointer->substr(start, end - start);
		insert_position = start;
	}

	// delete selected text if there's a selection
	if (has_selection)
		delete_selected_text();

	// insert the new text
	text_pointer->insert(cursor_position, filtered_input);

	// Track action for undo
	if (!selected_text.empty()) {
		// This is a replace operation (selection replaced with new text)
		add_replace_action(insert_position, selected_text, filtered_input);
	}
	else {
		// This is a simple insert
		add_insert_action(insert_position, filtered_input);
	}

	cursor_position += filtered_input.length();
	reset_selection();
}

void TextInput::backspace() {
	if (has_selection) {
		size_t start = std::min(cursor_position, selection_start);
		size_t end = std::max(cursor_position, selection_start);
		std::string selected_text = text_pointer->substr(start, end - start);

		// Delete the selected text
		text_pointer->erase(start, end - start);
		cursor_position = start;
		reset_selection();

		// Track action for undo
		add_delete_action(start, selected_text);
	}
	else if (cursor_position > 0) {
		// Save the character being deleted
		std::string deleted_char = text_pointer->substr(cursor_position - 1, 1);

		// Delete the character
		text_pointer->erase(cursor_position - 1, 1);
		cursor_position--;

		// Track action for undo
		add_delete_action(cursor_position, deleted_char);
	}
}

void TextInput::delete_word_backward() {
	if (has_selection) {
		size_t start = std::min(cursor_position, selection_start);
		size_t end = std::max(cursor_position, selection_start);
		std::string selected_text = text_pointer->substr(start, end - start);

		// Delete the selected text
		text_pointer->erase(start, end - start);
		cursor_position = start;
		reset_selection();

		// Track action for undo
		add_delete_action(start, selected_text);
		return;
	}

	if (cursor_position == 0)
		return;

	// Find word boundary
	// same logic as before
	size_t pos = cursor_position;
	while (pos > 0 && std::isspace((*text_pointer)[pos - 1])) {
		pos--;
	}
	while (pos > 0 && !std::isspace((*text_pointer)[pos - 1])) {
		pos--;
	}

	// Save the text being deleted
	std::string deleted_text = text_pointer->substr(pos, cursor_position - pos);

	// Delete the text
	text_pointer->erase(pos, cursor_position - pos);

	// Track action for undo - we'll treat word delete as a single action
	add_delete_action(pos, deleted_text);

	cursor_position = pos;
}

void TextInput::delete_word_forward() {
	if (has_selection) {
		size_t start = std::min(cursor_position, selection_start);
		size_t end = std::max(cursor_position, selection_start);
		std::string selected_text = text_pointer->substr(start, end - start);

		// Delete the selected text
		text_pointer->erase(start, end - start);
		cursor_position = start;
		reset_selection();

		// Track action for undo
		add_delete_action(start, selected_text);
		return;
	}

	if (cursor_position == text_pointer->length())
		return;

	// Find word boundary
	// same logic as before
	size_t pos = cursor_position;
	while (pos < text_pointer->length() && !std::isspace((*text_pointer)[pos])) {
		pos++;
	}
	while (pos < text_pointer->length() && std::isspace((*text_pointer)[pos])) {
		pos++;
	}

	// Save the text being deleted
	std::string deleted_text = text_pointer->substr(cursor_position, pos - cursor_position);

	// Delete the text
	text_pointer->erase(cursor_position, pos - cursor_position);

	// Track action for undo - we'll treat word delete as a single action
	add_delete_action(cursor_position, deleted_text);
}

void TextInput::delete_all_backward() {
	if (has_selection) {
		delete_selected_text();
		return;
	}

	if (cursor_position == 0)
		return;

	// Save the text being deleted
	std::string deleted_text = text_pointer->substr(0, cursor_position);

	// Delete the text
	text_pointer->erase(0, cursor_position);
	cursor_position = 0;

	// Track action for undo
	add_delete_action(0, deleted_text);
}

void TextInput::delete_all_forward() {
	if (has_selection) {
		delete_selected_text();
		return;
	}

	if (cursor_position >= text_pointer->length())
		return;

	// Save the text being deleted
	std::string deleted_text = text_pointer->substr(cursor_position);

	// Delete the text
	text_pointer->erase(cursor_position, text_pointer->length() - cursor_position);

	// Track action for undo
	add_delete_action(cursor_position, deleted_text);
}

void TextInput::delete_forward() {
	if (has_selection) {
		size_t start = std::min(cursor_position, selection_start);
		size_t end = std::max(cursor_position, selection_start);
		std::string selected_text = text_pointer->substr(start, end - start);

		// Delete the selected text
		text_pointer->erase(start, end - start);
		cursor_position = start;
		reset_selection();

		// Track action for undo
		add_delete_action(start, selected_text);
	}
	else if (cursor_position < text_pointer->length()) {
		// Save the character being deleted
		std::string deleted_char = text_pointer->substr(cursor_position, 1);

		// Delete the character
		text_pointer->erase(cursor_position, 1);

		// Track action for undo
		add_delete_action(cursor_position, deleted_char);
	}
}

void TextInput::delete_selected_text() {
	if (!has_selection)
		return;

	size_t start = std::min(cursor_position, selection_start);
	size_t end = std::max(cursor_position, selection_start);

	// Save the text being deleted
	std::string deleted_text = text_pointer->substr(start, end - start);

	// Delete the text
	text_pointer->erase(start, end - start);
	cursor_position = start;
	reset_selection();

	// Track action for undo
	add_delete_action(start, deleted_text);
}

void TextInput::cut(const std::function<void(const std::string&)>& set_clipboard_func) {
	if (!has_selection)
		return;

	size_t start = std::min(cursor_position, selection_start);
	size_t end = std::max(cursor_position, selection_start);
	std::string selected_text = text_pointer->substr(start, end - start);

	// Put the selected text in clipboard
	set_clipboard_func(selected_text);

	// Delete the selected text
	text_pointer->erase(start, end - start);
	cursor_position = start;
	reset_selection();

	// Track action for undo
	add_delete_action(start, selected_text);
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
		// If we have selection, this is a replace operation
		if (has_selection) {
			size_t start = std::min(cursor_position, selection_start);
			size_t end = std::max(cursor_position, selection_start);
			std::string selected_text = text_pointer->substr(start, end - start);

			// Delete selected text and insert clipboard text
			text_pointer->erase(start, end - start);
			text_pointer->insert(start, clipboard_text);

			// Track as replace action
			add_replace_action(start, selected_text, clipboard_text);

			cursor_position = start + clipboard_text.length();
			reset_selection();
		}
		// Otherwise, it's a simple insert
		else {
			insert_text(clipboard_text);
		}
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
					redo();
				}
				else {
					undo();
				}
				return true;
			}
			break;

// Handle Y as redo on Windows/Linux, but not needed on Mac (Cmd+Shift+Z is standard)
#if !defined(__APPLE__)
		case os::KeyScancode::kKeyY:
			if (ctrl) {
				redo();
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

bool TextInput::handle_scrollbar_drag(const gfx::Point& pos) {
	if (!is_scrollbar_dragging) {
		return false;
	}

	float drag_delta = pos.x - scrollbar_drag_start_x;

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

size_t TextInput::get_char_index_at_position(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font) {
	const int adjusted_x = ((pos.x + scroll_offset) - config.padding_horizontal) - rect.x;

	int last_size = 0;
	for (size_t i = 1; i < text_pointer->size(); i++) {
		int size = render::get_text_size(text_pointer->substr(0, i), font).w;
		int half = (size - last_size) / 2;

		if (i == 1) {
			// first char need to check left side too since no char before it, redundant otherwise
			if (adjusted_x < half)
				return i - 1;
		}

		if (adjusted_x < size + half)
			return i;

		last_size = size;
	}

	return text_pointer->size();
}

bool TextInput::handle_mouse_click(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font) {
	// Check if click is on scrollbar
	ScrollbarMetrics metrics =
		calculate_scrollbar_metrics(gfx::Rect(pos.x, pos.y, 0, 0)); // Using dummy rect for context

	gfx::Rect scrollbar_area(pos.x, pos.y + pos.y - config.scrollbar_height, pos.x, config.scrollbar_height);

	if (metrics.visible && pos.y >= scrollbar_area.y && pos.y <= scrollbar_area.y + scrollbar_area.h) {
		is_scrollbar_dragging = true;
		scrollbar_drag_start_x = pos.x;
		scrollbar_drag_start_offset = scroll_offset;
		return true;
	}

	// Handle normal text click
	is_scrollbar_dragging = false;
	size_t click_pos = get_char_index_at_position(rect, pos, font);
	click_pos = std::min(click_pos, text_pointer->length());
	set_cursor_position(click_pos);
	return true;
}

bool TextInput::handle_mouse_drag(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font) {
	// Check if we're dragging the scrollbar
	if (is_scrollbar_dragging) {
		return handle_scrollbar_drag(pos);
	}

	// If this is the first drag event, start selection
	if (!has_selection) {
		has_selection = true;
		selection_start = cursor_position;
	}

	size_t drag_pos = get_char_index_at_position(rect, pos, font);
	drag_pos = std::min(drag_pos, text_pointer->length());
	selection_end = drag_pos;
	cursor_position = drag_pos;
	return true;
}

bool TextInput::handle_mouse_double_click(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font) {
	size_t click_pos = get_char_index_at_position(rect, pos, font);
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

bool TextInput::handle_mouse_triple_click(const gfx::Point& pos) {
	// Select the entire text on triple click
	select_all();
	return true;
}

void TextInput::render(
	os::Surface* surface,
	const SkFont& font,
	const gfx::Rect& rect,
	float anim,
	float hover_anim,
	float focus_anim,
	const std::string& placeholder
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

	// Determine text to render (text or placeholder)
	std::string display_text = text_pointer->empty() ? placeholder : *text_pointer;
	gfx::Color text_color = utils::adjust_color(
		text_pointer->empty() ? gfx::rgba(150, 150, 150, 255) : gfx::rgba(255, 255, 255, 255), anim
	);

	// Draw text with scroll offset
	render::text(surface, gfx::Point(text_area.x - scroll_offset, text_y), text_color, display_text, font);

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

void TextInput::add_insert_action(size_t position, const std::string& text) {
	if (is_undoing || is_redoing)
		return;

	auto now = std::chrono::system_clock::now();
	std::chrono::duration<float> elapsed = now - last_action_time;
	last_action_time = now;

	// Check if we should merge with the previous action
	if (can_merge_actions && !undo_stack.empty() && elapsed.count() < 1.0f && // Less than 1 second between actions
	    should_merge_with_last_action(ActionType::InsertText, position))
	{
		// Merge with previous insert
		TextAction& last_action = undo_stack.back();
		if (last_action.type == ActionType::InsertText && last_action.position + last_action.text.length() == position)
		{
			last_action.text += text;
			return;
		}
	}

	// Otherwise add as new action
	undo_stack.emplace_back(position, text);
	can_merge_actions = true;

	// Clear redo stack since we've made a new action
	redo_stack.clear();

	// Limit the size of the undo stack
	if (undo_stack.size() > max_history_size) {
		undo_stack.erase(undo_stack.begin());
	}
}

void TextInput::add_delete_action(size_t position, const std::string& text) {
	if (is_undoing || is_redoing)
		return;

	auto now = std::chrono::system_clock::now();
	std::chrono::duration<float> elapsed = now - last_action_time;
	last_action_time = now;

	// Check if we should merge with the previous action
	if (can_merge_actions && !undo_stack.empty() && elapsed.count() < 1.0f && // Less than 1 second between actions
	    should_merge_with_last_action(ActionType::DeleteText, position))
	{
		// Merge with previous delete
		TextAction& last_action = undo_stack.back();
		if (last_action.type == ActionType::DeleteText && position == last_action.position) {
			// Backspace: we're deleting before previous deletion
			last_action.text = text + last_action.text;
			return;
		}
		else if (last_action.type == ActionType::DeleteText && position + text.length() == last_action.position) {
			// Delete: we're deleting after previous deletion
			last_action.text += text;
			last_action.position = position;
			return;
		}
	}

	// Otherwise add as new action
	undo_stack.emplace_back(ActionType::DeleteText, position, text);
	can_merge_actions = true;

	// Clear redo stack since we've made a new action
	redo_stack.clear();

	// Limit the size of the undo stack
	if (undo_stack.size() > max_history_size) {
		undo_stack.erase(undo_stack.begin());
	}
}

void TextInput::add_replace_action(size_t position, const std::string& old_text, const std::string& new_text) {
	if (is_undoing || is_redoing)
		return;

	undo_stack.emplace_back(position, old_text, new_text);
	can_merge_actions = false; // Don't merge replacements

	// Clear redo stack since we've made a new action
	redo_stack.clear();

	// Update the last action time
	last_action_time = std::chrono::system_clock::now();

	// Limit the size of the undo stack
	if (undo_stack.size() > max_history_size) {
		undo_stack.erase(undo_stack.begin());
	}
}

bool TextInput::should_merge_with_last_action(ActionType type, size_t position) {
	if (undo_stack.empty())
		return false;

	TextAction& last_action = undo_stack.back();

	// Only merge actions of the same type
	if (last_action.type != type)
		return false;

	if (type == ActionType::InsertText) {
		// Merge consecutive insertions at cursor position
		return (last_action.position + last_action.text.length() == position);
	}
	else if (type == ActionType::DeleteText) {
		// Merge consecutive deletions at the same position (delete key)
		// or consecutive backspaces (position decreasing by 1)
		return (position == last_action.position || position + 1 == last_action.position);
	}

	return false;
}

void TextInput::undo() {
	if (undo_stack.empty()) {
		return;
	}

	is_undoing = true;

	// Get the last action
	TextAction action = undo_stack.back();
	undo_stack.pop_back();

	// Handle different action types
	switch (action.type) {
		case ActionType::InsertText:
			// Undo an insert by deleting the inserted text
			text_pointer->erase(action.position, action.text.length());
			cursor_position = action.position;
			break;

		case ActionType::DeleteText:
			// Undo a deletion by inserting the deleted text
			text_pointer->insert(action.position, action.text);
			cursor_position = action.position + action.text.length();
			break;

		case ActionType::ReplaceText:
			// Undo a replacement by replacing with the original text
			text_pointer->replace(action.position, action.text.length(), action.replaced_text);
			cursor_position = action.position + action.replaced_text.length();
			break;
	}

	// Save to redo stack
	redo_stack.push_back(action);

	// Reset selection
	reset_selection();

	// Reset cursor blink
	show_cursor = true;
	last_blink_time = std::chrono::system_clock::now();

	can_merge_actions = false;
	is_undoing = false;
}

void TextInput::redo() {
	if (redo_stack.empty()) {
		return;
	}

	is_redoing = true;

	// Get the next action
	TextAction action = redo_stack.back();
	redo_stack.pop_back();

	// Handle different action types
	switch (action.type) {
		case ActionType::InsertText:
			// Redo an insert
			text_pointer->insert(action.position, action.text);
			cursor_position = action.position + action.text.length();
			break;

		case ActionType::DeleteText:
			// Redo a deletion
			text_pointer->erase(action.position, action.text.length());
			cursor_position = action.position;
			break;

		case ActionType::ReplaceText:
			// Redo a replacement
			text_pointer->replace(action.position, action.replaced_text.length(), action.text);
			cursor_position = action.position + action.text.length();
			break;
	}

	// Save back to undo stack
	undo_stack.push_back(action);

	// Reset selection
	reset_selection();

	// Reset cursor blink
	show_cursor = true;
	last_blink_time = std::chrono::system_clock::now();

	can_merge_actions = false;
	is_redoing = false;
}
