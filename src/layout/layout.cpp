//
// Created by castle on 8/21/22.
//

#include "components.hpp"
#include "graphics.hpp"
#include "../graphics/events.hpp"

logging::logger log_lay = {
  .name = "LAYOUT", .on = true, .min_level = logging::INFO};

#define COMPUTE(DIM)                                                           \
  if (!dimension_t::compute(DIM)) {                                            \
    return false;                                                              \
  }

static bool compute_dimensions(cydui::components::Component* rt) {
  using namespace cydui::dimensions;
  component_dimensions_t* dim = *(rt->state.let(_(cydui::components::ComponentState *, {
    return &it->dim;
  })).unwrap());
  /// POSITION
  COMPUTE(dim->x)
  COMPUTE(dim->y)
  
  COMPUTE(dim->margin.top)
  COMPUTE(dim->margin.right)
  COMPUTE(dim->margin.bottom)
  COMPUTE(dim->margin.left)
  
  COMPUTE(dim->padding.top)
  COMPUTE(dim->padding.right)
  COMPUTE(dim->padding.bottom)
  COMPUTE(dim->padding.left)
  
  rt->parent.also(_(cydui::components::Component *, {
    it->state.let(_(cydui::components::ComponentState * , {
      dim->cx = it->dim.cx.val() + dim->x.val()
        + dim->margin.left.val() + dim->padding.left.val();
      dim->cy = it->dim.cy.val() + dim->y.val()
      + dim->margin.top.val() + dim->padding.top.val();
    }));
  })) or __((), {
    dim->cx = dim->x.val() + dim->margin.left.val() + dim->padding.left.val();
    dim->cy = dim->y.val() + dim->margin.top.val() + dim->padding.top.val();
  });
  
  if (!dim->given_w) {
    dim->w.unknown = true;
  } else {
    COMPUTE(dim->w)
    dim->cw = dim->w.val() - dim->padding.left.val() - dim->padding.right.val()
      - dim->margin.left.val() - dim->margin.right.val();
  }
  if (!dim->given_h) {
    dim->h.unknown = true;
  } else {
    COMPUTE(dim->h)
    dim->ch = dim->h.val() - dim->padding.top.val() - dim->padding.bottom.val()
      - dim->margin.top.val() - dim->margin.bottom.val();
  }
  
  std::vector<cydui::components::Component*> pending;
  
  dimension_value_t total_w = 0;
  dimension_value_t total_h = 0;
  for (auto &child: rt->children) {
    //compute_dimensions(child);
    // if error (circular dependency), skip for now, and then recalculate
    if (compute_dimensions(child)) {
      component_dimensions_t* c_dim = &(*child->state.unwrap())->dim;
      dimension_value_t child_max_w = c_dim->x.val() + c_dim->w.val();
      dimension_value_t child_max_h = c_dim->y.val() + c_dim->h.val();
      total_w = std::max(total_w, child_max_w);
      total_h = std::max(total_h, child_max_h);
    } else {
      pending.push_back(child);
    }
  }
  
  if (!dim->given_w) {// If not given, or given has error (ie: circular dep)
    dim->cw = total_w;
    dim->w = dim->cw.val() + dim->padding.left.val() + dim->padding.right.val()
      + dim->margin.left.val() + dim->margin.right.val();
  }
  
  if (!dim->given_h) {// If not given, or given has error (ie: circular dep)
    dim->ch = total_h;
    dim->h = dim->ch.val() + dim->padding.top.val() + dim->padding.bottom.val()
      + dim->margin.top.val() + dim->margin.bottom.val();
  }
  
  return std::all_of(pending.begin(), pending.end(), compute_dimensions);
}

#undef COMPUTE

