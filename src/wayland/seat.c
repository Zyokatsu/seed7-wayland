#include "seat.h"
#include "shared_memory.h"
#include "state.h"
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

/*- Listeners --------------------------
--------------------------------------*/
const struct wl_seat_listener wlSeatListener =
{ .capabilities = wl_seat_capabilities,
  .name = wl_seat_name
};

const struct wl_pointer_listener wlPointerListener =
{ .enter = wl_pointer_enter,
  .leave = wl_pointer_leave,
  .motion = wl_pointer_motion,
  .button = wl_pointer_button,
  .axis = wl_pointer_axis,
  .frame = wl_pointer_frame,
  .axis_source = wl_pointer_axis_source,
  .axis_stop = wl_pointer_axis_stop,
  .axis_discrete = wl_pointer_axis_discrete,
  .axis_value120 = wl_pointer_axis_120,
  .axis_relative_direction = wl_pointer_axis_relative_direction
};

const struct wl_keyboard_listener wlKeyboardListener =
{ .keymap = wl_keyboard_keymap,
  .enter = wl_keyboard_enter,
  .leave = wl_keyboard_leave,
  .key = wl_keyboard_key,
  .modifiers = wl_keyboard_modifiers,
  .repeat_info = wl_keyboard_repeat_info,
};

/*- Seats ------------------------------
--------------------------------------*/
void wl_seat_capabilities (void *data, struct wl_seat *seat, uint32_t capabilities)
{
  struct ClientState *state = data;
  bool hasPointer = capabilities & WL_SEAT_CAPABILITY_POINTER;

  // Pointer present? Add listener.
  if (hasPointer && state->wlPointer == NULL)
  { state->wlPointer = 0;
    state->wlPointer = wl_seat_get_pointer(state->wlSeat);
    wl_pointer_add_listener(state->wlPointer, &wlPointerListener, state);
  }
  // Pointer removed? Tell Wayland it can release the resources.
  else if (!hasPointer && state->wlPointer != NULL)
  { wl_pointer_release(state->wlPointer);
    state->wlPointer = NULL;
  }

  bool hasKeyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

  if (hasKeyboard && state->wlKeyboard == NULL)
  { state->wlKeyboard = wl_seat_get_keyboard(state->wlSeat);
    wl_keyboard_add_listener(state->wlKeyboard, &wlKeyboardListener, state);
  }
  else
  if (!hasKeyboard && state->wlKeyboard != NULL)
  { wl_keyboard_release(state->wlKeyboard);
    state->wlKeyboard = NULL;
  }
}

void wl_seat_name (void *data, struct wl_seat *seat, const char *name)
{
  //printf("Seat: %s\n", name);
}

/*- Pointer Events ---------------------
--------------------------------------*/
void wl_pointer_enter
( void *data, struct wl_pointer *wl_pointer,
  uint32_t serial, struct wl_surface *surface,
  wl_fixed_t localX, wl_fixed_t localY
)
{
  struct ClientState *state = data;
  state->pointerEnterId = serial;
  state->pointerEvent.eventMask |= POINTER_EVENT_ENTER;
  state->pointerEvent.serial = serial;
  state->pointerEvent.surfaceX = localX,
  state->pointerEvent.surfaceY = localY;
  reset_pressed_mouse_buttons(state);

  if (state->hidePointer)
    wl_pointer_set_cursor(wl_pointer, serial, 0, 0, 0);
}

void wl_pointer_leave
( void *data, struct wl_pointer *wl_pointer,
  uint32_t serial, struct wl_surface *surface
)
{
  struct ClientState *state = data;
  state->pointerEvent.serial = serial;
  state->pointerEvent.eventMask |= POINTER_EVENT_LEAVE;
}

void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t localX, wl_fixed_t localY)
{
  struct ClientState *state = data;
  state->pointerEvent.eventMask |= POINTER_EVENT_MOTION;
  state->pointerEvent.time = time;
  state->pointerEvent.surfaceX = localX,
  state->pointerEvent.surfaceY = localY;

  record_mouse_movement(state, wl_fixed_to_double(localX), wl_fixed_to_double(localY));
}

void wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
  struct ClientState *clientState = data;
  clientState->pointerEvent.eventMask |= POINTER_EVENT_BUTTON;
  clientState->pointerEvent.time = time;
  clientState->pointerEvent.serial = serial;
  clientState->pointerEvent.button = button,
  clientState->pointerEvent.state = state;
}

void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
  struct ClientState *clientState = data;
  clientState->pointerEvent.eventMask |= POINTER_EVENT_AXIS;
  clientState->pointerEvent.time = time;
  clientState->pointerEvent.axes[axis].valid = true; // Axis has changed.
  clientState->pointerEvent.axes[axis].value = value;
}

void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source)
{
  struct ClientState *clientState = data;
  clientState->pointerEvent.eventMask |= POINTER_EVENT_AXIS_SOURCE;
  clientState->pointerEvent.axisSource = axis_source;
}

void
wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis)
{
  struct ClientState *clientState = data;
  clientState->pointerEvent.time = time;
  clientState->pointerEvent.eventMask |= POINTER_EVENT_AXIS_STOP;
  clientState->pointerEvent.axes[axis].valid = true; // Axis has changed.
}

void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete)
{
  struct ClientState *ClientState = data;
  ClientState->pointerEvent.eventMask |= POINTER_EVENT_AXIS_DISCRETE;
  ClientState->pointerEvent.axes[axis].valid = true; // Axis has changed.
  ClientState->pointerEvent.axes[axis].discrete = discrete;
}

void wl_pointer_axis_120(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t value)
{}
void wl_pointer_axis_relative_direction(void *data, struct wl_pointer *wl_pointer, uint32_t axis, uint32_t direction)
{}

void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer)
{
  struct ClientState *clientState = data;
  struct PointerEvent *event = &clientState->pointerEvent;
  /* printf("pointer frame @ %d: ", event->time);

  if (event->eventMask & POINTER_EVENT_ENTER) {
          printf("entered %f, %f ",
                          wl_fixed_to_double(event->surfaceX),
                          wl_fixed_to_double(event->surfaceY));
  }

  if (event->eventMask & POINTER_EVENT_LEAVE) {
          printf("leave");
  }

  if (event->eventMask & POINTER_EVENT_MOTION) {
          printf("motion %f, %f ",
                          wl_fixed_to_double(event->surfaceX),
                          wl_fixed_to_double(event->surfaceY));
  }*/

  if (event->eventMask & POINTER_EVENT_BUTTON)
  { /*char *state = event->state == WL_POINTER_BUTTON_STATE_RELEASED ? "released" : "pressed";
    printf("button %d %s ", event->button, state);*/
    alter_mouse_button_state(clientState, event->button, event->state != WL_POINTER_BUTTON_STATE_RELEASED, true);
  }

  uint32_t axis_events = POINTER_EVENT_AXIS
          | POINTER_EVENT_AXIS_SOURCE
          | POINTER_EVENT_AXIS_STOP
          | POINTER_EVENT_AXIS_DISCRETE;
  char *axis_name[2] = {
          [WL_POINTER_AXIS_VERTICAL_SCROLL] = "vertical",
          [WL_POINTER_AXIS_HORIZONTAL_SCROLL] = "horizontal",
  };
  char *axisSource[4] = {
          [WL_POINTER_AXIS_SOURCE_WHEEL] = "wheel",
          [WL_POINTER_AXIS_SOURCE_FINGER] = "finger",
          [WL_POINTER_AXIS_SOURCE_CONTINUOUS] = "continuous",
          [WL_POINTER_AXIS_SOURCE_WHEEL_TILT] = "wheel tilt",
  };
  if (event->eventMask & axis_events)
  { for (size_t i = 0; i < 2; ++i)
    { if (!event->axes[i].valid)
        continue;

      printf("%s axis ", axis_name[i]);
      if (event->eventMask & POINTER_EVENT_AXIS)
      { // Scroll wheel (vertical movment).
        printf("value %f ", wl_fixed_to_double(event->axes[i].value));

        if (event->axes[i].value)
          trigger_mouse_scroll(clientState, event->axes[i].value < 0);
      }
      if (event->eventMask & POINTER_EVENT_AXIS_DISCRETE) {
              printf("discrete %d ",
                              event->axes[i].discrete);
      }
      if (event->eventMask & POINTER_EVENT_AXIS_SOURCE) {
              printf("via %s ",
                              axisSource[event->axisSource]);
      }
      if (event->eventMask & POINTER_EVENT_AXIS_STOP) {
              printf("(stopped) ");
      }
    }
    printf("\n");
  }

  memset(event, 0, sizeof(*event)); // Wipes the event?
}

