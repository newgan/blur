#pragma once

#include "../keys.h"

// configuration
struct TextInputConfig {
	int max_length = 0; // 0 = unlimited
	bool numbers_only = false;
	bool allow_special_chars = true;
	float cursor_blink_rate = 0.5f; // seconds
	int tab_size = 4;               // spaces
	int scrollbar_gap = 2;
	int scrollbar_height = 3;       // height of the scrollbar in pixels
	float scrollbar_opacity = 0.5f; // scrollbar opacity
	int padding_horizontal = 8;     // horizontal padding inside the text input
	int padding_vertical = 4;       // vertical padding inside the text input
};

class TextInput {
public:
	TextInput(std::string* text_pointer, const TextInputConfig& config = TextInputConfig());

	// core text manipulation functions
	void insert_text(const std::string& input);
	void backspace();
	void delete_word_backward();
	void delete_word_forward();
	void delete_all_backward();
	void delete_all_forward();
	void delete_forward();
	void delete_selected_text();

	// clipboard operations
	void cut(const std::function<void(const std::string&)>& set_clipboard_func);
	void copy(const std::function<void(const std::string&)>& set_clipboard_func);
	void paste(const std::function<std::string()>& get_clipboard_func);

	// selection management
	void reset_selection();
	void select_all();
	void set_selection(size_t start, size_t end);
	[[nodiscard]] bool has_text_selected() const;
	[[nodiscard]] size_t get_selection_length() const;

	// cursor position management
	void set_cursor_position(size_t pos, bool update_selection = false);
	[[nodiscard]] size_t get_cursor_position() const;
	void move_cursor_left(bool update_selection = false);
	void move_cursor_right(bool update_selection = false);
	void move_cursor_home(bool update_selection = false);
	void move_cursor_end(bool update_selection = false);
	void move_cursor_word_left(bool update_selection = false);
	void move_cursor_word_right(bool update_selection = false);

	// input handling
	bool handle_key_input(const keys::KeyPress& key_press);

	bool handle_mouse_click(int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position);
	bool handle_mouse_drag(int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position);
	bool handle_mouse_double_click(int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position);
	bool handle_mouse_triple_click(int x, int y, const std::function<size_t(int, int)>& get_char_index_at_position);
	bool handle_scrollbar_drag(int x, int y);

	// rendering
	void render(
		os::Surface* surface, const SkFont& font, const gfx::Rect& rect, float anim, float hover_anim, float focus_anim
	);

private:
	std::string* text_pointer;

	// text state
	size_t cursor_position = 0;

	// selection state
	bool has_selection = false;
	size_t selection_start = 0;
	size_t selection_end = 0;

	// configuration
	TextInputConfig config;

	// cursor blinking
	bool show_cursor = true;
	std::chrono::system_clock::time_point last_blink_time;

	// scrolling state
	float scroll_offset = 0.0f;     // horizontal scroll offset in pixels
	float max_scroll_offset = 0.0f; // maximum scroll offset
	bool is_scrollbar_dragging = false;
	float scrollbar_drag_start_x = 0.0f;
	float scrollbar_drag_start_offset = 0.0f;

	// update scroll to ensure cursor visibility
	void ensure_cursor_visible(const SkFont& font, const gfx::Rect& rect);

	// calculate scroll position for a given cursor position
	float calculate_scroll_for_cursor(const SkFont& font, const gfx::Rect& rect);

	// render the scrollbar
	void render_scrollbar(os::Surface* surface, const gfx::Rect& rect, float focus);

	// calculate scrollbar metrics
	struct ScrollbarMetrics {
		bool visible;
		float thumb_x;
		float thumb_width;
	};

	[[nodiscard]] ScrollbarMetrics calculate_scrollbar_metrics(const gfx::Rect& rect) const;
};
