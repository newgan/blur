#include "text_input.h"
#include "../keys.h"
#include "../../render/render.h"

constexpr int SCROLL_CURSOR_RIGHT_GAP = 15;

namespace {
	void textedit_layoutrow(StbTexteditRow* r, STB_TEXTEDIT_STRING* str, int n) {
		if (!str || !str->font || !str->text) { // Safety checks
			r->num_chars = 0;
			r->x0 = r->x1 = 0;
			r->baseline_y_delta = 1.0f; // Avoid division by zero later maybe
			r->ymin = 0;
			r->ymax = 1.0f;
			return;
		}
		const render::Font& font = *str->font;
		const std::string& text = *str->text;
		int text_len = text.length();

		// Basic single-line implementation: find next newline or end of string
		int start_char = n;
		start_char = std::max(start_char, 0);
		start_char = std::min(start_char, text_len);

		int end_char = text_len; // Assume rest of string initially
		for (int i = start_char; i < text_len; ++i) {
			if (text[i] == '\n') {
				end_char = i + 1; // Include the newline in this row's count
				break;
			}
		}
		r->num_chars = end_char - start_char;
		r->num_chars = std::max(r->num_chars, 0);

		// Calculate width
		std::string sub;
		if (r->num_chars > 0) {
			// Make sure substr args are valid
			int count = r->num_chars;
			if (start_char + count > text_len) {
				count = text_len - start_char;
			}
			if (count > 0) {
				sub = text.substr(start_char, count);
				// Trim trailing newline for width calculation if it exists
				if (!sub.empty() && sub.back() == '\n') {
					sub.pop_back();
				}
			}
		}
		gfx::Size text_size = font.calc_size(sub);

		r->x0 = 0; // Relative to the text starting position
		r->x1 = static_cast<float>(text_size.w);
		r->baseline_y_delta = static_cast<float>(font.height());
		r->ymin = 0;
		r->ymax = static_cast<float>(font.height());
	}

	float textedit_getwidth(STB_TEXTEDIT_STRING* str, int n, int i) {
		if (!str || !str->font || !str->text)
			return 0;

		// bounds check
		if (n + i < 0 || n + i >= str->text->length())
			return 0;

		std::array<char, 2> c_str = { (*(str->text))[n + i], 0 };
		return static_cast<float>(str->font->calc_size(c_str.data()).w);
	}

	void textedit_deletechars(STB_TEXTEDIT_STRING* str, int i, int n) {
		if (!str || !str->text)
			return; // Safety check

		int current_len = str->text->length();
		i = std::max(i, 0);

		if (i >= current_len) {
			return;
		} // Cannot delete past end

		if (i + n > current_len) {
			n = current_len - i; // Adjust count if it goes past end
		}

		if (n <= 0) {
			return;
		}

		str->text->erase(i, n);
		if (str->on_change) {
			(*str->on_change)(*str->text);
		}
	}

	int textedit_insertchars(STB_TEXTEDIT_STRING* str, int i, const STB_TEXTEDIT_CHARTYPE* c, int n) {
		if (!str || !str->text)
			return 0;

		int current_len = str->text->length();
		i = std::clamp(i, 0, current_len);
		if (n <= 0) {
			return 1;
		}

		// Insert `n` characters starting from `c`
		str->text->insert(i, std::string(c, n));

		if (str->on_change) {
			(*str->on_change)(*str->text);
		}
		return 1;
	}

	int textedit_keytotext(int key) {
		// This function remains largely unused as we handle input via SDL events
		return -1;
	}

	bool is_word_boundary(char c) {
		return (std::isspace(c) != 0) || (std::ispunct(c) != 0);
	}

	int move_word_left(STB_TEXTEDIT_STRING* str, int c) {
		if (c <= 0)
			return 0;

		// Skip spaces backwards
		while (c > 0 && is_word_boundary((*str->text)[c - 1]))
			c--;

		// Skip non-spaces backwards
		while (c > 0 && !is_word_boundary((*str->text)[c - 1]))
			c--;

		return c;
	}

	int move_word_right(STB_TEXTEDIT_STRING* str, int c) {
		int len = STB_TEXTEDIT_STRINGLEN(str);
		if (c >= len)
			return len;

		// Skip spaces forward
		while (c < len && is_word_boundary((*str->text)[c]))
			c++;

		// Skip non-spaces forward
		while (c < len && !is_word_boundary((*str->text)[c]))
			c++;

		return c;
	}
}

