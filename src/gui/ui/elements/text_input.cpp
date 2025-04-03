#include "../ui.h"
#include "../render.h"
#include "../keys.h"

#include "../../renderer.h"

#include "../helpers/input.h"

const int LABEL_GAP = 10;
const gfx::Size TEXT_INPUT_PADDING(10, 5);

namespace {
	std::map<std::string, std::unique_ptr<TextInput>> input_map;

	struct Positions {
		int font_height;
		gfx::Point label_pos;
		gfx::Rect input_rect;
	};

	Positions get_positions(const ui::TextInputElementData& input_data, const ui::AnimatedElement& element) {
		int font_height = render::get_font_height(input_data.font);

		gfx::Point label_pos = element.element->rect.origin();
		label_pos.y += font_height;

		auto input_rect = element.element->rect;
		input_rect.y = label_pos.y + LABEL_GAP;
		input_rect.h -= input_rect.y - element.element->rect.y;

		return {
			.font_height = font_height,
			.label_pos = label_pos,
			.input_rect = input_rect,
		};
	}
}

void ui::render_text_input(const Container& container, os::Surface* surface, const AnimatedElement& element) {
	const auto& input_data = std::get<TextInputElementData>(element.element->data);

	float anim = element.animations.at(hasher("main")).current;
	float hover_anim = element.animations.at(hasher("hover")).current;
	float focus_anim = element.animations.at(hasher("focus")).current;

	auto pos = get_positions(input_data, element);

	render::text(surface, pos.label_pos, gfx::rgba(255, 255, 255, anim * 255), input_data.placeholder, input_data.font);

	auto& input = input_map.at(element.element->id);
	input->render(surface, input_data.font, pos.input_rect, anim, hover_anim, focus_anim, input_data.placeholder);
}

bool ui::update_text_input(const Container& container, AnimatedElement& element) {
	auto& input_data = std::get<TextInputElementData>(element.element->data);
	auto& hover_anim = element.animations.at(hasher("hover"));
	auto& focus_anim = element.animations.at(hasher("focus"));

	auto& input = input_map.at(element.element->id);

	auto pos = get_positions(input_data, element);

	bool hovered = pos.input_rect.contains(keys::mouse_pos) && set_hovered_element(element);

	hover_anim.set_goal(hovered ? 1.f : 0.f);

	if (hovered)
		set_cursor(os::NativeCursor::IBeam);

	// Handle mouse click to focus/unfocus
	if (hovered && keys::is_mouse_down()) {
		active_element = &element;
		focus_anim.set_goal(1.f);

		input->handle_mouse_click(pos.input_rect, keys::mouse_pos, input_data.font);
		keys::on_mouse_press_handled(os::Event::MouseButton::LeftButton);
	}
	else if (keys::is_mouse_down() && !hovered) {
		active_element = nullptr;
		focus_anim.set_goal(0.f);
	}

	bool active = active_element == &element;

	if (active) {
		if (keys::is_mouse_dragging()) {
			input->handle_mouse_drag(pos.input_rect, keys::mouse_pos, input_data.font);
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
	const gfx::Size input_size(200, font.getSize() + LABEL_GAP + font.getSize() + (TEXT_INPUT_PADDING.h * 2));

	if (!input_map.contains(id))
		input_map.emplace(
			id,
			std::make_unique<TextInput>(
				&text,
				TextInputConfig{
					.padding_horizontal = TEXT_INPUT_PADDING.w,
					.padding_vertical = TEXT_INPUT_PADDING.h,
				}
			)
		);

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
