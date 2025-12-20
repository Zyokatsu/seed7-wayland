#include "output.h"
#include "state.h"

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
)
{ /* Required, but we're not using it. */ }

void wayland_output_done (void *data, struct wl_output *wl_output)
{ /* Required, but we're not using it. */ }

void wayland_output_mode
( void *data,
  struct wl_output *wl_output,
  int unsigned flags,
  int width,
  int height,
  int refresh // Refresh rate, seems to be Hz * 1000
)
{
  struct ClientState *state = data;
  state->outputWidth = width;
  state->outputHeight = height;
}

void wayland_output_named (void *data, struct wl_output *wl_output, const char *name)
{ /* Required, but we're not using it. */ }

void wayland_output_described (void *data, struct wl_output *wl_output, const char *description)
{ /* Required, but we're not using it. */ }

void wayland_output_scaled (void *data, struct wl_output *wl_output, int factor)
{ /* Required, but we're not using it. */ }

// Listeners for output events (v4), see the wayland.xml file (possibly /usr/share/wayland/wayland.xml).
const struct wl_output_listener waylandOutputListener =
{ .geometry = wayland_output_geometry,
  .mode = wayland_output_mode,
  .done = wayland_output_done,
  .scale = wayland_output_scaled,
  .name = wayland_output_named,
  .description = wayland_output_described
};