// Copyright (c) 2024, Víctor Castillo Agüero.
// SPDX-License-Identifier: GPL-3.0-or-later

module;
#include <cairomm-1.16/cairomm/cairomm.h>

export module cydui.graphics:vg;

import std;

import fabric.memory.multidim_data;
import fabric.type_aliases;

export import :vg_attributes;
export import cydui.dimensions;

export namespace vg {
  struct vg_element_t {
    int origin_x = 0;
    int origin_y = 0;
    void _internal_set_origin(int x, int y) {
      origin_x = x;
      origin_y = y;
    }

    virtual ~vg_element_t() = default;

    virtual void apply_to(pixelmap_editor_t &editor) const = 0;

    struct footprint {
      // int x, y, w, h;
      cyd::ui::dimensions::screen_measure x;
      cyd::ui::dimensions::screen_measure y;
      cyd::ui::dimensions::screen_measure w;
      cyd::ui::dimensions::screen_measure h;
    };
    virtual footprint get_footprint() const = 0;
  };

  struct vg_fragment_t {
    std::vector<std::shared_ptr<vg_element_t>> elements{};

    void clear() {
      elements.clear();
    }

    bool empty() const {
      return elements.empty();
    }

    template<typename... T>
    void append(T &&... _elements) {
      static_assert(
        (std::derived_from<std::remove_reference_t<T>, vg_element_t> && ...),
        "Elements must derive from vg_element_t."
      );
      (elements.emplace_back(new std::remove_reference_t<T>{std::forward<T &&>(_elements)}), ...);
    }

    template<typename T, typename... Args>
    T& draw(Args&&... args) {
      static_assert(
        std::derived_from<std::remove_reference_t<T>, vg_element_t>,
        "Elements must derive from vg_element_t."
      );
      elements.emplace_back(new std::remove_reference_t<T>{std::forward<Args &&>(args)...});
      return *static_cast<T*>(elements.back().get());
    }
  };

  template<typename... T>
  struct accepts_content {
    accepts_content &with(std::vector<std::variant<T...>> _content_) {
      this->content = _content_;
      return *this;
    }

    std::vector<std::variant<T...>> content{};
  };

  // ? Basic Shapes
  // * <pixel>
  struct pixel:
    vg_element_t,
    attrs_core<pixel>,
    attrs_fill<pixel>,
    attr_x<pixel>,
    attr_y<pixel> {
    pixel() = default;

    void apply_to(pixelmap_editor_t &editor) const override {
      auto lg = Cairo::LinearGradient::create(0, 0, 1, 1);

      if (static_cast<size_t>(origin_x + _x) < editor.width && static_cast<size_t>(origin_y + _y) < editor.height) {
        auto clr = sample_fill(_x, _y);
        // editor.img.set(
        //   {(unsigned int) origin_x + _x, (unsigned int) origin_y + _y},
        //   {
        //     (u8) (clr.b * 255),
        //     (u8) (clr.g * 255),
        //     (u8) (clr.r * 255),
        //     (u8) (clr.a * _fill_opacity * 255)
        //   }
        // );
      }
    }

    footprint get_footprint() const override {
      return {_x, _y, 1, 1};
    }
  };

  // * <line>
  struct line:
    vg_element_t,
    attrs_core<line>,
    attrs_stroke<line>,
    attr_x1<line>,
    attr_y1<line>,
    attr_x2<line>,
    attr_y2<line> {
    line() {
      // per element Defaults
      this->stroke_width(1);
    }

    void apply_to(pixelmap_editor_t &editor) const override {
      apply_stroke(editor);

      set_source_to_stroke(editor);

      if ((_stroke_width % 2) != 0) {
        editor->move_to(origin_x + _x1 + 0.5, origin_y + _y1 + 0.5);
        editor->line_to(origin_x + _x2 + 0.5, origin_y + _y2 + 0.5);
      } else {
        editor->move_to(origin_x + _x1, origin_y + _y1);
        editor->line_to(origin_x + _x2, origin_y + _y2);
      }

      editor->stroke();
    }

    footprint get_footprint() const override {
      return {
        std::min(_x1, _x2),
        std::min(_y1, _y2),
        std::abs(_x2 - _x1),
        std::abs(_y2 - _y1),
      };
    }
  };

