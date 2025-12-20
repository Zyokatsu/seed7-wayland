#include <stdio.h>
#include "xdg.h"
#include "state.h"
#include "window.h"
#include "keyboard_globals.h"

/*- Wayland XDG ----------------------------------------------------------------
  XDG enables/handles the application windows.
------------------------------------------------------------------------------*/
/* The configure event will get called when the server informs the client (us)
about a configuration or reconfiguration of a surface. In practice, this happens
when a surface is created or resized, and whenever our window's active state is
toggled (via alt-tab and the like). */
void xdg_surface_configure (void *data, struct xdg_surface *surface, uint32_t serial)
{
  // struct ClientState *state = data;
  way_winType window = (way_winType) data;
  xdg_surface_ack_configure(surface, serial); // Library function, acknowledges configuration (Wayland Book 7.1)
  printf("Configure xdg: %d x %d    %d x %d\n", window->width, window->height, window->pendingWidth, window->pendingHeight);

  if
  ( window->pendingWidth > 0 && window->pendingWidth != window->width ||
    window->pendingHeight > 0 && window->pendingHeight != window->height
  )
  { resizeWindow(window, window->pendingWidth, window->pendingHeight, TRUE);
    window->pendingWidth = window->width;
    window->pendingHeight = window->height;
  }
  else
    redrawWindow(window);
}

const struct xdg_surface_listener xdgSurfaceListener =
{ .configure = xdg_surface_configure };

// Ping/pong lets the compositor know we aren't deadlocked (Wayland Book 7)
void xdg_window_ping (void *data, struct xdg_wm_base *xdgWindow, uint32_t serial)
{
  xdg_wm_base_pong(xdgWindow, serial); // Library function.
}

const struct xdg_wm_base_listener xdgWindowListener =
{ .ping = xdg_window_ping };

void xdg_toplevel_close (void *data, struct xdg_toplevel *xdg_toplevel)
{
  way_winType window = (way_winType) data;

  if (window->close_action == CLOSE_BUTTON_CLOSES_PROGRAM)
    exit(0);
  else
  if (window->close_action == CLOSE_BUTTON_RETURNS_KEY)
    expand_key_history(data, K_CLOSE);
}

const struct xdg_toplevel_listener xdgToplevelListener =
{ .configure = xdg_toplevel_configure,
  .close = xdg_toplevel_close
};

void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int width, int height, struct wl_array *states)
{
  //0printf("Configure toplevel: w %d  h %d\n", width, height);
  way_winType window = (way_winType) data;

  // Make sure we aren't minimized.
  if (width > 0 && height > 0)
  { window->pendingWidth = width;
    window->pendingHeight = height;
  } // resizeWindow(primaryWindow, width, height, TRUE);

  if (states)
  { enum xdg_toplevel_state *state = 0;
    wl_array_for_each(state, states)
    { if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED)
      { // is_maximized = true;
        puts("Maximize requested.");

        /*struct xdg_positioner *positioner = xdg_shell_create_positioner(shell);
        xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
        xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_TOP_LEFT);
        xdg_positioner_set_constraint_adjustment(positioner,
            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_HARD);
        xdg_positioner_set_offset(positioner, 0, 0);
        // Use positioner when configuring the toplevel surface
        xdg_toplevel_set_position(toplevel, positioner);*/

        break;
      }
    }
  }
}