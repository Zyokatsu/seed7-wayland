#ifndef INCLUDE_STATE_H
#define INCLUDE_STATE_H
#include "seat.h"
#include <time.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

/*#define KeyModShift 1
#define KeyModControl 2
#define KeyModAlt 4

struct KeyState
{
  uint32_t key;
  bool pressed;
  int unsigned modifiers;
};*/

struct KeyArray
{
  int unsigned size;
  int unsigned use; // Can be a portion of size (to reduce re-allocation).
  uint32_t content[];
};

struct KeyHistory
{
  time_t age;
  struct KeyArray *keys;
};

struct MousePoint
{
  int x;
  int y;
};

struct ClientState
{
  // Globals.
  struct wl_display *display; // Client connection to the server.
  struct wl_output *output;   // Output device (such as a monitor).
  struct wl_registry *registry;
  struct wl_shm *sharedMemory;
  struct wl_compositor *compositor;
  struct wl_subcompositor *subCompositor;
  //struct wl_surface *surface;       // A drawable area (window)
  struct xdg_wm_base *xdgWindow;
  //struct xdg_surface *xdgSurface;   // An application surface.
  //struct xdg_toplevel *xdgTopLevel; // A main window.
  struct zxdg_decoration_manager_v1 *xdgDecorations;

  struct wl_seat *wlSeat;
  struct wl_keyboard *wlKeyboard;
  struct wl_pointer *wlPointer;
  struct PointerEvent pointerEvent;
  uint32_t pointerEnterId;
  bool hidePointer;
  struct wl_surface *pointerSurface;
  struct xkb_state *xkbState;
  struct xkb_context *xkbContext;
  struct xkb_keymap *xkbKeymap;

  struct KeyHistory *keyHistory;
  struct KeyArray *keysPressed;
  struct MousePoint mousePoint;

  // Output (eg monitor).
  int outputWidth;
  int outputHeight;
};

extern struct ClientState waylandState;

void alter_key_state (struct ClientState *state, uint32_t key, bool pressed);
void alter_mouse_button_state (struct ClientState *state, uint32_t button, bool pressed);
void record_mouse_movement (struct ClientState *state, int x, int y);
void trigger_mouse_scroll (struct ClientState *state, bool forward);
#endif