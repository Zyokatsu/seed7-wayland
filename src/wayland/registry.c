#include "registry.h"
#include "output.h"
#include "seat.h"
#include "state.h"
#include "xdg.h"
#include "xdg_decorations_unstable.h"
#include <string.h>

/*- Wayland Registry -----------------------------------------------------------
  "Globals" are bound here, so that their events can be detected.
------------------------------------------------------------------------------*/
void wayland_register_global
( void *data,
  struct wl_registry *registry,
  uint32_t name,
  const char *interface,
  uint32_t version
)
{
  struct ClientState *state = data;

  // Capture output events.
  if (strcmp(interface, wl_output_interface.name) == 0)
  { state->output = wl_registry_bind(registry, name, &wl_output_interface, 4); // Library function, last argument is the version for this interface (4, in this case).
    wl_output_add_listener(state->output, &waylandOutputListener, &waylandState);
  }
  else // Capture shared memory object events.
  if (strcmp(interface, wl_shm_interface.name) == 0)
    state->sharedMemory = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  else // Capture compositor events (windows).
  if (strcmp(interface, wl_compositor_interface.name) == 0)
    state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 6);
  else // Capture sub-compositor events (sub-windows).
  if (strcmp(interface, wl_subcompositor_interface.name) == 0)
    state->subCompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
  else // Capture cross-desktop-group window events (application windows).
  if (strcmp(interface, xdg_wm_base_interface.name) == 0)
  { state->xdgWindow = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(state->xdgWindow, &xdgWindowListener, state); // Library function.
  }
  else // Window close requests, resizes, etc.
  if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
    state->xdgDecorations = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
  else // Input events.
  if (strcmp(interface, wl_seat_interface.name) == 0)
  { state->wlSeat = wl_registry_bind(registry, name, &wl_seat_interface, 9);
    wl_seat_add_listener(state->wlSeat, &wlSeatListener, state);
  }
}

void wayland_unregister_global (void *data, struct wl_registry *registry, uint32_t name)
{
  // This function is required by Wayland, but doesn't need to do anything for our purposes.
}

const struct wl_registry_listener waylandRegistryListener =
{ .global = wayland_register_global,
  .global_remove = wayland_unregister_global
};