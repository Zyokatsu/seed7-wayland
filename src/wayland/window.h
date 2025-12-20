#ifndef INCLUDE_WINDOW_H
#define INCLUDE_WINDOW_H
#include "common.h"

typedef struct
{ uintType usage_count; // How many sub-windows are using the window (+1 for the window itself)
  boolType resizeReturnsKey;
  int close_action;
  winType parentWindow;
  boolType isPixmap;
  int width;
  int height;
  int pendingWidth;
  int pendingHeight;
  struct BufferData *buffer;
  struct wl_surface *surface;       // A drawable area (window)
  struct xdg_surface *xdgSurface;   // An application surface.
  struct xdg_toplevel *xdgTopLevel; // A main window.
  struct wl_subsurface *subsurface;
  /*/ Change as necessary.
  Window window;
  Pixmap backup;
  Pixmap clip_mask;
  boolType is_pixmap;
  boolType is_managed;
  unsigned int width;
  unsigned int height;
  unsigned int backupWidth;
  unsigned int backupHeight;
  intType clear_col;*/
} way_winRecord, *way_winType;

typedef const way_winRecord *const_way_winType;

void drwSetSize (winType actual_window, intType width, intType height);
void resizeWindow (way_winType window, intType width, intType height, bool triggerKey);
void redrawWindow (way_winType window);
#endif