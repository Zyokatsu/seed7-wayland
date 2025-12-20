#ifndef INCLUDE_XDG_H
#define INCLUDE_XDG_H
#include "xdg_shell_client_protocol.h"

void xdg_surface_configure (void *data, struct xdg_surface *xdg_surface, uint32_t serial);
void xdg_wm_base_ping (void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
void xdg_toplevel_close (void *data, struct xdg_toplevel *xdg_toplevel);
void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int width, int height, struct wl_array *states);

extern const struct xdg_surface_listener xdgSurfaceListener;
extern const struct xdg_toplevel_listener xdgToplevelListener;
extern const struct xdg_wm_base_listener xdgWindowListener;
#endif