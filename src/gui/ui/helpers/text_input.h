#pragma once

#include "gui/render/font/font.h"

#define STB_TEXTEDIT_CHARTYPE       char
#define STB_TEXTEDIT_POSITIONTYPE   int
#define STB_TEXTEDIT_UNDOSTATECOUNT 99
#define STB_TEXTEDIT_UNDOCHARCOUNT  999

#define STB_TEXTEDIT_STRING          ui::helpers::text_input::TextInputState   // Use the forward-declared type
#define STB_TEXTEDIT_STRINGLEN(obj)  ((obj)->text ? (obj)->text->length() : 0) // Add safety check
#define STB_TEXTEDIT_LAYOUTROW       textedit_layoutrow
#define STB_TEXTEDIT_GETWIDTH        textedit_getwidth
#define STB_TEXTEDIT_KEYTOTEXT       textedit_keytotext
#define STB_TEXTEDIT_GETCHAR(obj, i) ((*(obj)->text)[(i)]) // Assumes text is valid if called
#define STB_TEXTEDIT_NEWLINE         '\n'
#define STB_TEXTEDIT_DELETECHARS     textedit_deletechars
#define STB_TEXTEDIT_INSERTCHARS(text_data, pos, newtext, newtextlen)                                                  \
	textedit_insertchars(text_data, pos, newtext, newtextlen)

// Key mapping (Use SDL_SCANCODE_, combine modifiers in event handler)
#define STB_TEXTEDIT_K_SHIFT     (1 << 30) // Use a high bit for Shift flag internally if needed by STB
#define STB_TEXTEDIT_K_CTRL      (1 << 29) // Use a high bit for Ctrl flag internally if needed by STB
#define STB_TEXTEDIT_K_LEFT      SDL_SCANCODE_LEFT
#define STB_TEXTEDIT_K_RIGHT     SDL_SCANCODE_RIGHT
#define STB_TEXTEDIT_K_UP        SDL_SCANCODE_UP
#define STB_TEXTEDIT_K_DOWN      SDL_SCANCODE_DOWN
#define STB_TEXTEDIT_K_PGUP      SDL_SCANCODE_PAGEUP
#define STB_TEXTEDIT_K_PGDOWN    SDL_SCANCODE_PAGEDOWN
#define STB_TEXTEDIT_K_LINESTART SDL_SCANCODE_HOME
#define STB_TEXTEDIT_K_LINEEND   SDL_SCANCODE_END
#define STB_TEXTEDIT_K_TEXTSTART (SDL_SCANCODE_HOME | STB_TEXTEDIT_K_CTRL) // Use the flag bit
#define STB_TEXTEDIT_K_TEXTEND   (SDL_SCANCODE_END | STB_TEXTEDIT_K_CTRL)  // Use the flag bit
#define STB_TEXTEDIT_K_DELETE    SDL_SCANCODE_DELETE
#define STB_TEXTEDIT_K_BACKSPACE SDL_SCANCODE_BACKSPACE
#define STB_TEXTEDIT_K_UNDO      (SDL_SCANCODE_Z | STB_TEXTEDIT_K_CTRL) // Use the flag bit
#define STB_TEXTEDIT_K_REDO      (SDL_SCANCODE_Y | STB_TEXTEDIT_K_CTRL) // Use the flag bit

// #define STB_TEXTEDIT_K_INSERT    SDL_SCANCODE_INSERT
#define STB_TEXTEDIT_IS_SPACE(ch) std::isspace(static_cast<unsigned char>(ch))

#define STB_TEXTEDIT_K_WORDLEFT  (SDL_SCANCODE_LEFT | STB_TEXTEDIT_K_CTRL)  // Use the flag bit
#define STB_TEXTEDIT_K_WORDRIGHT (SDL_SCANCODE_RIGHT | STB_TEXTEDIT_K_CTRL) // Use the flag bit

#include <stb_textedit.h>

namespace ui::helpers::text_input {
	struct TextInputState {
		std::string* text;
		const render::Font* font;
		std::optional<std::function<void(const std::string&)>> on_change;

		bool operator==(const TextInputState& other) const {
			return text == other.text && font == other.font;
		}
	};

	struct TextInputStateInternal {
		STB_TexteditState edit_state{};
		bool active = false;
		std::string composition;   // For IME
		int ime_cursor = 0;        // Cursor within IME composition
		int ime_selection_len = 0; // Selection length within IME composition
	};

	inline std::unordered_map<std::string, TextInputStateInternal> text_input_map;

	float get_cursor_x(
		const ui::helpers::text_input::TextInputState& input_data, int cursor_pos, const gfx::Point& text_start_pos
	);

	void handle_text_input_event(TextInputState& input_data, TextInputStateInternal& state, const SDL_Event& event);

	bool has_selection(const STB_TexteditState& state);

	void click(STB_TEXTEDIT_STRING* str, STB_TexteditState* state, float x, float y);
	void drag(STB_TEXTEDIT_STRING* str, STB_TexteditState* state, float x, float y);
	void clamp(STB_TEXTEDIT_STRING* str, STB_TexteditState* state);

	void add_text_edit(const std::string& id, TextInputState& input_data);
}
