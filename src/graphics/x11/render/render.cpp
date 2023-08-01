//
// Created by castle on 8/23/22.
//

#include "render.hpp"
#include "../state/state.hpp"
#include "cyd-log/dist/include/logging.hpp"
#include "x11_impl.hpp"

#include <X11/Xlib.h>

logging::logger xlog_ctrl = {.name = "X11::RENDER::CTRL", .on = false};
logging::logger xlog_task = {.name = "X11::RENDER::TASK", .on = true};

static void render_requests(cydui::graphics::window_t* win) {
  if (!win->staging_reqs) return;
  //xlog_task.info("rendering requests %zu", win->staging_reqs->size());
  auto* node = win->staging_reqs->begin();
  while (node != nullptr) {
    auto* r = node;
    switch (r->type) {
      case render_req_type_e::LINE:
        render::drw_line(
          r->line.target,
          r->line.color,
          r->line.x,
          r->line.y,
          r->line.x1,
          r->line.y1
        );
        break;
      case render_req_type_e::RECTANGLE:
        render::drw_rect(
          r->rect.target,
          r->rect.color,
          r->rect.x,
          r->rect.y,
          r->rect.w,
          r->rect.h,
          r->rect.filled
        );
        break;
      case render_req_type_e::ARC:
        render::drw_arc(
          r->arc.target,
          r->arc.color,
          r->arc.x,
          r->arc.y,
          r->arc.w,
          r->arc.h,
          r->arc.a0,
          r->arc.a1,
          r->arc.filled
        );
        break;
      case render_req_type_e::TEXT:if (r->text.text == nullptr) break;
        render::drw_text(
          r->text.target,
          *r->text.font,
          r->text.color,
          r->text.text->c_str(),
          r->text.x,
          r->text.y
        );
        delete r->text.text;
        break;
      case render_req_type_e::IMAGE:
        render::drw_image(
          r->img.target,
          r->img.image,
          r->img.x,
          r->img.y,
          r->img.w,
          r->img.h
        );
        break;
      case render_req_type_e::TARGET:
        render::drw_target(
          r->target.target,
          r->target.source_target,
          r->target.xs,
          r->target.ys,
          r->target.xd,
          r->target.yd,
          r->target.w,
          r->target.h
        );
        break;
      case render_req_type_e::FLUSH://render::flush(win);
        break;
    }
    node = node->next;
  }
  win->staging_reqs->clear();
}

void render_sbr(cydui::graphics::window_t* win) {
  //window_render_req req;
  
  win->render_mtx.lock();
  bool dirty = win->dirty;
  win->dirty = false;
  
  if (!dirty || win->staging_reqs->empty()) {
    win->render_mtx.unlock();
    return;
  } else {
    render_requests(win);
    win->render_mtx.unlock();
  }
  
  //win->x_mtx.lock();
  //win->render_mtx.lock();
  XCopyArea(state::get_dpy(),
    win->render_target->drawable,
    win->staging_target->drawable,
    win->render_target->gc,
    0,
    0,
    win->render_target->w,
    win->render_target->h,
    0,
    0);
  XCopyArea(state::get_dpy(),
    win->staging_target->drawable,
    win->xwin,
    win->staging_target->gc,
    0,
    0,
    win->staging_target->w,
    win->staging_target->h,
    0,
    0);
  //win->render_mtx.unlock();
  //win->x_mtx.unlock();
  XFlush(state::get_dpy());
}

using namespace std::chrono_literals;

void render_task(cydui::threading::thread_t* this_thread) {
  xlog_task.debug("Started render thread");
  auto* render_data = (render::RenderThreadData*) this_thread->data;
  while (this_thread->running) {
    auto t1 = std::chrono::high_resolution_clock::now();
    render_sbr(render_data->win);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    //auto us = dt.count();
    //if (us > 0) {
    //  xlog_task.debug("RENDER: %ld us | FPS: %ld", us, 1000000 / std::max(16000L, us));
    //}
    
    std::this_thread::sleep_for(16ms - dt);
  }
}