/*- Keyboard Events --------------------
--------------------------------------*/
void wl_keyboard_keymap (void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size)
{
  struct ClientState *clientState = data;
  assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

  // Seems to crash when passing MAP_SHARED. Does it matter which is used?
  char *map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  assert(map_shm != MAP_FAILED);

  struct xkb_keymap *keymap = xkb_keymap_new_from_string
  ( clientState->xkbContext,
    map_shm,
    XKB_KEYMAP_FORMAT_TEXT_V1,
    XKB_KEYMAP_COMPILE_NO_FLAGS
  );
  munmap(map_shm, size);
  close(fd);

  struct xkb_state *xkbState = xkb_state_new(keymap);
  xkb_keymap_unref(clientState->xkbKeymap);
  xkb_state_unref(clientState->xkbState);
  clientState->xkbKeymap = keymap;
  clientState->xkbState = xkbState;
}

// Called when the surface gains keyboard focus.
void wl_keyboard_enter
( void *data,
  struct wl_keyboard *wl_keyboard,
  uint32_t serial,
  struct wl_surface *surface,
  struct wl_array *keys
)
{
  struct ClientState *clientState = data;
  // fprintf(stderr, "keyboard enter; keys pressed are:\n");
  uint32_t *key;
  reset_pressed_keys(clientState);

  // evdev scancodes need +8 to convert to XKB scancode (hence "*key + 8" below).
  wl_array_for_each(key, keys)
  { // char buf[128];
    xkb_keysym_t sym = xkb_state_key_get_one_sym(clientState->xkbState, *key + 8);
    // xkb_keysym_get_name(sym, buf, sizeof(buf));
    // fprintf(stderr, "sym: %-12s (%d), ", buf, sym);
    // xkb_state_key_get_utf8(clientState->xkbState, *key + 8, buf, sizeof(buf));
    // fprintf(stderr, "utf8: '%s'\n", buf);
    alter_key_state(clientState, sym, true, false);
  }
}

void wl_keyboard_key (void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
  struct ClientState *clientState = data;
  // char buf[128];
  uint32_t keycode = key + 8;
  xkb_keysym_t sym = xkb_state_key_get_one_sym(clientState->xkbState, keycode);
  // xkb_keysym_get_name(sym, buf, sizeof(buf));
  // const char *action = state == WL_KEYBOARD_KEY_STATE_PRESSED ? "press" : "release";
  // fprintf(stderr, "key %s: sym: %-12s (%d), ", action, buf, sym);
  // xkb_state_key_get_utf8(clientState->xkbState, keycode, buf, sizeof(buf));
  // fprintf(stderr, "utf8: '%s'\n", buf);

  // Function located in gkb_way.c
  alter_key_state(clientState, sym, state == WL_KEYBOARD_KEY_STATE_PRESSED, true);
}

// Called when the surface loses keyboard focus.
void wl_keyboard_leave (void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface)
{
  // fprintf(stderr, "keyboard leave\n");
}

void wl_keyboard_modifiers
( void *data, struct wl_keyboard *wl_keyboard,
  uint32_t serial, uint32_t mods_depressed,
  uint32_t mods_latched, uint32_t mods_locked,
  uint32_t group
)
{
  struct ClientState *clientState = data;
  xkb_state_update_mask(clientState->xkbState, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay)
{
  /* Left as an exercise for the reader */
}