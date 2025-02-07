// Copyright (c) 2024, Víctor Castillo Agüero.
// SPDX-License-Identifier: GPL-3.0-or-later

module;
#include "../components/component_macros.h"

export module cydui.std;

import std;
export import cydui;

export import cydui.std.structural;
export import cydui.std.input;
export import cydui.std.charts;

// namespace stdui {
// #define ATTR_PROP(PROP)                                                                            \
//   auto& PROP(decltype(props_t::PROP)& _##PROP) {                                                   \
//     props.PROP = _##PROP;                                                                          \
//     return *this;                                                                                  \
//   }                                                                                                \
//   auto& PROP(decltype(props_t::PROP)&& _##PROP) {                                                  \
//     props.PROP = _##PROP;                                                                          \
//     return *this;                                                                                  \
//   }

//   namespace layout {
//     struct COMPONENT(
//       Flow,
//       ;
//       enum class Direction{
//         HORIZONTAL,
//         VERTICAL,
//       };

//       struct props_t {
//         Direction                                   dir     = Direction::HORIZONTAL;
//         cyd::ui::dimensions::dimensional_relation_t spacing = 0;
//       };
//       ATTR_PROP(dir) ATTR_PROP(spacing)

//         auto& horizontal() {
//           props.dir = Direction::HORIZONTAL;
//           return *this;
//         } auto& vertical() {
//           props.dir = Direction::VERTICAL;
//           return *this;
//         }

//     ) {
//       void horizontal_fixed_width() {
//         std::unordered_set<cyd::ui::dimensions::dimension_t*> deps{};
//         deps.insert(&attrs->_w);
//         auto chi = $children;

//         for (auto& child: chi) {
//           deps.insert(&child->attrs()->_w);
//         }
//         for (auto& child: chi) {
//           auto ats = child->attrs();
//           ats->h($height);
//           ats->x(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               int total_w = 0;
//               for (const auto& dim: deps) {
//                 if (dim != &attrs->_w) {
//                   // avoid summing the container's width
//                   total_w += dim->val();
//                 }
//               }
//               int spacing = std::max(0, $width.val() - total_w) / ((int)deps.size() - 1);

//               int res = 0;
//               for (const auto& dim: deps) {
//                 res += dim->val() + spacing;
//               }
//               return res;
//             },
//             deps
//           });
//         }
//       }
//       void vertical_fixed_height() {
//         std::unordered_set<cyd::ui::dimensions::dimension_t*> deps{};
//         deps.insert(&attrs->_h);
//         auto chi = $children();

//         for (auto& child: chi) {
//           deps.insert(&child->attrs()->_h);
//         }
//         int index = 0;
//         for (auto& child: chi) {
//           auto ats = child->attrs();
//           ats->w($width);
//           ats->y(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               int total_h = 0;
//               for (const auto& dim: deps) {
//                 if (dim != &attrs->_h) { // avoid summing the container's width
//                   total_h += dim->val();
//                 }
//               }
//               int spacing = std::max(0, $height.val() - total_h) / ((int)deps.size() - 1);

//               int res = 0;
//               int i   = 0;
//               for (const auto& dim: deps) {
//                 if (i == index)
//                   break;
//                 if (dim != &attrs->_h) { // avoid summing the container's width
//                   res += dim->val() + spacing;
//                 }
//                 i++;
//               }
//               return res;
//             },
//             deps
//           });
//           index++;
//         }
//       }
//       void horizontal_free_width() {
//         auto* spacing = new cyd::ui::dimensions::dimension_t{props.spacing};
//         std::unordered_set<cyd::ui::dimensions::dimension_t*> deps{};
//         deps.insert(spacing);
//         auto chi = $children();
//         for (auto& child: chi) {
//           auto ats = child->attrs();
//           ats->h($height);
//           ats->x(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               int res = 0;
//               for (const auto& dim: deps) {
//                 if (dim != spacing) {
//                   res += dim->val() + spacing->val();
//                 }
//               }
//               return res;
//             },
//             deps
//           });