void render::start(cydui::graphics::window_t* win) {
  if (win->render_thd && win->render_thd->running)
    return;
  xlog_ctrl.debug("Starting render thread");
  
  win->render_data = new RenderThreadData {
    .win = win,
  };
  win->render_thd = cydui::threading::new_thread(render_task, win->render_data)
    ->set_name("RENDER_THD");
}

static void req(cydui::graphics::window_t* win, int x, int y, int w, int h) {
  win->render_mtx.lock();
  win->dirty = true;
  win->staging_reqs->append(*win->render_reqs);
  win->render_mtx.unlock();
}

static std::unordered_map <u32, XColor> xcolor_cache;

struct xcolor_hot_cache_entry_t {
  u32 id;
  XColor xcolor;
};
static constexpr u32 xcolor_hot_cache_size = 8;
static xcolor_hot_cache_entry_t xcolor_hot_cache[xcolor_hot_cache_size];
static u32 xcolor_hot_cache_current_insert_index = 0U;

XColor color_to_xcolor(color::Color color) {
  u32 color_id = color.to_id();
  
  for (auto &entry: xcolor_hot_cache) {
    if (entry.id == color_id) {
      return entry.xcolor;
    }
  }
  
  if (xcolor_cache.contains(color_id)) {
    auto &c = xcolor_cache[color_id];
    xcolor_hot_cache[xcolor_hot_cache_current_insert_index++] = {
      .id = color_id,
      .xcolor = c,
    };
    if (xcolor_hot_cache_current_insert_index >= xcolor_hot_cache_size) {
      xcolor_hot_cache_current_insert_index = 0U;
    }
    return c;
  }
  
  Colormap map = DefaultColormap(state::get_dpy(), state::get_screen());
  XColor c;
  XParseColor(state::get_dpy(), map, color.to_string().c_str(), &c);
  XAllocColor(state::get_dpy(), map, &c);
  
  xcolor_hot_cache[xcolor_hot_cache_current_insert_index++] = {
    .id = color_id,
    .xcolor = c,
  };
  if (xcolor_hot_cache_current_insert_index >= xcolor_hot_cache_size) {
    xcolor_hot_cache_current_insert_index = 0U;
  }
  
  xcolor_cache[color_id] = c;
  return c;
}

static std::unordered_map<u32, XftColor*> xftcolor_cache;

XftColor* color_to_xftcolor(color::Color color) {
  u32 color_id = color.to_id();
  if (xftcolor_cache.contains(color_id))
    return xftcolor_cache[color_id];
  
  auto* c = new XftColor;
  
  if (!XftColorAllocName(state::get_dpy(),
    DefaultVisual(state::get_dpy(), state::get_screen()),
    DefaultColormap(state::get_dpy(), state::get_screen()),
    color.to_string().c_str(),
    c)) {
    xlog_ctrl.error("Cannot allocate color %s", color.to_string().c_str());
  }
  
  xftcolor_cache[color_id] = c;
  return c;
}

void render::resize(cydui::graphics::render_target_t* target, int w, int h) {
  if (w != target->w || h != target->h) {
    Drawable new_drw = XCreatePixmap(state::get_dpy(),
      target->win->xwin,
      w,
      h,
      DefaultDepth(state::get_dpy(), state::get_screen()));
    XSetForeground(state::get_dpy(),
      target->gc,
      BlackPixel(state::get_dpy(), state::get_screen()));
    XFillRectangle(state::get_dpy(), new_drw, target->gc, 0, 0, w, h);
    
    //target->win->x_mtx.lock();
    XCopyArea(state::get_dpy(),
      target->drawable,
      new_drw,
      target->gc,
      0,
      0,
      target->w,
      target->h,
      0,
      0);
    
    XFreePixmap(state::get_dpy(), target->drawable);
    XFreeGC(state::get_dpy(), target->gc);
    
    target->drawable = new_drw;
    target->gc = XCreateGC(state::get_dpy(), target->drawable, 0, NULL);
    
    //target->win->x_mtx.unlock();
    
    XFlush(state::get_dpy());
    
    target->w = w;
    target->h = h;
    req(target->win, 0, 0, 0, 0);
  }
}