#define STB_TEXTEDIT_MOVEWORDRIGHT move_word_right
#define STB_TEXTEDIT_MOVEWORDLEFT  move_word_left

#define STB_TEXTEDIT_IMPLEMENTATION
#include <stb_textedit.h>

float ui::helpers::text_input::get_cursor_x(
	const ui::helpers::text_input::TextInputData& input_data, int cursor_pos, const gfx::Point& text_start_pos
) {
	if (!input_data.text || !input_data.font)
		return text_start_pos.x;

	cursor_pos = std::clamp(cursor_pos, 0, static_cast<int>(input_data.text->length()));
	auto text_before_cursor = input_data.text->substr(0, cursor_pos);

	return text_start_pos.x + input_data.font->calc_size(text_before_cursor).w;
}

bool ui::helpers::text_input::has_selection(const STB_TexteditState& state) {
	return STB_TEXT_HAS_SELECTION(&state);
}

void ui::helpers::text_input::click(STB_TEXTEDIT_STRING* str, STB_TexteditState* state, float x, float y) {
	stb_textedit_click(str, state, x, y);
}

void ui::helpers::text_input::drag(STB_TEXTEDIT_STRING* str, STB_TexteditState* state, float x, float y) {
	stb_textedit_drag(str, state, x, y);
}

void ui::helpers::text_input::clamp(STB_TEXTEDIT_STRING* str, STB_TexteditState* state) {
	stb_textedit_clamp(str, state);
}

void ui::helpers::text_input::select_all(STB_TEXTEDIT_STRING* str, STB_TexteditState* state) {
	int text_length = str->text->length();
	state->select_start = 0;
	state->select_end = text_length;
	state->cursor = state->select_start;
}

