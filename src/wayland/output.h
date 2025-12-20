#ifndef INCLUDE_OUTPUT_H
#define INCLUDE_OUTPUT_H
#include <wayland-client.h>

/*- Wayland Output (eg monitor) --------
--------------------------------------*/
void wayland_output_geometry
( void* data,
  struct wl_output* wl_output,
  int x,
  int y,
  int physical_width,  // in millimetres.
  int physical_height, // in millimetres.
  int subpixel,
  const char *make,
  const char *model,
  int transform
);

void wayland_output_done (void *data, struct wl_output *wl_output);

void wayland_output_mode
( void *data,
  struct wl_output *wl_output,
  int unsigned flags,
  int width,
  int height,
  int refresh // Refresh rate, seems to be Hz * 1000
);

void wayland_output_named (void *data, struct wl_output *wl_output, const char *name);
void wayland_output_described (void *data, struct wl_output *wl_output, const char *description);
void wayland_output_scaled (void *data, struct wl_output *wl_output, int factor);

extern const struct wl_output_listener waylandOutputListener;
#endif