//
// Created by castle on 8/16/24.
//

module;
#include "cyd_ui/components/component_macros.h"

export module cydui.std.input.text;

import fabric.logging;
import cydui;

namespace stdui::input {
  struct TextInputContext {

  };
  export struct COMPONENT(
    text, { std::string* text; } STATE { int caret_pos = 0; };
    ATTRIBUTE(on_enter, std::function<void()>){[] {}};
    ATTRIBUTE(on_escape, std::function<void()>){[] {}};
  ) {
    provide_context<TextInputContext> text_input_ctx{};

    CHILDREN {
      return {
        text_input_ctx > with_context {
          $content
        }
      };
    }

    FRAGMENT {
      fragment.draw<vg::rect>().w($width).h($height).fill("#222222"_color);
      fragment.append(build_text(*props.text).fill("#FFFFFF"_color));

      if (state.focused) {
        const auto [x, y, w, h] = build_text(props.text->substr(0, state.caret_pos)).get_footprint();
        fragment.draw<vg::rect>().x(x + w - 1_px).y(y - 3_px).w(2_px).h(h + 3_px).fill("#FFFFFF"_color);
      }
    }

    ON_KEY_PRESS {
      if (ev.keysym.code == Keycode::SDLK_LEFT) {
        state.caret_pos = advance_from(state.caret_pos, -1);
      } else if (ev.keysym.code == Keycode::SDLK_RIGHT) {
        state.caret_pos = advance_from(state.caret_pos);
      } else if (ev.keysym.code == Keycode::SDLK_HOME) {
        state.caret_pos = 0;
      } else if (ev.keysym.code == Keycode::SDLK_END) {
        state.caret_pos = props.text->size();
      } else if (ev.keysym.code == Keycode::SDLK_RETURN) {
        $component.on_enter_();
      } else if (ev.keysym.code == Keycode::SDLK_ESCAPE) {
        $component.on_escape_();
      } else if (ev.keysym.code == Keycode::SDLK_BACKSPACE) {
        if (state.caret_pos == static_cast<int>(props.text->size())) {
          while (!props.text->empty()) {
            const unsigned char c = (*props.text)[props.text->size() - 1];
            props.text->pop_back();
            --state.caret_pos;
            if ((c & 0x80) == 0) { // ASCII character (1 byte)
              break;
            } else if ((c & 0xC0) == 0x80) { // Continuation byte (part of a multi-byte character)
              continue;
            } else { // Leading byte of a UTF-8 character
              break;
            }
            // state.text = state.text.substr(0, state.text.size() - 1);
          }
        } else {
          while (!props.text->empty() && state.caret_pos > 0) {
            const unsigned char c = (*props.text)[state.caret_pos - 1];
            props.text->erase(state.caret_pos - 1, 1);
            --state.caret_pos;
            if ((c & 0x80) == 0) { // ASCII character (1 byte)
              break;
            } else if ((c & 0xC0) == 0x80) { // Continuation byte (part of a multi-byte character)
              continue;
            } else { // Leading byte of a UTF-8 character
              break;
            }
            // state.text = state.text.substr(0, state.text.size() - 1);
          }
        }
      }
      state.mark_dirty();
    }

    ON_TEXT_INPUT {
      if (not ev.compositing_event) {
        if (state.caret_pos == static_cast<int>(props.text->size())) {
          props.text->append(ev.text);
        } else {
          props.text->insert(state.caret_pos, ev.text);
        }
        state.caret_pos += static_cast<int>(ev.text.size());
        state.mark_dirty();
      }
    }

    ON_BUTTON_PRESS {
      int  min_distance = std::numeric_limits<int>::max();
      auto prev         = build_text(props.text->substr(0, advance_from(0))).get_footprint();
      {
        const auto d1 = std::abs(x.value_as_base_unit() - prev.x.value_as_base_unit());
        const auto d2 = std::abs(x.value_as_base_unit() - (prev.x.value_as_base_unit() + prev.w.value_as_base_unit()));
        if (d1 < d2) {
          state.caret_pos = 0;
          min_distance    = d1;
        } else {
          state.caret_pos = 1;
          min_distance    = d2;
        }
      }
      for (int i = 2; i < static_cast<int>(props.text->size()); i = advance_from(i)) {
        const auto curr = build_text(props.text->substr(0, i)).get_footprint();
        {
          const auto d1 = std::abs(x.value_as_base_unit() - (prev.x + prev.w).value_as_base_unit());
          const auto d2 = std::abs(x.value_as_base_unit() - (curr.x + curr.w).value_as_base_unit());
          if (d1 > min_distance && d2 > min_distance) {
            break;
          }
          if (d1 < d2) {
            state.caret_pos = advance_from(i, -1);
            min_distance    = d1;
          } else {
            state.caret_pos = i;
            min_distance    = d2;
          }
        }
        prev = curr;
      }
      state.mark_dirty();
    }

  private:
    int advance_from(int index, int n = 1) const {
      if (n > 0) {
        while (n-- > 0) {
          if (((*props.text)[index] & 0xC0) == 0xC0) {
            ++index;
            while (((*props.text)[index] & 0xC0) == 0x80 && index < static_cast<int>(props.text->size())
            ) {
              ++index;
            }
            index = std::min(index, static_cast<int>(props.text->size()));
          } else {
            ++index;
            index = std::min(index, static_cast<int>(props.text->size()));
          }
        }
      } else if (n < 0) {
        while (n++ < 0) {
          while (((*props.text)[index - 1] & 0xC0) == 0x80 && index > 0) {
            --index;
          }
          --index;
          index = std::max(index, 0);
        }
      }
      return index;
    }

  private:
    vg::text build_text(const std::string& s) const {
      return vg::text{s}.x(0).y(25).font_family("Helvetica");
    }
  };
}