void ui::helpers::text_input::handle_text_input_event(
	TextInputData& input_data, TextInputStateInternal& state, const SDL_Event& event
) {
	switch (event.type) {
		case SDL_EVENT_TEXT_INPUT: {
			// Character input - pass directly to stb_textedit_key
			// STB expects int, char is fine but cast for clarity/safety if needed
			// Handle multi-byte UTF-8 chars in the string correctly
			const char* text_input = event.text.text;
			stb_textedit_paste(
				&input_data, &state.edit_state, text_input, strlen(text_input)
			); // Use paste for multi-char input potentially
			   // OR loop through bytes if stb_textedit_key expects single chars
			   // for (int i = 0; text_input[i] != '\0'; ++i) {
			   //    stb_textedit_key(&input_data, &state.edit_state, (STB_TEXTEDIT_KEYTYPE)text_input[i]);
			   // }
			break;
		}

		case SDL_EVENT_KEY_DOWN: {
			SDL_Scancode scan = event.key.scancode;
			SDL_Keymod mod = SDL_GetModState();

#ifdef __APPLE__
			bool is_cmd = (mod & SDL_KMOD_GUI) != 0u;
			bool is_shortcut = (mod & SDL_KMOD_GUI) != 0u; // Use Cmd for shortcuts
#else
			bool is_cmd = (mod & SDL_KMOD_CTRL) != 0u;
			bool is_shortcut = (mod & SDL_KMOD_CTRL) != 0u; // Use Ctrl for shortcuts
#endif
			bool is_ctrl = (mod & SDL_KMOD_CTRL) != 0u; // true Ctrl key
			bool is_shift = (mod & SDL_KMOD_SHIFT) != 0u;
			bool is_alt = (mod & SDL_KMOD_ALT) != 0u;

			int stb_key = 0;

			// Handle special movement (word, line jump)
#ifdef __APPLE__
			if (scan == SDL_SCANCODE_LEFT) {
				if (is_alt)
					stb_key = STB_TEXTEDIT_K_WORDLEFT;
				else if (is_cmd)
					stb_key = STB_TEXTEDIT_K_LINESTART;
			}
			else if (scan == SDL_SCANCODE_RIGHT) {
				if (is_alt)
					stb_key = STB_TEXTEDIT_K_WORDRIGHT;
				else if (is_cmd)
					stb_key = STB_TEXTEDIT_K_LINEEND;
			}
			else if (scan == SDL_SCANCODE_UP) {
				if (is_cmd)
					stb_key = STB_TEXTEDIT_K_TEXTSTART;
			}
			else if (scan == SDL_SCANCODE_DOWN) {
				if (is_cmd)
					stb_key = STB_TEXTEDIT_K_TEXTEND;
			}
#else
			if (scan == SDL_SCANCODE_LEFT && is_ctrl)
				stb_key = STB_TEXTEDIT_K_WORDLEFT;
			else if (scan == SDL_SCANCODE_RIGHT && is_ctrl)
				stb_key = STB_TEXTEDIT_K_WORDRIGHT;
			else if (scan == SDL_SCANCODE_UP && is_ctrl)
				stb_key = STB_TEXTEDIT_K_TEXTSTART;
			else if (scan == SDL_SCANCODE_DOWN && is_ctrl)
				stb_key = STB_TEXTEDIT_K_TEXTEND;
#endif

			if (stb_key == 0) {
				// No special movement: normal keys
				stb_key = scan;
			}

			// If Shift is held, apply shift modifier now
			if (is_shift) {
				stb_key |= STB_TEXTEDIT_K_SHIFT;
			}

			if (is_shortcut) {
				switch (scan) {
					case SDL_SCANCODE_X:   // cut
					case SDL_SCANCODE_C: { // copy
						if (STB_TEXT_HAS_SELECTION(&state.edit_state)) {
							int start = state.edit_state.select_start;
							int end = state.edit_state.select_end;
							if (start > end)
								std::swap(start, end);

							std::string selection = input_data.text->substr(start, end - start);
							SDL_SetClipboardText(selection.c_str());

							if (scan == SDL_SCANCODE_X) {
								stb_textedit_cut(&input_data, &state.edit_state);
							}
						}
						break;
					}
					case SDL_SCANCODE_V: { // paste
						if (SDL_HasClipboardText()) {
							if (char* clipboard_text = SDL_GetClipboardText()) {
								stb_textedit_paste(
									&input_data, &state.edit_state, clipboard_text, strlen(clipboard_text)
								);
								SDL_free(clipboard_text);
							}
						}
						break;
					}
					case SDL_SCANCODE_A: { // select all (Ctrl + A / Cmd + A)
						if (is_ctrl || is_cmd) {
							select_all(&input_data, &state.edit_state);
						}
						break;
					}
					default:
						// should never reach
						stb_textedit_key(&input_data, &state.edit_state, stb_key);
						break;
				}

				keys::on_key_press_handled(scan);
			}
			else {
				stb_textedit_key(&input_data, &state.edit_state, stb_key);
				keys::on_key_press_handled(scan);
			}
			break;
		}

		case SDL_EVENT_TEXT_EDITING: {
			state.composition = event.edit.text;
			state.ime_cursor = event.edit.start; // Use the state's ime_cursor
			state.ime_selection_len =
				event.edit.length; // Use the state's ime_selection_len
			                       // You might want to adjust the main cursor (state.edit_state.cursor) here
			                       // E.g., place it at the beginning or end of the composition temporarily.
			                       // For now, we leave it where text input placed it.
			break;
		}

		default:
			break;
	}

	stb_textedit_clamp(&input_data, &state.edit_state);
}

ui::helpers::text_input::TextInputStateInternal& ui::helpers::text_input::add_text_edit(
	const std::string& id, TextInputData& input_data
) {
	if (!text_input_map.contains(id)) {
		TextInputStateInternal new_state;
		stb_textedit_initialize_state(&new_state.edit_state, 1);
		new_state.edit_state.single_line = 1;
		return text_input_map.emplace(id, std::move(new_state)).first->second;
	}

	return text_input_map.at(id);
}

bool ui::helpers::text_input::has_text_edit(const std::string& id) {
	return text_input_map.contains(id);
}

void ui::helpers::text_input::remove_text_edit(const std::string& id) {
	text_input_map.erase(id);
}

bool ui::helpers::text_input::has_active_text_edit(const std::string& id) {
	if (!text_input_map.contains(id))
		return false;

	return text_input_map.at(id).active;
}