  // * <polygon>
  struct polygon:
    vg_element_t,
    attrs_core<polygon>,
    attrs_fill<polygon>,
    attrs_stroke<polygon>,
    attr_points<polygon> {
    void apply_to(pixelmap_editor_t &editor) const override {
      apply_stroke(editor);
      apply_fill(editor);

      bool first = true;
      double odd_offset = 0.0;
      if ((_stroke_width % 2) != 0) {
        odd_offset = 0.5;
      }
      for (const auto &p: _points) {
        if (first) {
          editor->move_to(origin_x + p[0] + odd_offset, origin_y + p[1] + odd_offset);
          first = false;
        } else {
          editor->line_to(origin_x + p[0] + odd_offset, origin_y + p[1] + odd_offset);
        }
      }
      // Close the polygon
      editor->close_path();

      set_source_to_fill(editor);
      editor->fill_preserve();

      set_source_to_stroke(editor);
      editor->stroke();
    }

    footprint get_footprint() const override {
      if (_points.empty()) {
        return {0, 0, 0, 0};
      }

      int min_x = _points[0][0];
      int min_y = _points[0][1];
      int max_x = _points[0][0];
      int max_y = _points[0][1];
      int x, y;
      for (std::size_t i = 1; i < _points.size(); ++i) {
        x = _points[i][0];
        y = _points[i][1];
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
      }
      return {
        min_x,
        min_y,
        max_x - min_x,
        max_y - min_y,
      };
    }
  };

  // * <polyline>
  struct polyline:
    vg_element_t,
    attrs_core<polyline>,
    attrs_fill<polyline>,
    attrs_stroke<polyline>,
    attr_points<polyline> {
    void apply_to(pixelmap_editor_t &editor) const override {
      apply_stroke(editor);
      apply_fill(editor);

      bool first = true;
      double odd_offset = 0.0;
      if ((_stroke_width % 2) != 0) {
        odd_offset = 0.5;
      }
      for (const auto &p: _points) {
        if (first) {
          editor->move_to(origin_x + p[0] + odd_offset, origin_y + p[1] + odd_offset);
          first = false;
        } else {
          editor->line_to(origin_x + p[0] + odd_offset, origin_y + p[1] + odd_offset);
        }
      }

      set_source_to_fill(editor);
      editor->fill_preserve();

      set_source_to_stroke(editor);
      editor->stroke();
    }

    footprint get_footprint() const override {
      if (_points.empty()) {
        return {0, 0, 0, 0};
      }

      int min_x = _points[0][0];
      int min_y = _points[0][1];
      int max_x = _points[0][0];
      int max_y = _points[0][1];
      int x, y;
      for (std::size_t i = 1; i < _points.size(); ++i) {
        x = _points[i][0];
        y = _points[i][1];
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
      }
      return {
        min_x,
        min_y,
        max_x - min_x,
        max_y - min_y,
      };
    }
  };

  // * <path>
  struct path:
    vg_element_t,
    attrs_core<path>,
    attrs_fill<path>,
    attrs_stroke<path>,
    attr_path_str<path> {
    void apply_to(pixelmap_editor_t &editor) const override {
      apply_stroke(editor);
      apply_fill(editor);
    }

    footprint get_footprint() const override {
      return {};
    }
  };

  // * <rect>
  struct rect:
    vg_element_t,
    attrs_core<rect>,
    attrs_fill<rect>,
    attrs_stroke<rect>,
    attr_x<rect>,
    attr_y<rect>,
    attr_w<rect>,
    attr_h<rect>,
    attr_rx<rect>,
    attr_ry<rect> {
    void apply_to(pixelmap_editor_t &editor) const override {
      apply_stroke(editor);
      apply_fill(editor);

      // Replace with a proper path construction that takes into account
      // _rx and _ry
      if (_rx == 0 && _ry == 0) {
        double odd_offset = 0.0;
        if ((_stroke_width % 2) != 0) {
          odd_offset = 0.5;
        }
        editor->rectangle(origin_x + _x + odd_offset, origin_y + _y + odd_offset, _w, _h);
        set_source_to_fill(editor);
        editor->fill_preserve();

        set_source_to_stroke(editor);
        editor->stroke();
      } else {
        //editor->rectangle(
        //  _x + _rx,
        //  _y,
        //  _width - _rx * 2,
        //  _height
        //);
        //editor->rectangle(
        //  _x,
        //  _y + _ry,
        //  _rx,
        //  _height - _ry * 2
        //);
        //editor->rectangle(
        //  _width - _rx,
        //  _y + _ry,
        //  _rx,
        //  _height - _ry * 2
        //);
      }
    }

    footprint get_footprint() const override {
      return {_x, _y, _w, _h};
    }
  };

