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
	bool handle_mouse_click(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font);
	bool handle_mouse_drag(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font);
	void reset_mouse_drag();
	bool handle_mouse_double_click(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font);
	bool handle_mouse_triple_click(const gfx::Point& pos);
	bool handle_scrollbar_drag(const gfx::Point& pos);

	// state
	void undo();
	void redo();

	// rendering
	void render(
		os::Surface* surface,
		const SkFont& font,
		const gfx::Rect& rect,
		float anim,
		float hover_anim,
		float focus_anim,
		const std::string& placeholder = ""
	);

private:
	std::string* text_pointer;

	// text state
	size_t cursor_position = 0;

	// selection state
	bool has_selection = false;
	size_t selection_start = 0;
	size_t selection_end = 0;
	bool drag_start = false;

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

	enum class ActionType {
		InsertText,
		DeleteText,
		ReplaceText
	};

	struct TextAction {
		ActionType type;
		size_t position;
		std::string text;          // Text that was inserted or deleted
		std::string replaced_text; // For ReplaceText actions

		// Insert constructor
		TextAction(size_t pos, const std::string& inserted)
			: type(ActionType::InsertText), position(pos), text(inserted) {}

		// Delete constructor
		TextAction(ActionType type, size_t pos, const std::string& deleted)
			: type(type), position(pos), text(deleted) {}

		// Replace constructor
		TextAction(size_t pos, const std::string& deleted, const std::string& inserted)
			: type(ActionType::ReplaceText), position(pos), text(inserted), replaced_text(deleted) {}
	};

	std::vector<TextAction> undo_stack;
	std::vector<TextAction> redo_stack;
	size_t max_history_size = 100; // Maximum number of undo operations
	bool is_undoing = false;
	bool is_redoing = false;
	std::chrono::system_clock::time_point last_action_time;
	bool can_merge_actions = false;

	void add_insert_action(size_t position, const std::string& text);
	void add_delete_action(size_t position, const std::string& text);
	void add_replace_action(size_t position, const std::string& old_text, const std::string& new_text);
	bool should_merge_with_last_action(ActionType type, size_t position);

	size_t get_char_index_at_position(const gfx::Rect& rect, const gfx::Point& pos, const SkFont& font);
};
