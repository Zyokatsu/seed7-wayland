#ifndef INCLUDE_BUFFER_H
#define INCLUDE_BUFFER_H
#include "state.h"
#include "window.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <wayland-client.h>

struct BufferData
{ uint32_t *content;
  size_t contentSize;
  struct wl_buffer *waylandData;
  int width;
  int height;
};

bool prepare_buffer_data (struct ClientState *state, way_winType window);
boolType prepare_buffer_copy (struct ClientState *state, way_winType window);

extern const struct wl_buffer_listener waylandBufferListener;
#endif