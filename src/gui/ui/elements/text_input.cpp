#include "../ui.h"
#include "../render.h"
#include "../utils.h"
#include "../keys.h"

#include "../../renderer.h"

#include "../helpers/input.h"

namespace {
	std::map<std::string, std::unique_ptr<TextInput>> input_map;
}

void ui::render_text_input(const Container& container, os::Surface* surface, const AnimatedElement& element) {
	const auto& input_data = std::get<TextInputElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float focus_anim = element.animations.at(hasher("focus")).current;

	auto& input = input_map.at(element.element->id);
	input->render(
		surface, input_data.font, element.element->rect, anim, hover_anim, focus_anim, input_data.placeholder
	);
}

bool ui::update_text_input(const Container& container, AnimatedElement& element) {
	auto& input_data = std::get<TextInputElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& focus_anim = element.animations.at(hasher("focus"));

	auto& input = input_map.at(element.element->id);

	bool hovered = element.element->rect.contains(keys::mouse_pos) && set_hovered_element(element);

	hover_anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered)
		set_cursor(os::NativeCursor::IBeam);

	// Handle mouse click to focus/unfocus
	if (hovered && keys::is_mouse_down()) {
		active_element = &element;
		focus_anim.set_goal(1.f);

		input->handle_mouse_click(element.element->rect, keys::mouse_pos, input_data.font);
		keys::on_mouse_press_handled(os::Event::MouseButton::LeftButton);
	}
	else if (keys::is_mouse_down() && !hovered) {
		active_element = nullptr;
		focus_anim.set_goal(0.f);
	}

	bool active = active_element == &element;

	if (active) {
		if (keys::is_mouse_dragging()) {
			input->handle_mouse_drag(element.element->rect, keys::mouse_pos, input_data.font);
		}
		else {
			input->reset_mouse_drag();
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