static void redraw_component(
  const cydui::window::CWindow* win, cydui::components::Component* target
) {
  log_lay.debug("REDRAW");
  //auto t0 = std::chrono::system_clock::now().time_since_epoch();
  // Clear render area of component instances
  for (auto child: target->children)
    delete child;
  target->children.clear();
  
  // Recreate those instances with redraw(), this set all size hints relationships
  target->redraw();
  
  if (!compute_dimensions(target)) {
    cydui::components::Component* c = *target->parent.unwrap();
    while (c && !compute_dimensions(c)) {
      if (!c->parent) {
        log_lay.error("Could not compute dimensions");
        // TODO - Catch dimensional error
      }
      c = *c->parent.unwrap();
    }
  }
  //auto t1 = std::chrono::system_clock::now().time_since_epoch();
  //log_lay.debug("TARGET: w  = %d, h  = %d",
  //  target->state->dim.w.val(),
  //  target->state->dim.h.val());
  //log_lay.debug("TARGET: cw = %d, ch = %d",
  //  target->state->dim.cw.val(),
  //  target->state->dim.ch.val());
  
  // Clear screen area
  cydui::graphics::render_target_t* r_target = win->win_ref->render_target;
  
  // Render screen area & flush graphics
  /// We must render from the root, this also re-renders any component that might
  /// occlude the target.
  const cydui::components::Component* root = target;
  while (root->parent) root = *root->parent.unwrap();
  
  root->state.let(_(cydui::components::ComponentState *, {
    cydui::graphics::clr_rect(r_target,
      it->dim.cx.val() - it->dim.padding.left.val(),
      it->dim.cy.val() - it->dim.padding.top.val(),
      it->dim.cw.val() + it->dim.padding.left.val() + it->dim.padding.right.val(),
      it->dim.ch.val() + it->dim.padding.top.val() + it->dim.padding.bottom.val());
  }));
  root->render(r_target);
  
  cydui::graphics::flush(win->win_ref);
  //auto t2 = std::chrono::system_clock::now().time_since_epoch();
  //auto redraw_time = (t1 - t0).count();
  //auto render_time = (t2 - t1).count();
  //log_lay.info("[REDRAW EV] REDRAW=%d, RENDER=%d, f=%f", redraw_time, render_time,
  //  ((double) render_time) / redraw_time);
}

void cydui::layout::Layout::bind_window(cydui::window::CWindow* _win) {
  this->win = _win;
  root->state.let(_(cydui::components::ComponentState *, {
    it->dim.w = win->win_ref->render_target->w;
    it->dim.h = win->win_ref->render_target->h;
    it->dim.given_w = true;
    it->dim.given_h = true;
    
    it->win = _win->win_ref;
  }));
  
  listen(RedrawEvent, {
    if (it.data->win != 0 && it.data->win != win->win_ref->xwin)
      return;
    cydui::components::Component* target = root;
    if (it.data->component) {
      cydui::components::ComponentState* target_state =
        ((cydui::components::ComponentState*) it.data->component);
      cydui::components::Component* specified_target =
        target_state->component_instance;
      if (specified_target)
        target = specified_target;
    }
    
    redraw_component(this->win, target);
  });
  listen(KeyEvent, {
    if (it.data->win != win->win_ref->xwin)
      return;
    if (focused && focused->component_instance) {
      if (focused->focused) {
        if (it.data->pressed) {
          focused->component_instance->on_key_press(*it.data);
        } else if (it.data->released) {
          focused->component_instance->on_key_release(*it.data);
        }
      } else {
        focused = nullptr;
      }
    }
  });
  listen(ButtonEvent, {
    if (it.data->win != win->win_ref->xwin)
      return;
    if (it.data->pressed) {
      cydui::components::Component* target = root;
      cydui::components::Component* specified_target =
        find_by_coords(root, it.data->x, it.data->y);
      if (specified_target)
        target = specified_target;
      
      int rel_x;
      int rel_y;
      target->state.let(__((cydui::components::ComponentState * st), {
        rel_x = it.data->x - st->dim.cx.val();
        rel_y = it.data->y - st->dim.cy.val();
        
        if (focused != *target->state.unwrap()) {
          if (focused && focused->component_instance) {
            focused->focused = false;
            focused = nullptr;
          }
          focused = *target->state.unwrap();
          focused->focused = true;
        }
      }));
      
      target->on_mouse_click(rel_x, rel_y, it.data->button);
      if (render_if_dirty(root))
        graphics::flush(win->win_ref);
    }
  });
  listen(ScrollEvent, {
    if (it.data->win != win->win_ref->xwin)
      return;
    cydui::components::Component* target = root;
    cydui::components::Component* specified_target =
      find_by_coords(root, it.data->x, it.data->y);
    if (specified_target)
      target = specified_target;
    
    target->on_scroll(it.data->dx, it.data->dy);
    if (render_if_dirty(root))
      graphics::flush(win->win_ref);
  });
  listen(MotionEvent, {
    if (it.data->win != win->win_ref->xwin)
      return;
    
    if (it.data->x == -1 && it.data->y == -1) {
      if (hovering && hovering->component_instance) {
        int exit_rel_x = 0;
        int exit_rel_y = 0;
        hovering->component_instance->on_mouse_exit(exit_rel_x, exit_rel_y);
        hovering = nullptr;
      }
    } else {
      cydui::components::Component* target = root;
      cydui::components::Component* specified_target =
        find_by_coords(root, it.data->x, it.data->y);
      if (specified_target)
        target = specified_target;
      
      if (hovering != *target->state.unwrap()) {
        if (hovering && hovering->component_instance) {
          int exit_rel_x = it.data->x - (*target->state.unwrap())->dim.cx.val();
          int exit_rel_y = it.data->y - (*target->state.unwrap())->dim.cy.val();
          hovering->component_instance->on_mouse_exit(exit_rel_x, exit_rel_y);
        }
        hovering = *target->state.unwrap();
        
        int rel_x = it.data->x - (*target->state.unwrap())->dim.cx.val();
        int rel_y = it.data->y - (*target->state.unwrap())->dim.cy.val();
        target->on_mouse_enter(rel_x, rel_y);
      } else {
        int rel_x = it.data->x - (*target->state.unwrap())->dim.cx.val();
        int rel_y = it.data->y - (*target->state.unwrap())->dim.cy.val();
        target->on_mouse_motion(rel_x, rel_y);
      }
    }
    
    // Calling 'Drag' related event handlers
    cydui::components::Component* target = root;
    cydui::components::Component* specified_target =
      find_by_coords(root, it.data->x, it.data->y);
    if (specified_target)
      target = specified_target;
    
    if (it.data->dragging) {
      if (dragging) {
        int rel_x = it.data->x - (*target->state.unwrap())->dim.cx.val();
        int rel_y = it.data->y - (*target->state.unwrap())->dim.cy.val();
        target->on_drag_motion(rel_x, rel_y);
      } else {
        int rel_x = it.data->x - (*target->state.unwrap())->dim.cx.val();
        int rel_y = it.data->y - (*target->state.unwrap())->dim.cy.val();
        target->on_drag_start(rel_x, rel_y);
        dragging = true;
      }
    } else if (dragging) {
      int rel_x = it.data->x - (*target->state.unwrap())->dim.cx.val();
      int rel_y = it.data->y - (*target->state.unwrap())->dim.cy.val();
      target->on_drag_finish(rel_x, rel_y);
      dragging = false;
    }
    
    if (render_if_dirty(root))
      graphics::flush(win->win_ref);
  });
  listen(ResizeEvent, {
    if (it.data->win != win->win_ref->xwin)
      return;
    log_lay.debug("RESIZE w=%d, h=%d", it.data->w, it.data->h);
    
    (*root->state.unwrap())->dim.w = it.data->w;
    (*root->state.unwrap())->dim.h = it.data->h;
    (*root->state.unwrap())->dim.given_w = true;
    (*root->state.unwrap())->dim.given_h = true;
    
    redraw_component(this->win, root);
    //if (render_if_dirty(root))
    //    graphics::flush(win->win_ref);
  });
}