void render::clr_rect(
  cydui::graphics::render_target_t* target,
  int x,
  int y,
  unsigned int w,
  unsigned int h
) {
  //target->win->x_mtx.lock();
  
  XSetForeground(state::get_dpy(),
    target->gc,
    BlackPixel(state::get_dpy(), state::get_screen()));
  XFillRectangle(state::get_dpy(), target->drawable, target->gc, x, y, w, h);
  
  //target->win->x_mtx.unlock();
  
  //req(win, x, y, (int)w + 1, (int)h + 1);// added 1 margin for lines
}

void render::flush(cydui::graphics::window_t* win) {
  req(win, 0, 0, 0, 0);
}

void render::drw_line(
  cydui::graphics::render_target_t* target,
  color::Color color,
  int x,
  int y,
  int x1,
  int y1
) {
  //target->win->x_mtx.lock();
  
  auto xc = color_to_xcolor(color).pixel;
  XSetForeground(state::get_dpy(), target->gc, xc);
  XSetBackground(state::get_dpy(),
    target->gc,
    BlackPixel(state::get_dpy(), state::get_screen()));
  XDrawLine(state::get_dpy(), target->drawable, target->gc, x, y, x1, y1);
  
  //target->win->x_mtx.unlock();
  
  //req(win, x, y, x1 - x + 1, y1 - y + 1);// added 1 margin for lines
}

void render::drw_rect(
  cydui::graphics::render_target_t* target,
  color::Color color,
  int x,
  int y,
  int w,
  int h,
  bool filled
) {
  //target->win->x_mtx.lock();
  auto dpy = state::get_dpy();
  XSetState(dpy, target->gc, color_to_xcolor(color).pixel, BlackPixel(dpy, state::get_screen()), GXcopy,
    AllPlanes);
  if (filled) {
    XFillRectangle(dpy, target->drawable, target->gc, x, y, w, h);
  } else {
    XDrawRectangle(dpy, target->drawable, target->gc, x, y, w, h);
  }
  //target->win->x_mtx.unlock();
  
  //req(win, x, y, w + 1, h + 1);// added 1 margin for lines
}

void render::drw_arc(
  cydui::graphics::render_target_t* target,
  color::Color color,
  int x,
  int y,
  int w,
  int h,
  int a0,
  int a1,
  bool filled
) {
  //target->win->x_mtx.lock();
  XSetForeground(state::get_dpy(), target->gc, color_to_xcolor(color).pixel);
  XSetBackground(state::get_dpy(),
    target->gc,
    BlackPixel(state::get_dpy(), state::get_screen()));
  if (filled) {
    XFillArc(state::get_dpy(),
      target->drawable,
      target->gc,
      x,
      y,
      w,
      h,
      a0 * 64,
      a1 * 64);
  } else {
    XDrawArc(state::get_dpy(),
      target->drawable,
      target->gc,
      x,
      y,
      w,
      h,
      a0 * 64,
      a1 * 64);
  }
  //target->win->x_mtx.unlock();
  //req(win, x, y, w + 1, h + 1);// added 1 margin for lines
}