  // * <circle>
  struct circle:
    vg_element_t,
    attrs_core<circle>,
    attrs_fill<circle>,
    attrs_stroke<circle>,
    attr_cx<circle>,
    attr_cy<circle>,
    attr_r<circle> {
    void apply_to(pixelmap_editor_t &editor) const override {
      editor->save();
      apply_stroke(editor);
      apply_fill(editor);

      double odd_offset = 0.0;
      if ((_stroke_width % 2) != 0) {
        odd_offset = 0.5;
      }
      editor->begin_new_path();
      editor->translate(origin_x + _cx + odd_offset, origin_y + _cy + odd_offset);
      editor->arc(0.0, 0.0, _r, 0, 2 * std::numbers::pi);
      set_source_to_fill(editor);
      editor->fill_preserve();

      set_source_to_stroke(editor);
      editor->stroke();
      editor->restore();
    }

    footprint get_footprint() const override {
      return {_cx - _r, _cy - _r, 2 * _r, 2 * _r};
    }
  };
  // * <arc>
  struct arc:
    vg_element_t,
    attrs_core<arc>,
    attrs_fill<arc>,
    attrs_stroke<arc>,
    attr_cx<arc>,
    attr_cy<arc>,
    attr_r<arc>,
    attr_a1<arc>,
    attr_a2<arc> {
    void apply_to(pixelmap_editor_t &editor) const override {
      editor->save();
      apply_stroke(editor);
      apply_fill(editor);

      double odd_offset = 0.0;
      if ((_stroke_width % 2) != 0) {
        odd_offset = 0.5;
      }
      editor->translate(origin_x + _cx + odd_offset, origin_y + _cy + odd_offset);
      //editor->scale(_rx, _ry);
      editor->arc(0.0, 0.0, _r, _a1 * 2 * std::numbers::pi / 360.0, _a2 * 2 * std::numbers::pi / 360.0);

      set_source_to_fill(editor);
      editor->fill_preserve();

      set_source_to_stroke(editor);
      editor->stroke();

      editor->restore();
    }

    footprint get_footprint() const override {
      return {_cx - _r, _cy - _r, 2 * _r, 2 * _r};
    }
  };

  // * <ellipse>
  struct ellipse:
    vg_element_t,
    attrs_core<ellipse>,
    attrs_fill<ellipse>,
    attrs_stroke<ellipse>,
    attr_cx<ellipse>,
    attr_cy<ellipse>,
    attr_rx<ellipse>,
    attr_ry<ellipse> {
    void apply_to(pixelmap_editor_t &editor) const override {
      apply_stroke(editor);
      apply_fill(editor);

      editor->save();
      double odd_offset = 0.0;
      if ((_stroke_width % 2) != 0) {
        odd_offset = 0.5;
      }
      editor->translate(origin_x + _cx + odd_offset, origin_y + _cy + odd_offset);
      editor->scale(_rx, _ry);
      editor->arc(0.0, 0.0, 1.0, 0.0, 2 * std::numbers::pi);

      set_source_to_fill(editor);
      editor->fill_preserve();

      set_source_to_stroke(editor);
      editor->stroke();

      editor->restore();
    }

    footprint get_footprint() const override {
      return {_cx - _rx, _cy - _ry, 2 * _rx, 2 * _ry};
    }
  };

  // ? Group
  // * <g> - Group
  struct group:
    vg_element_t,
    attr_cx<group>,
    attrs_core<group>,
    attrs_fill<ellipse>,
    attrs_stroke<ellipse>,
    accepts_content<line> {
    void apply_to(pixelmap_editor_t &editor) const override {
      apply_stroke(editor);
    }

    footprint get_footprint() const override {
      return {};
    }
  };