bool cydui::layout::Layout::render_if_dirty(cydui::components::Component* c) {
  if ((*c->state.unwrap())->_dirty) {
    redraw_component(this->win, c);
    return true;
  } else {
    bool any = false;
    for (auto &item: c->children)
      any = render_if_dirty(item) || any;// F**K, order here matters
    return any;
  }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma ide diagnostic ignored "modernize-loop-convert"

cydui::components::Component* cydui::layout::Layout::find_by_coords(
  components::Component* c, int x, int y
) {
  components::Component* target = nullptr;
  c->state.let(_(components::ComponentState*, {
    if (it->sub_render_target) {
      x += it->sub_render_event_offset.first;
      y += it->sub_render_event_offset.second;
    }
  }));
  for (auto i = c->children.rbegin(); i != c->children.rend(); ++i) {
    auto* item = *i;
    if (x >= (*item->state.unwrap())->dim.cx.val()
      && x < ((*item->state.unwrap())->dim.cx.val() + (*item->state.unwrap())->dim.cw.val())
      && y >= (*item->state.unwrap())->dim.cy.val()
      && y < ((*item->state.unwrap())->dim.cy.val() + (*item->state.unwrap())->dim.ch.val())) {
      target = find_by_coords(item, x, y);
      if (target)
        break;
    }
  }
  if (target)
    return target;
  
  if (!(*c->state.unwrap())->stateless_comp && x >= (*c->state.unwrap())->dim.cx.val()
    && x < ((*c->state.unwrap())->dim.cx.val() + (*c->state.unwrap())->dim.cw.val())
    && y >= (*c->state.unwrap())->dim.cy.val()
    && y < ((*c->state.unwrap())->dim.cy.val() + (*c->state.unwrap())->dim.ch.val())) {
    target = c;
  }
  return target;
}

#pragma clang diagnostic pop
