#ifndef INCLUDE_REGISTRY_H
#define INCLUDE_REGISTRY_H
#include <wayland-client.h>

void wayland_register_global
( void *data,
  struct wl_registry *registry,
  uint32_t name,
  const char *interface,
  uint32_t version
);

void wayland_unregister_global (void *data, struct wl_registry *registry, uint32_t name);

extern const struct wl_registry_listener waylandRegistryListener;
#endif