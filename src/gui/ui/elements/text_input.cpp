#include "../ui.h"
#include "../render.h"
#include "../utils.h"
#include "../keys.h"

#include "../../renderer.h"

#include "../helpers/input.h"

namespace {
	std::map<std::string, std::unique_ptr<TextInput>> input_map;

	gfx::Rect get_input_rect(const ui::AnimatedElement& element) {
		auto input_rect = element.element->rect;
		return input_rect;
	}
}

void ui::render_text_input(const Container& container, os::Surface* surface, const AnimatedElement& element) {
	const auto& input_data = std::get<TextInputElementData>(element.element->data);
	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float focus_anim = element.animations.at(hasher("focus")).current;

	auto& input = input_map.at(element.element->id);
	input->render(surface, input_data.font, get_input_rect(element), anim, hover_anim, focus_anim);
}

bool ui::update_text_input(const Container& container, AnimatedElement& element) {
	auto& input_data = std::get<TextInputElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& focus_anim = element.animations.at(hasher("focus"));

	auto& input = input_map.at(element.element->id);

	auto input_rect = get_input_rect(element);

	bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);

	hover_anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered)
		set_cursor(os::NativeCursor::IBeam);

	auto get_char_index = [&](int x, int y) -> size_t {
		int last_size = 0;
		for (size_t i = 1; i < input_data.text->size(); i++) {
			int size = render::get_text_size(input_data.text->substr(0, i), input_data.font).w;
			int half = (size - last_size) / 2;

			if (i == 1) {
				// first char need to check left side too since no char before it, redundant otherwise
				if (x < half)
					return i - 1;
			}

			if (x < size + half)
				return i;

			last_size = size;
		}

		return input_data.text->size();
	};

	// Handle mouse click to focus/unfocus
	if (hovered && keys::is_mouse_down()) {
		active_element = &element;
		focus_anim.set_goal(1.f);

		input->handle_mouse_click(keys::mouse_pos.x - input_rect.x, keys::mouse_pos.y - input_rect.y, get_char_index);
		keys::on_mouse_press_handled(os::Event::MouseButton::LeftButton);
	}
	else if (keys::is_mouse_down() && !hovered) {
		active_element = nullptr;
		focus_anim.set_goal(0.f);
	}

	bool active = active_element == &element;

	if (active) {
		if (keys::is_mouse_dragging()) {
			input->handle_mouse_drag(
				keys::mouse_pos.x - input_rect.x, keys::mouse_pos.y - input_rect.y, get_char_index
			);
		}

		for (const auto& key : keys::pressed_keys) {
			input->handle_key_input(key);
		}

		keys::pressed_keys.clear();

		// clear other inputs selections (could do in else, but edge cases)
		for (const auto& [id, other_input] : input_map) {
			if (input == other_input)
				continue;
			other_input->reset_selection();
		}
	}

	return active;
}

ui::Element& ui::add_text_input(
	const std::string& id,
	Container& container,
	std::string& text,
	const std::string& placeholder,
	const SkFont& font,
	std::optional<std::function<void(const std::string&)>> on_change
) {
	const gfx::Size input_size(200, font.getSize() + 12);

	if (!input_map.contains(id))
		input_map.emplace(id, std::make_unique<TextInput>(&text));

	Element element(
		id,
		ElementType::TEXT_INPUT,
		gfx::Rect(container.current_position, input_size),
		TextInputElementData{
			.text = &text,
			.placeholder = placeholder,
			.font = font,
			.on_change = std::move(on_change),
		},
		render_text_input,
		update_text_input
	);

	return *add_element(
		container,
		std::move(element),
		container.element_gap,
		{
			{ hasher("main"), { .speed = 25.f } },
			{ hasher("hover"), { .speed = 80.f } },
			{ hasher("focus"), { .speed = 25.f } },
		}
	);
}