void ui::helpers::text_input::render_text(
	const TextInputData& input_data,
	const TextInputStateInternal& state,
	gfx::Point text_pos,
	const gfx::Color& text_color,
	const gfx::Rect& clip_rect,
	const std::string& placeholder,
	const gfx::Color& placeholder_color,
	const gfx::Color& selection_colour,
	const gfx::Color& composition_text_color,
	const gfx::Color& composition_bg_color
) {
	render::push_clip_rect(clip_rect, true);

	// --- Render Text / Placeholder ---
	const std::string& display_text = *input_data.text; // Assumes text ptr is valid

	// Calculate the total width of the text to determine overflow
	float text_width = input_data.font->calc_size(display_text).w;

	// Horizontal Scrolling Logic: Shift text position if it's too wide
	if (text_width > clip_rect.w) {
		int cursor_x = helpers::text_input::get_cursor_x(input_data, state.edit_state.cursor, gfx::Point(0, 0));
		int scroll_offset = std::max(0, cursor_x - (clip_rect.w - SCROLL_CURSOR_RIGHT_GAP));
		text_pos.x = text_pos.x - scroll_offset;
	}

	// Handle vertical scrolling if needed (optional, depending on requirements)
	// If text overflows vertically (height of text exceeds clip_rect height), we could add vertical scrolling logic

	if (display_text.empty() && !state.active && !placeholder.empty()) {
		render::text(text_pos, placeholder_color, placeholder, *input_data.font);
		render::pop_clip_rect();
		return;
	}

	// --- Render Selection ---
	if (helpers::text_input::has_selection(state.edit_state)) {
		int sel_start = state.edit_state.select_start;
		int sel_end = state.edit_state.select_end;
		if (sel_start > sel_end)
			std::swap(sel_start, sel_end);

		// Use helper from anonymous namespace
		float x1 = helpers::text_input::get_cursor_x(input_data, sel_start, text_pos);
		float x2 = helpers::text_input::get_cursor_x(input_data, sel_end, text_pos);

		gfx::Rect selection_rect(
			static_cast<int>(x1), text_pos.y, static_cast<int>(x2 - x1), input_data.font->height()
		);

		if (selection_rect.w > 0 && selection_rect.h > 0) {
			render::rect_filled(selection_rect, selection_colour);
		}
	}

	// Render actual text
	render::text(text_pos, text_color, *input_data.text, *input_data.font);

	// --- Render IME Composition --- TODO: NEED TO TEST
	if (state.active && !state.composition.empty()) {
		float base_comp_x = helpers::text_input::get_cursor_x(input_data, state.edit_state.cursor, text_pos);
		gfx::Point comp_pos = { (int)base_comp_x, text_pos.y };
		gfx::Size comp_size = input_data.font->calc_size(state.composition);
		gfx::Rect comp_bg_rect(comp_pos.x, comp_pos.y, comp_size.w, comp_size.h);

		render::rect_filled(comp_bg_rect, composition_bg_color);
		render::text(comp_pos, composition_text_color, state.composition, *input_data.font);
		render::line(
			{ comp_pos.x, comp_pos.y + comp_size.h },
			{ comp_pos.x + comp_size.w, comp_pos.y + comp_size.h },
			composition_text_color
		);

		if (state.ime_cursor >= 0) {
			std::string before_ime_cursor = state.composition.substr(0, state.ime_cursor);
			float ime_cursor_x_offset = input_data.font->calc_size(before_ime_cursor).w;
			render::line(
				{ comp_pos.x + (int)ime_cursor_x_offset, comp_pos.y },
				{ comp_pos.x + (int)ime_cursor_x_offset, comp_pos.y + input_data.font->height() },
				composition_text_color,
				false,
				1.f
			);
		}
	}

	// --- Render Cursor ---
	if (state.active) {
		float cursor_x = helpers::text_input::get_cursor_x(input_data, state.edit_state.cursor, text_pos);
		gfx::Point p1(static_cast<int>(cursor_x), text_pos.y);
		gfx::Point p2(static_cast<int>(cursor_x), text_pos.y + input_data.font->height());
		// Ensure cursor is drawn within clip rect
		auto clip_rect = render::get_clip_rect();
		if (p1.x >= clip_rect.x && p1.x <= clip_rect.x + clip_rect.w) {
			render::line(p1, p2, text_color, false, 1.f);
		}
	}

	render::pop_clip_rect();
}