  // ? Text
  // * <text>
  struct text:
    vg_element_t,
    attrs_core<text>,
    attrs_fill<text>,
    //attrs_stroke<text>,
    attrs_font<text>,
    attr_x<text>,
    attr_y<text>,
    attr_rotate<text>,
    attr_pivot_x<text>,
    attr_pivot_y<text>,
    attr_scale_x<text>,
    attr_scale_y<text>,
    attr_w<text>,
    attr_h<text> {
    std::string _text;
    explicit text(std::string _text): _text(std::move(_text)) {
    }
    void apply_to(pixelmap_editor_t &editor) const override {
      apply_font(editor);
      //apply_stroke(editor);
      apply_fill(editor);

      Cairo::FontExtents fextents;
      editor->get_font_extents(fextents);

      Cairo::TextExtents extents;
      editor->get_text_extents(_text, extents);

      editor->save();
      editor->translate(origin_x + _x + _pivot_x, origin_y + _y - _pivot_y);
      editor->rotate(_rotate * std::numbers::pi / 180.0);
      editor->scale(_scale_x, _scale_y);
      editor->move_to(-_pivot_x, _pivot_y);
      set_source_to_fill(editor);
      editor->show_text(_text);
      editor->restore();

      //editor->move_to(origin_x + _x, origin_y + _y + fextents.ascent);
      //editor->show_text(_text);
      //editor->move_to(origin_x + _x + 5, origin_y + _y + fextents.height);
      //editor->show_text(_text);
      //editor->move_to(origin_x + _x + 10, origin_y + _y + extents.height);
      //editor->show_text(_text);
    }

    footprint get_footprint() const override {
      pixelmap_t pm{0, 0};
      pixelmap_editor_t pe{pm};

      apply_font(pe);
      apply_fill(pe);

      Cairo::FontExtents fextents;
      pe->get_font_extents(fextents);

      Cairo::TextExtents extents;
      pe->get_text_extents(_text, extents);

      return {
        .x = static_cast<int>(_x),
        .y = static_cast<int>(_y - fextents.ascent),
        .w = (int) extents.x_advance,
        .h = (int) fextents.height,
      };
    }
  };

  // ? Pixelmap
  // * <g> - Pixelmap
  struct pixelmap:
    vg_element_t,
    attrs_core<pixelmap>,
    attr_x<pixelmap>,
    attr_y<pixelmap>,
    attr_rotate<pixelmap>,
    attr_pivot_x<pixelmap>,
    attr_pivot_y<pixelmap>,
    attr_scale_x<pixelmap>,
    attr_scale_y<pixelmap>,
    attr_w<pixelmap>,
    attr_h<pixelmap> {
    const pixelmap_t &_pxm;
    explicit pixelmap(const pixelmap_t &pxm): _pxm(pxm) {
    }

    void apply_to(pixelmap_editor_t &editor) const override {
      // TODO - cairo_format_stride_for_width() should be called before allocating the image buffer and thus use its
      // output to determine the 'width' of the buffer.
      Cairo::RefPtr<Cairo::Surface> surf = Cairo::ImageSurface::create(
        (unsigned char*) this->_pxm.data,
        Cairo::Surface::Format::ARGB32,
        (int) this->_pxm.width(),
        (int) this->_pxm.height(),
        (int) this->_pxm.width() * 4
      );
      editor->save();
      editor->translate(origin_x + _x + _pivot_x, origin_y + _y + _pivot_y);
      editor->rotate(_rotate * std::numbers::pi / 180.0);
      editor->scale(_scale_x, _scale_y);
      editor->set_source(surf, -_pivot_x, -_pivot_y);
      editor->paint_with_alpha(_opacity);
      editor->restore();

      surf->finish();
    }

    footprint get_footprint() const override {
      return {};
    }
  };

  // * <textPath>
  // * <tspan>
}


// * <image>

// * <linearGradient>
// * <radialGradient>

// * <text>
// * <textPath>
// * <tspan>

// * <mask>
// * <clipPath>

// * <defs>

// * <filter>
// * <feBlend>
// * <feColorMatrix>
// * <feComponentTransfer>
// * <feComposite>
// * <feConvolveMatrix>
// * <feDiffuseLighting>
// * <feDisplacementMap>
// * <feDistantLight>
// * <feDropShadow>
// * <feFlood>
// * <feFuncA>
// * <feFuncB>
// * <feFuncG>
// * <feFuncR>
// * <feGaussianBlur>
// * <feImage>
// * <feMerge>
// * <feMergeNode>
// * <feMorphology>
// * <feOffset>
// * <fePointLight>
// * <feSpecularLighting>
// * <feSpotLight>
// * <feTile>
// * <feTurbulence>

// * <animate>
// * <animateMotion>
// * <animateTransform>
// * <stop>

// * <pattern>

// * <svg>
// * <a>
// * <cursor>
// * <foreignObject>
// * <glyph>
// * <glyphRef>
// * <marker>
// * <metadata>
// * <missing-glyph>
// * <mpath>
// * <script>
// * <set>
// * <style>
// * <switch>
// * <symbol>
// * <title> — the SVG accessible name element
// * <desc>
// * <tref>
// * <use>
// * <view>
// * <hkern>
// * <vkern>

// * <font>
// * <font-face-format>
// * <font-face-name>
// * <font-face-src>
// * <font-face-uri>
// * <font-face>