void render::drw_text(
  cydui::graphics::render_target_t* target,
  window_font font,
  color::Color color,
  const str &text,
  int x,
  int y
) {
  XColor c = color_to_xcolor(color);
  XftColor* xft_c = color_to_xftcolor(color);
  XftDraw* xft_draw = XftDrawCreate(state::get_dpy(),
    target->drawable,
    DefaultVisual(state::get_dpy(), state::get_screen()),
    DefaultColormap(state::get_dpy(), state::get_screen()));
  XGlyphInfo x_glyph_info;
  int w, h;
  
  //target->win->x_mtx.lock();
  
  XSetForeground(state::get_dpy(), target->gc, c.pixel);
  XSetBackground(state::get_dpy(),
    target->gc,
    BlackPixel(state::get_dpy(), state::get_screen()));
  
  XftDrawStringUtf8(
    xft_draw, xft_c, font.xfont, x, y, (FcChar8*) text.c_str(), text.size());
  XftTextExtentsUtf8(state::get_dpy(),
    font.xfont,
    (XftChar8*) text.c_str(),
    text.size(),
    &x_glyph_info);
  w = x_glyph_info.xOff;
  h = x_glyph_info.yOff;
  
  XftDrawDestroy(xft_draw);
  
  //target->win->x_mtx.unlock();
  
  //req(win, x, y, w + 1, h + 1);// added 1 margin for lines
}

void render::drw_image(
  cydui::graphics::render_target_t* target,
  window_image img,
  int x,
  int y,
  int w,
  int h
) {
  //target->win->x_mtx.lock();
  
  Pixmap tmp_pixmap = XCreatePixmap(state::get_dpy(),
    target->drawable,
    img.ximg->width,
    img.ximg->height,
    DefaultDepth(state::get_dpy(), state::get_screen()));
  
  GC tmp_gc = XCreateGC(state::get_dpy(), tmp_pixmap, 0, nullptr);
  
  XPutImage(state::get_dpy(),
    tmp_pixmap,
    tmp_gc,
    img.ximg,
    0,
    0,
    0,
    0,
    img.ximg->width,
    img.ximg->height);
  
  XRenderPictureAttributes p;
  XRenderPictFormat* p_fmt = XRenderFindVisualFormat(
    state::get_dpy(), DefaultVisual(state::get_dpy(), state::get_screen()));
  Picture src_p =
    XRenderCreatePicture(state::get_dpy(), tmp_pixmap, p_fmt, 0, &p);
  Picture dst_p =
    XRenderCreatePicture(state::get_dpy(), target->drawable, p_fmt, 0, &p);
  
  double x_scl = (double) img.ximg->width / w;
  double y_scl = (double) img.ximg->height / h;
  XTransform tr = {
    {
      {XDoubleToFixed(x_scl), XDoubleToFixed(0), XDoubleToFixed(0)},
      {XDoubleToFixed(0), XDoubleToFixed(y_scl), XDoubleToFixed(0)},
      {XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(1.0)},
    }
  };
  XRenderSetPictureTransform(state::get_dpy(), src_p, &tr);
  
  int final_w = x + w > target->w ? target->w - x : w;
  int final_h = y + h > target->h ? target->h - y : h;
  
  XRenderComposite(state::get_dpy(),
    PictOpSrc,
    src_p,
    0,
    dst_p,
    0,
    0,
    0,
    0,
    x,
    y,
    final_w,
    final_h);
  
  XSync(state::get_dpy(), False);
  
  XRenderFreePicture(state::get_dpy(), src_p);
  XRenderFreePicture(state::get_dpy(), dst_p);
  XFreeGC(state::get_dpy(), tmp_gc);
  XFreePixmap(state::get_dpy(), tmp_pixmap);
  
  //target->win->x_mtx.unlock();
  //req(win, x, y, w + 1, h + 1);// added 1 margin for lines
}

void render::drw_target(
  cydui::graphics::render_target_t* dest_target,
  cydui::graphics::render_target_t* source_target,
  int xs,
  int ys,
  int xd,
  int yd,
  int w,
  int h
) {
  //dest_target->win->x_mtx.lock();
  XCopyArea(state::get_dpy(),
    source_target->drawable,
    dest_target->drawable,
    dest_target->gc,
    xs,
    ys,
    w,
    h,
    xd,
    yd);
  //dest_target->win->x_mtx.unlock();
  //req(win, x, y, w + 1, h + 1);// added 1 margin for lines
}