//           deps.insert(&child->attrs()->_w);
//         }
//       }
//       void vertical_free_height() {
//         auto* spacing = new cyd::ui::dimensions::dimension_t{props.spacing};
//         std::unordered_set<cyd::ui::dimensions::dimension_t*> deps{};
//         deps.insert(spacing);
//         auto chi = $children();
//         for (auto& child: chi) {
//           auto ats = child->attrs();
//           ats->w($width);
//           ats->y(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               int res = 0;
//               for (const auto& dim: deps) {
//                 if (dim != spacing) {
//                   res += dim->val() + spacing->val();
//                 }
//               }
//               return res;
//             },
//             deps
//           });

//           deps.insert(&child->attrs()->_h);
//         }
//       }

//       ON_REDRAW {
//         switch (props.dir) {
//           case Flow::Direction::HORIZONTAL:
//             if (attrs->_w_has_changed) {
//               horizontal_fixed_width();
//             } else {
//               horizontal_free_width();
//             }
//             break;
//           case Flow::Direction::VERTICAL:
//             if (attrs->_h_has_changed) {
//               vertical_fixed_height();
//             } else {
//               vertical_free_height();
//             }
//             break;
//         }

//         return {};
//       }
//     };

//     struct COMPONENT(
//       Grid,
//       {
//         unsigned int rows;
//         unsigned int cols;
//         int          x_gap = 0;
//         int          y_gap = 0;
//       };
//       ATTR_PROP(rows) ATTR_PROP(cols) ATTR_PROP(x_gap) ATTR_PROP(y_gap) STATE {
//         std::unordered_map<cyd::ui::components::component_base_t*, std::pair<int, int>> positions{};
//         std::unordered_map<cyd::ui::components::component_base_t*, std::pair<int, int>> sizes{};
//       }
//     ) {
//       ON_REDRAW {
//         state.positions.clear();
//         state.sizes.clear();
//         auto                                                  chi = $children();
//         std::unordered_set<cyd::ui::dimensions::dimension_t*> deps{};
//         deps.insert(&attrs->_w);
//         deps.insert(&attrs->_h);

//         for (auto& child: chi) {
//           auto ats                = child->attrs();
//           state.positions[child] = {ats->_x, ats->_y};
//           state.sizes[child]     = {ats->_w, ats->_h};

//           ats->x(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               auto [grid_x, _] = state.positions[child];
//               int total_w      = attrs->_w;
//               int step         = total_w / (int)props.cols;
//               return grid_x * step + (props.x_gap / 2);
//             },
//             deps
//           });
//           ats->y(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               auto [_, grid_y] = state.positions[child];
//               int total_h      = attrs->_h;
//               int step         = total_h / (int)props.rows;
//               return grid_y * step + (props.y_gap / 2);
//             },
//             deps
//           });
//           ats->w(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               auto [grid_w, _] = state.sizes[child];
//               int total_w      = attrs->_w;
//               int step         = total_w / (int)props.cols;
//               return grid_w * step - props.x_gap;
//             },
//             deps
//           });
//           ats->h(cyd::ui::dimensions::dimensional_relation_t{
//             [=, this]() {
//               auto [_, grid_h] = state.sizes[child];
//               int total_h      = attrs->_h;
//               int step         = total_h / (int)props.rows;
//               return grid_h * step - props.y_gap;
//             },
//             deps
//           });
//         }
//         return {};
//       }
//     };
//   }

//   template<typename F>
//   using fun = std::function<F>;

//   namespace control {
//     // Button
//     struct COMPONENT(
//       Button,
//       {
//       fun<void(int)> on_click;
//       }
//     ) {
//     };
//     // Toggle Button (checkbox, radial toggle,...)
//     // Slider
//   }

//   namespace media {
//     // Image
//     // Video / Video Player
//     // Audio / Audio Player
//   }

//   using namespace layout;
// }
