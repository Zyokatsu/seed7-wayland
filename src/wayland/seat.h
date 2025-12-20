#ifndef INCLUDE_SEAT_H
#define INCLUDE_SEAT_H
#include <wayland-client.h>
#include <stdbool.h>

// Listeners.
extern const struct wl_seat_listener wlSeatListener;
extern const struct wl_pointer_listener wlPointerListener;
extern const struct wl_keyboard_listener wlKeyboardListener;

// Seat.
void wl_seat_capabilities (void *data, struct wl_seat *seat, uint32_t capabilities);
void wl_seat_name (void *data, struct wl_seat *seat, const char *name);

// Pointer.
void wl_pointer_enter (void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t localX, wl_fixed_t localY);
void wl_pointer_leave (void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface);
void wl_pointer_motion (void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t localX, wl_fixed_t localY);
void wl_pointer_button (void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
void wl_pointer_axis (void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source);
void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis);
void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete);
void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer);
void wl_pointer_axis_120(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t value);
void wl_pointer_axis_relative_direction(void *data, struct wl_pointer *wl_pointer, uint32_t axis, uint32_t direction);

enum PointerEventMask
{
  POINTER_EVENT_ENTER = 1 << 0,
  POINTER_EVENT_LEAVE = 1 << 1,
  POINTER_EVENT_MOTION = 1 << 2,
  POINTER_EVENT_BUTTON = 1 << 3,
  POINTER_EVENT_AXIS = 1 << 4,
  POINTER_EVENT_AXIS_SOURCE = 1 << 5,
  POINTER_EVENT_AXIS_STOP = 1 << 6,
  POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
};

struct PointerEvent
{
  uint32_t eventMask;
  wl_fixed_t surfaceX, surfaceY;
  uint32_t button, state;
  uint32_t time;
  uint32_t serial;
  struct
  { bool valid;
    wl_fixed_t value;
    int32_t discrete;
  } axes[2];
  uint32_t axisSource;
};

// Keyboard.
void wl_keyboard_keymap (void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size);
void wl_keyboard_enter (void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
void wl_keyboard_key (void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
void wl_keyboard_leave (void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface);
void wl_keyboard_modifiers (void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay);
#endif