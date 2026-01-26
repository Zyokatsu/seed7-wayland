/********************************************************************/
/*                                                                  */
/*  drw_way.c     Graphic access using Wayland capabilities.        */
/*  Copyright (C) 2026  Thomas Mertes                               */
/*                                                                  */
/*  This file is part of the Seed7 Runtime Library.                 */
/*                                                                  */
/*  The Seed7 Runtime Library is free software; you can             */
/*  redistribute it and/or modify it under the terms of the GNU     */
/*  Lesser General Public License as published by the Free Software */
/*  Foundation; either version 2.1 of the License, or (at your      */
/*  option) any later version.                                      */
/*                                                                  */
/*  The Seed7 Runtime Library is distributed in the hope that it    */
/*  will be useful, but WITHOUT ANY WARRANTY; without even the      */
/*  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR */
/*  PURPOSE.  See the GNU Lesser General Public License for more    */
/*  details.                                                        */
/*                                                                  */
/*  You should have received a copy of the GNU Lesser General       */
/*  Public License along with this program; if not, write to the    */
/*  Free Software Foundation, Inc., 51 Franklin Street,             */
/*  Fifth Floor, Boston, MA  02110-1301, USA.                       */
/*                                                                  */
/*  Module: Seed7 Runtime Library                                   */
/*  File: seed7/src/wayland/drw_way.c                               */
/*  Content: Graphic access using Wayland capabilities.             */
/*                                                                  */
/********************************************************************/
#define LOG_FUNCTIONS 0
#define VERBOSE_EXCEPTIONS 0
#include "buffer.h"
#include "registry.h"
#include "state.h"
#include "window.h"
#include "xdg.h"
#include "xdg_decorations_unstable.h"
#include <wayland-cursor.h>

#include "../data_rtl.h"
#include "../heaputl.h"
#include "../striutl.h"
#include "keyboard_globals.h"
#include "../rtl_err.h"

#include "../drw_drv.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*#include "stdio.h"
#include "limits.h"*/

void gkbInitKeyboard (void);

/*- Globals ----------------------------
--------------------------------------*/
#define PI  3.141592653589793238462643383279502884197
#define PI2 6.283185307179586476925286766559005768394
// Uncomment the following directive to enable alpha support (in colours).
// #define USE_ALPHA 1
static winType globalEmptyWindow = 0;
bool init_called = FALSE;
struct ClientState waylandState = { 0 }; // Initial state (all nulls/zeros).
//way_winType primaryWindow = 0;

/*------------------------------------------------------------------------------
                            Required Seed7 Functions
------------------------------------------------------------------------------*/
winType generateEmptyWindow (void); // Defined later.

void drawInit (void)
{
  /* Connect to the default Wayland display's Unix socket.
  As per the documentation, this represents a client's (our) connection to
  the Wayland server. */
  waylandState.display = wl_display_connect(NULL); // Library function.

  // Hook in to the registry to access the compositor's global objects.
  waylandState.registry = wl_display_get_registry(waylandState.display); // Library function.

  // Grab the keyboard handle.
  waylandState.xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  // Listen for global events.
  /* This will start by firing a bunch of object-publication-type events,
  allowing us to hook into the output, compositors, etc. */
  wl_registry_add_listener(waylandState.registry, &waylandRegistryListener, &waylandState); // Library function.

  /* Signal the server to process all current requests, producing those initial
  global-object events, thus building the event queue. */
  wl_display_roundtrip(waylandState.display); // Returns the number of dispatched events.

  /* Now, tell the server to process incoming events (through the just-registered
  globals/event queues). This function will block until there are events, but that
  is fine, since there are initial events, such as the output's mode event,
  informing us of the output's dimensions (i.e. monitor resulotion).
    We will be calling this again in other functions, to process rendering events. */
  wl_display_dispatch(waylandState.display); // Returns the number of dispatched events.

  globalEmptyWindow = generateEmptyWindow(); // Needed?

  gkbInitKeyboard();
  init_called = TRUE;
}

/**
 *  Determine the height of the screen in pixels.
 */
intType drwScreenHeight (void)
{
  logFunction(printf("drwScreenHeight()\n"););
  if (!init_called)
    drawInit();
  logFunction(printf("drwScreenHeight() --> %u\n", height););
  return (intType) waylandState.outputHeight;
}

/**
 *  Determine the width of the screen in pixels.
 */
intType drwScreenWidth (void)
{
  logFunction(printf("drwScreenWidth()\n"););
  if (!init_called)
    drawInit();
  logFunction(printf("drwScreenWidth() --> %u\n", width););
  return (intType) waylandState.outputWidth;
}

// Gets called when opening the main window.
winType drwOpen (intType xPos, intType yPos, intType width, intType height, const const_striType windowName)
{
  way_winType newWindow = NULL;
  errInfoType err_info = OKAY_NO_ERROR;
  os_striType newName = stri_to_os_stri(windowName, &err_info);

  if
  ( unlikely
    ( !inIntRange(xPos) || !inIntRange(yPos) ||
      width < 1 || width > INT_MAX ||
      height < 1 || height > INT_MAX
    )
  )
  { logError(printf("drwOpen(" FMT_D ", " FMT_D ", " FMT_D ", " FMT_D
                    ", \"%s\"): Illegal window dimensions\n",
                    xPos, yPos, width, height,
                    striAsUnquotedCStri(windowName)););
    raise_error(RANGE_ERROR);
  }
  else
  { if (!init_called)
      drawInit();

    if (ALLOC_RECORD2(newWindow, way_winRecord, count.win, count.win_bytes))
    { memset(newWindow, 0, sizeof(way_winRecord));
      newWindow->usage_count = 1;
      newWindow->isPixmap = FALSE;
      newWindow->width = width;
      newWindow->height = height;
      newWindow->buffer = NULL;
      // primaryWindow = newWindow;
      // printf("  Window initialized: %p\n", newWindow);

      // Create a new surface (i.e. drawable area).
      newWindow->surface = wl_compositor_create_surface(waylandState.compositor);

      // Make the surface an "application" surface.
      newWindow->xdgSurface = xdg_wm_base_get_xdg_surface(waylandState.xdgWindow, newWindow->surface);

      xdg_surface_add_listener(newWindow->xdgSurface, &xdgSurfaceListener, newWindow); // Listen for application events.
      newWindow->xdgTopLevel = xdg_surface_get_toplevel(newWindow->xdgSurface); // Make the surface "toplevel" (a main window).

      xdg_toplevel_add_listener(newWindow->xdgTopLevel, &xdgToplevelListener, newWindow); // Listen for toplevel events (like window close requests).
      zxdg_toplevel_decoration_v1_set_mode(zxdg_decoration_manager_v1_get_toplevel_decoration(waylandState.xdgDecorations, newWindow->xdgTopLevel), ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
      xdg_toplevel_set_title(newWindow->xdgTopLevel, newName); // Set the surface's/window's title.

      // Commit current surface prior to buffer attachment.
      wl_surface_commit(newWindow->surface);
      wl_display_dispatch(waylandState.display);

      drwClear((winType) newWindow, 0xFF000000); // Clear window to black.
    }
  }

  return (winType) newWindow;
}

// If parent window is the empty window, then this should open a top-level, decorationless window.
winType drwOpenSubWindow (winType parent_window, intType xPos, intType yPos, intType width, intType height)
{
  way_winType newWindow = NULL;

  if (ALLOC_RECORD2(newWindow, way_winRecord, count.win, count.win_bytes))
  { memset(newWindow, 0, sizeof(way_winRecord));
    newWindow->usage_count = 1;
    newWindow->isPixmap = FALSE;
    newWindow->width = width;
    newWindow->height = height;
    newWindow->buffer = NULL;
    newWindow->surface = wl_compositor_create_surface(waylandState.compositor);

    if (parent_window && parent_window != globalEmptyWindow)
    { // Increase parent's usage count. Reduce in drwFree.
      way_winType parent = (way_winType) parent_window;
      parent->usage_count++;
      newWindow->parentWindow = parent_window;
      newWindow->subsurface = wl_subcompositor_get_subsurface(waylandState.subCompositor, newWindow->surface, parent->surface);
      wl_subsurface_set_position(newWindow->subsurface, xPos, yPos);
      // By desyncing the subsurface, we allow it to update without needing to commit the whole parent.
      wl_subsurface_set_desync(newWindow->subsurface);
      wl_surface_commit(newWindow->surface);
      // wl_surface_commit(parent->surface);
    }
    else
    { newWindow->xdgSurface = xdg_wm_base_get_xdg_surface(waylandState.xdgWindow, newWindow->surface);
      xdg_surface_add_listener(newWindow->xdgSurface, &xdgSurfaceListener, newWindow); // Listen for application events.
      newWindow->xdgTopLevel = xdg_surface_get_toplevel(newWindow->xdgSurface); // Make the surface "toplevel" (a main window).
      xdg_toplevel_add_listener(newWindow->xdgTopLevel, &xdgToplevelListener, newWindow); // Listen for toplevel events (like window close requests).
      wl_surface_commit(newWindow->surface);
    }

    wl_display_dispatch(waylandState.display);
    drwClear((winType) newWindow, 0xFF000000); // Clear window to black.
  }

  return (winType) newWindow;
}

// Unfinished.
/* Called when a window's reference is no longer used, such as when reassigned,
or at program exit. */
void drwFree (winType old_window)
{
  // printf("Draw free called for: %p\n", old_window);
  if (!((way_winType) old_window)->isPixmap)
  { wl_surface_destroy(((way_winType) old_window)->surface);
    if (((way_winType) old_window)->parentWindow && ((way_winType) old_window)->parentWindow->usage_count > 0)
    { ((way_winType) old_window)->parentWindow->usage_count--;
      if (((way_winType) old_window)->parentWindow->usage_count == 0)
        drwFree(((way_winType) old_window)->parentWindow);
    }
    wl_display_flush(waylandState.display);
  }
  FREE_RECORD2(old_window, way_winRecord, count.win, count.win_bytes);
}

// Should this be called implicitly before our calls to wl_display_dispatch?
void drwFlush (void)
{
  if (!init_called)
    drawInit();

  // Flush outgoing requests to the Wayland server
  wl_display_flush(waylandState.display); // Returns -1 if it wasn't able to send all data.
}

/*- Utilities --------------------------
--------------------------------------*/
/* Ensures the area from source to destination lies within the windows'
bounds (or else width or height becomes zero). */
void clamp_common_area
( way_winType source,
  way_winType destination,
  intType *src_x,
  intType *src_y,
  intType *width,
  intType *height,
  intType *dest_x,
  intType *dest_y
)
{
  if
  ( *width > 0 && *height > 0 &&
    *src_x < source->width && *src_x + *width >= 0 && *src_y < source->height && *src_y + *height >= 0 &&
    *dest_x < destination->width && *dest_y < destination->height
  )
  { /* If source-x is negative, we can skip those out-of-bound pixels,
    so push the start-x's forward and reduce the reach accordingly. */
    if (*src_x < 0)
    { *width += *src_x;
      *dest_x -= *src_x;
      *src_x = 0;
    }
    // Do the same with the y.
    if (*src_y < 0)
    { *height += *src_y;
      *dest_y -= *src_y;
      *src_y = 0;
    }
    /* If destination-x is negative, the corresponding source pixels would be copied into nothing,
    so push the start-x's forward and reduce the reach to match. */
    if (*dest_x < 0)
    { *src_x -= *dest_x;
      *width += *dest_x;
      *dest_x = 0;
    }
    // Do the same with the y.
    if (*dest_y < 0)
    { *src_y -= *dest_y;
      *height += *dest_y;
      *dest_y = 0;
    }
    /* If the reach (from the start) would proceed "off-the-edge" of the destination,
    reduce the reach to the edge. */
    if (*width + *dest_x > destination->width)
      *width -= *width+*dest_x - destination->width;
    if (*height + *dest_y > destination->height)
      *height -= *height+*dest_y - destination->height;
    /* Do the same with the source (in this case, treating "off-the-edge" as fully transparent,
    and therefore pointless to copy over). */
    if (*src_x + *width > source->width)
      *width = source->width - *src_x;
    if (*src_y + *height > source->height)
      *height = source->height - *src_y;
  }
}

/*- Raw Drawing Functions --------------
These assume that a buffer has all ready
been prepared; and they do not message
the Wayland server.
--------------------------------------*/
void drawRawPoint (way_winType window, intType x, intType y, intType col)
{
  intType pos = y*window->buffer->width + x;
  if (pos < window->buffer->width * window->buffer->height)
    window->buffer->content[pos] = col;
}

void drawRawLine (way_winType window, intType x1, intType y1, intType x2, intType y2, intType col)
{
  /* Utilizing the slope-intercept formula:
    y = mx + b  (b, being the intercept, is zero for us.)
    m = y/x
  */
  float slope = (x2-x1) ? ((float)y2-(float)y1)/((float)x2-(float)x1) : 0;
  intType xPos = x1, yPos = y1, pos = 0, maxPos = window->buffer->width * window->buffer->height;

  if (slope == 0)
  { if (y1 == y2) // Horizontal line.
      do
      { pos = y1 * window->buffer->width + xPos;
        if (pos < maxPos)
        { window->buffer->content[pos] = col;
          xPos += x1 < x2 ? 1 : -1;
        }
        else
          break;
      } while (x1 <= x2 && xPos <= x2 || x1 > x2 && xPos >= x2);
    else // Vertical line.
      do
      { pos = yPos * window->buffer->width + x1;
        if (pos < maxPos)
        { window->buffer->content[pos] = col;
          yPos += y1 < y2 ? 1 : -1;
        }
        else
          break;
      } while (y1 <= y2 && yPos <= y2 || y1 > y2 && yPos >= y2);
  }
  else // Diagonal, utilizing Breshenham's algorithm.
  { intType dx, dy, adjuster = 1, check;

    if (slope >= -1.0 && slope <= 1.0) // Diagonal low-slope line.
    { if (x1 > x2)
      { dx = x1;
        x1 = x2;
        x2 = dx;
        dy = y1;
        y1 = y2;
        y2 = dy;
      }
      dx = x2-x1;
      dy = y2-y1;
      if (dy < 0)
      { adjuster = -1;
        dy = -dy;
      }
      check = dy*2 - dx;
      for (xPos = x1, yPos = y1; xPos <= x2; xPos++)
      { pos = yPos*window->buffer->width + xPos;
        if (pos < maxPos)
          window->buffer->content[pos] = col;
        if (check > 0)
        { yPos += adjuster;
          check += (dy - dx) * 2;
        }
        else
          check += dy*2;
      }
    }
    else // Diagonal high-slope line.
    { if (y1 > y2)
      { dx = x1;
        x1 = x2;
        x2 = dx;
        dy = y1;
        y1 = y2;
        y2 = dy;
      }
      dx = x2-x1;
      dy = y2-y1;
      if (dx < 0)
      { adjuster = -1;
        dx = -dx;
      }
      check = dx*2 - dy;
      for (xPos = x1, yPos = y1; yPos <= y2; yPos++)
      { pos = yPos*window->buffer->width + xPos;
        if (pos < maxPos)
          window->buffer->content[pos] = col;
        if (check > 0)
        { xPos += adjuster;
          check += (dx - dy) * 2;
        }
        else
          check += dx*2;
      }
    }
  }
  /*else // Diagonal line (custom algorithm)
  { // printf("x1: %ld, x2: %ld, y1: %ld, y2: %ld, slope: %f\n", x1, x2, y1, y2, slope);

    intType z = 0, limit = 0; // Progress and limit.
    do
    { if (slope >= 1 || slope <= -1)
      { yPos = y1 < y2 ? y1 + z : y1 - z;
        xPos = x1 + (yPos-y1)/slope; // x = y/m
      }
      else
      { xPos = x1 < x2 ? x1 + z : x1 - z;
        yPos = y1 + slope*(xPos-x1); // y = mx
      }

      pos = yPos*window->buffer->width + xPos;

      if (pos < maxPos)
      { window->buffer->content[pos] = col;
        z++;

        if (z == 1) // Set the limit on the first loop (being the length of the longest dimension).
          limit = slope >= 1 || slope <= -1 ? (y1 <= y2 ? y2-y1 : y1-y2) : (x1 < x2 ? x2-x1 : x1-x2);
      }
      else
        break;
    } while (z <= limit);
  }*/
}

// Could likey be optimized. Also, draws slightly different from x11's.
// Assumes the window's buffer has all ready been prepared.
void drawRawArc
( way_winType window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle,
  intType col
)
{
  /* Angles are given as radians. Start angle of 0 is right, sweeps counter-clockwise.
  As we are dealing with radians, 2 * PI is a full turn (or 360 degrees).
  Jesko's method works with 1/8 turns. */
  const float Turn8 = PI2 / 8.0;
  boolType crossingBound = false;
  startAngle = fmod(startAngle, PI2);
  if (startAngle < 0)
    startAngle += PI2;
  floatType endAngle = startAngle + sweepAngle;
  if (endAngle < 0)
  { crossingBound = true;
    endAngle += PI2;
  }
  else
  if (endAngle > PI2)
  { crossingBound = true;
    endAngle = fmod(endAngle, PI2);
  }

  // Ensure the start angle is earlier in the arc.
  if (startAngle > endAngle)
  { floatType tempAngle = endAngle;
    endAngle = startAngle;
    startAngle = tempAngle;
  }

  intType t1 = radius / 16, // This division makes for a smoother edge.
    t2 = 0,
    xd = radius,
    yd = 0;
  intType pos = 0, maxPos = window->buffer->width * window->buffer->height;
  intType xPos = 0, yPos = 0;
  float angle = 0.0;
  // Account for rounding errors in the dynamic angles within the loop.
  startAngle -= 0.000001;
  endAngle += 0.000001;

  while (xd >= yd)
  { // Using the coordinate, render the eight mirrored points when appropriate.
    // Render right, downward A (plunges from the middle)
    xPos = x + xd; yPos = y + yd; pos = yPos*window->buffer->width + xPos; angle = PI2 - ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;
    // Render right, upward A (rises from the middle)
    yPos = y - yd; pos = yPos*window->buffer->width + xPos; angle = ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;
    // Render left, upward A
    xPos = x - xd; pos = yPos*window->buffer->width + xPos; angle = PI - ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;
    // Render left, downward A (these first four draw the "parenthesis" portion: ( )
    yPos = y + yd; pos = yPos*window->buffer->width + xPos; angle = PI + ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;
    // Render right, downard B (touches the bottom)
    xPos = x + yd; yPos = y + xd; pos = yPos*window->buffer->width + xPos; angle = PI2*0.75 + ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;
    // Render right, upward B (touches the top)
    yPos = y - xd; pos = yPos*window->buffer->width + xPos; angle = PI*0.5 - ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;
    // Render left, upward B
    xPos = x - yd; pos = yPos*window->buffer->width + xPos; angle = PI*0.5 + ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;
    // Render left, downward B
    yPos = y + xd; pos = yPos*window->buffer->width + xPos; angle = PI2*0.75 - ((float)yd / (float)xd) * Turn8;
      if (pos < maxPos && (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))) window->buffer->content[pos] = col;

    // Adjust position.
    yd += 1;
    t1 += yd;
    t2 = t1 - xd;

    if (t2 >= 0)
    { t1 = t2;
      xd -= 1;
    }
  }

  /* Option to step into an arc (unfinished).
  Using these two formulas:
    x^2 + y^2 = r^2
    y = mx,  x = y/m,  m = y/x

  Solve for x.
    x^2 + y^2 = r^2
    x^2 + (mx)^2 = r^2
    x^2 + m^2*x^2 = r^2
    (m^2+1)*x^2 = r^2
    x^2 = r^2/(m^2+1)
    x = sqrt(r^2/(m^2+1))

  const float progress = startAngle == 0 ? 0.0 : (startAngle == Turn8 ? 1.0 : fmod(startAngle, Turn8));
  const float initialSlope = progress / Turn8;
  float initialX = !initialSlope ? radius : sqrt((radius*radius) / (initialSlope*initialSlope + 1.0)),
    initialY = initialSlope * initialX;

  intType pos = (y+yd)*window->buffer->width + x+xd,
       maxPos = window->buffer->width * window->buffer->height;

  while (angle <= sweepAngle)
  { // Counter clockwise from the right: .--  ./  |  \.  --. etc.
    if (pos < maxPos)
      window->buffer->content[pos] = col;
    //pos = (int)(y-xd)*window->buffer->width + x-yd;
    //  window->buffer->content[pos] = col;
    //pos = (int)(y-xd)*window->buffer->width + x+yd;
    //  window->buffer->content[pos] = col;
    //pos = (int)(y+yd)*window->buffer->width + x-xd;
    //  window->buffer->content[pos] = col;
    //pos = (int)(y-yd)*window->buffer->width + x-xd;
    //  window->buffer->content[pos] = col;
    //pos = (int)(y+xd)*window->buffer->width + x+yd;
    //  window->buffer->content[pos] = col;
    //pos = (int)(y+xd)*window->buffer->width + x-yd;
    //  window->buffer->content[pos] = col;
    //pos = (int)(y-yd)*window->buffer->width + x+xd;
    //  window->buffer->content[pos] = col;
    if (angle >= Turn8)
      break;

    if (slope < 1.0)
    { yd -= 1.0;
      // x^2 = r^2 - y^2  Therefore  x = sqrt(r^2 - y^2)
      xd = sqrt(radius*radius - yd*yd);
    }
    else
    { xd -= 1.0;
      // y^2 = r^2 - x^2  Therefore  y = sqrt(r^2 - x^2)
      yd = sqrt(radius*radius - xd*xd);
    }
    slope = yd && xd ? -yd/xd : 0;
    pos = (y+yd)*window->buffer->width + x+xd;
    // slope = progress / turn8  Therefore  progress = slope * turn8
    angle = slope * Turn8;
    printf("slope: %f  xd: %f  yd: %f\n", slope, xd, yd);
  }*/
}

/*- Secondary Drawing Functions --------
--------------------------------------*/
/* Needed?
boolType gkbButtonPressed (charType button);
intType gkbClickedXpos (void);
intType gkbClickedYpos (void);
charType gkbGetc (void);
boolType gkbInputReady (void);
charType gkbRawGetc (void);
void gkbSelectInput (winType aWindow, charType aKey, boolType active);
winType gkbWindow (void);
*/

// Unfinished.
rtlArrayType drwBorder (const_winType actual_window)
{
  rtlArrayType border = NULL;
  return border;
}

// Unfinished.
winType drwCapture (intType left, intType upper, intType width, intType height)
{
  puts("Draw capture.");
  way_winType pixmap = NULL;
  return (winType) pixmap;
}

void drwClear (winType actual_window, intType col)
{
  if (!init_called)
    drawInit();

  way_winType window = (way_winType) actual_window;
  // printf("Clear color: %ld\n", col);

  if (prepare_buffer_data(&waylandState, window))
  { // Could use memset instead?
    for (int pos = 0; pos < window->buffer->width * window->buffer->height; pos++)
      window->buffer->content[pos] = col;

    if (!window->isPixmap)
    { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
      wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
      wl_surface_damage_buffer(window->surface, 0, 0, window->buffer->width, window->buffer->height); // INT32_MAX, INT32_MAX);
      wl_surface_commit(window->surface);
      wl_display_flush(waylandState.display);
      wl_display_dispatch_pending(waylandState.display);
    }
  }
}

// Copied from drw_emc.c
rtlArrayType drwConvPointList (const const_bstriType pointList)
{
  memSizeType numCoords;
  int *coords;
  memSizeType pos;
  rtlArrayType xyArray;

  /* drwConvPointList */
  logFunction(printf("drwConvPointList(\"%s\")\n",
                     bstriAsUnquotedCStri(pointList)););
  numCoords = pointList->size / sizeof(int);
  if (unlikely(!ALLOC_RTL_ARRAY(xyArray, numCoords))) {
    raise_error(MEMORY_ERROR);
  } else {
    xyArray->min_position = 1;
    xyArray->max_position = (intType) numCoords;
    coords = (int *) pointList->mem;
    for (pos = 0; pos < numCoords; pos ++) {
      xyArray->arr[pos].value.intValue = (intType) coords[pos];
    } /* for */
  } /* if */
  logFunction(printf("drwConvPointList --> arr (size=" FMT_U_MEM ")\n",
                     arraySize(xyArray)););
  return xyArray;
} /* drwConvPointList */

/**
 *  Copy a rectangular area from 'src_window' to 'dest_window'.
 *  Coordinates are measured relative to the top left corner of the
 *  corresponding window drawing area (inside of the window decorations).
 *  @param src_window Source window.
 *  @param dest_window Destination window.
 *  @param src_x X-position of the top left corner of the source area.
 *  @param src_y Y-position of the top left corner of the source area.
 *  @param width Width of the rectangular area.
 *  @param height Height of the rectangular area.
 *  @param dest_x X-position of the top left corner of the destination area.
 *  @param dest_y Y-position of the top left corner of the destination area.
 */
void drwCopyArea
( const_winType src_window,
  const_winType dest_window,
  intType src_x,
  intType src_y,
  intType width,
  intType height,
  intType dest_x,
  intType dest_y
)
{
  way_winType source, destination;
  intType dx, dy, sPos, dPos;

  logFunction(printf("drwCopyArea(" FMT_U_MEM ", " FMT_U_MEM ", "
                     FMT_D ", " FMT_D ", " FMT_D ", " FMT_D ", " FMT_D
                     ", " FMT_D ")\n",
                     (memSizeType) src_window, (memSizeType) dest_window,
                     src_x, src_y, width, height, dest_x, dest_y););
  if (unlikely(!inIntRange(src_x) || !inIntRange(src_y) ||
               width < 1 || width > UINT_MAX ||
               height < 1 || height > UINT_MAX ||
               !inIntRange(dest_x) || !inIntRange(dest_y)))
  { logError(printf("drwCopyArea(" FMT_U_MEM ", " FMT_U_MEM ", "
                    FMT_D ", " FMT_D ", " FMT_D ", " FMT_D ", " FMT_D
                    ", " FMT_D "): Raises RANGE_ERROR\n",
                    (memSizeType) src_window, (memSizeType) dest_window,
                    src_x, src_y, width, height, dest_x, dest_y););
    raise_error(RANGE_ERROR);
  }
  else
  { source = (way_winType) src_window;
    destination = (way_winType) dest_window;
    clamp_common_area(source, destination, &src_x, &src_y, &width, &height, &dest_x, &dest_y);
    // If the resulting coordinates actually lie within both planes.
    if (width > 0 && height > 0 && prepare_buffer_copy(&waylandState, destination))
    { // Copy the pixels from source to destination.
      for (dy = 0; dy < height; dy++)
      { sPos = (src_y+dy)*source->width + src_x;
        dPos = (dest_y+dy)*destination->width + dest_x;
        for (dx = 0; dx < width; dx++)
          destination->buffer->content[dPos+dx] = source->buffer->content[sPos+dx];
      }
      // When dealing with a wayland object, notify its surface of the change.
      if (!destination->isPixmap)
      { wl_buffer_add_listener(destination->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(destination->surface, destination->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(destination->surface, dest_x, dest_y, width, height); // INT32_MAX, INT32_MAX);
        wl_surface_commit(destination->surface);
        // wl_display_flush(waylandState.display);
        // wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

winType drwEmpty (void)
{
  logFunction(printf("drwEmpty()\n"););
  if (!init_called)
      drawInit();
  logFunction(printf("drwEmpty --> " FMT_U_MEM " (usage=" FMT_U ")\n",
                    (memSizeType) emptyWindow,
                     emptyWindow != NULL ? emptyWindow->usage_count : (uintType) 0););
  return globalEmptyWindow;
}

void drwFPolyLine (const_winType actual_window, intType x, intType y, bstriType point_list, intType col)
{
  int *coords;
  memSizeType numCoords, point;
  intType left, right, top, bottom,
    y0, x1,y1, x2,y2, y3;
  intType xPos, yPos, pos, maxPos;
  float slope;
  bool countCorner, contained;

  logFunction(printf("drwFPolyLine(" FMT_U_MEM ", " FMT_D ", " FMT_D
                     ", " FMT_U_MEM ", " F_X(08) ")\n",
                     (memSizeType) actual_window, x, y,
                     (memSizeType) point_list, col););
  if (unlikely(!inShortRange(x) || !inShortRange(y)))
  { logError(printf("drwFPolyLine(" FMT_U_MEM ", " FMT_D ", " FMT_D
                    ", " FMT_U_MEM ", " F_X(08) "): "
                    "Raises RANGE_ERROR\n",
                    (memSizeType) actual_window, x, y,
                    (memSizeType) point_list, col););
    raise_error(RANGE_ERROR);
  }
  else
  { coords = (int *) point_list->mem;
    numCoords = point_list->size / sizeof(int);
    float slopes[numCoords/2];

    if (numCoords >= 4)
    { way_winType window = (way_winType) actual_window;
      if (prepare_buffer_copy(&waylandState, window))
      { // Find the bounds and the slopes, and draw the outline.
        left = x+coords[0]; right = left;
        top = y+coords[1]; bottom = top;
        slopes[0] = (float)(coords[numCoords-1]-coords[1]) / (float)(coords[numCoords-2]-coords[0]);
        for (point = 2; point+1 < numCoords; point+=2)
        { x1 = x+coords[point-2];
          y1 = y+coords[point-1];
          x2 = x+coords[point];
          y2 = y+coords[point+1];
          drawRawLine(window, x1, y1, x2, y2, col);
          if (x1 < left) left = x1;
          else if (x1 > right) right = x1;
          if (x2 < left) left = x2;
          else if (x2 > right) right = x2;
          if (y1 < top) top = y1;
          else if (y1 > bottom) bottom = y1;
          if (y2 < top) top = y2;
          else if (y2 > bottom) bottom = y2;
          slopes[point/2] = (float)(y1-y2) / (float)(x1-x2);
        }

        // Fill the polygon utilizing the even-odd rule.
        if (numCoords >= 6) // No need for more work if only a line is being drawn.
        { maxPos = window->buffer->width * window->buffer->height;

          for (yPos = top; yPos < bottom; yPos++)
            for (xPos = left, pos = yPos * window->buffer->width + xPos; xPos < right; xPos++, pos++)
              if (pos < maxPos)
              { // Check to see if the point is within the polygon.
                contained = false;
                for (point = 0, countCorner = false; point+1 < numCoords; point+=2, countCorner = !countCorner)
                { // For the first point, join from the last point.
                  if (point == 0)
                  { x1 = x+coords[numCoords-2];
                    y1 = y+coords[numCoords-1];
                    x2 = x+coords[point];
                    y2 = y+coords[point+1];
                  }
                  else
                  { x1 = x+coords[point-2];
                    y1 = y+coords[point-1];
                    x2 = x+coords[point];
                    y2 = y+coords[point+1];
                  }
                  slope = slopes[point/2];
                  if // Check for intersection.
                  ( (y1 <= yPos && y2 >= yPos || y2 <= yPos && y1 >= yPos) &&
                    ( !slope && (x1 >= xPos || x2 >= xPos) ||
                      // y = mx   x = y/m
                      slope && xPos-x1 <= (float)(yPos-y1) / slope
                    ) &&
                    (countCorner || yPos != y1 && yPos != y2)
                  )
                  { // Get the adjacent y's to the both y1 and y2.
                    if (point == 0)
                    { y0 = y+coords[numCoords-3];
                      y3 = y+coords[point+3];
                    }
                    else
                    { if (point >= 4)
                        y0 = y+coords[point-3];
                      else
                        y0 = y+coords[numCoords-(3-point)];
                      if (point+3 < numCoords)
                        y3 = y+coords[point+3];
                      else
                        y3 = y+coords[1];
                    }
                    /* Only count when not a corner point, or when both segments connected to the corner
                    lie on the same side (y) of the ray. */
                    if
                    ( (yPos != y1 || (y2 < y1) != (y0 < y1)) &&
                      (yPos != y2 || (y1 < y2) != (y3 < y2))
                    )
                      contained = !contained;
                  }
                }
                // If the ray has intersected an odd number of segments in the polygon.
                if (contained)
                  window->buffer->content[pos] = col;
              }
        }

        if (!window->isPixmap)
        { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
          wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
          wl_surface_damage_buffer(window->surface, left, top, right-left, bottom-top); // INT32_MAX, INT32_MAX);
          wl_surface_commit(window->surface);
          // wl_display_flush(waylandState.display);
          // wl_display_dispatch_pending(waylandState.display);
        }
      }
    }
  }
}

// Copied from drw_emc.c
bstriType drwGenPointList (const const_rtlArrayType xyArray)
{
  memSizeType num_elements;
  memSizeType len;
  int *coords;
  memSizeType pos;
  bstriType result;

  /* drwGenPointList */
  logFunction(printf("drwGenPointList(" FMT_D " .. " FMT_D ")\n",
                     xyArray->min_position, xyArray->max_position););
  num_elements = arraySize(xyArray);
  if (unlikely(num_elements & 1)) {
    raise_error(RANGE_ERROR);
    result = NULL;
  } else {
    len = num_elements >> 1;
    if (unlikely(len > MAX_BSTRI_LEN / (sizeof(int) << 1) || len > MAX_MEM_INDEX)) {
      raise_error(MEMORY_ERROR);
      result = NULL;
    } else {
      if (unlikely(!ALLOC_BSTRI_SIZE_OK(result, len * (sizeof(int) << 1)))) {
        raise_error(MEMORY_ERROR);
      } else {
        result->size = len * (sizeof(int) << 1);
        if (len > 0) {
          coords = (int *) result->mem;
          for (pos = 0; pos < len << 1; pos++) {
            coords[pos] = castToLong(xyArray->arr[pos].value.intValue);
          } /* for */
        } /* if */
      } /* if */
    } /* if */
  } /* if */
  return result;
} /* drwGenPointList */

intType drwGetPixel (const_winType sourceWindow, intType x, intType y)
{
  way_winType window = (way_winType) sourceWindow;

  if (window && window->buffer && window->buffer->content)
  { int pos = y * window->buffer->width + x;

    if (pos < window->buffer->width * window->buffer->height)
#ifdef USE_ALPHA
      return window->buffer->content[pos];
#else
    { intType result = window->buffer->content[pos];

      // FF 00 00 00 = 4278190080
      // Shift the first three bytes off (3*8 = 24) then negate the 4th byte (which is the alpha).
      return result ^ result >> 24 << 24; // Necessary?
    }
#endif
  }

  return 0;
}

// Returns the colour of each pixel in the window.
bstriType drwGetPixelData (const_winType sourceWindow)
{
  unsigned int pos;
  unsigned int maxPos;
  memSizeType result_size;
  uint32Type *image_data, pixel;
  bstriType result;
  way_winType window = (way_winType) sourceWindow;

  logFunction(printf("drwGetPixelData(" FMT_U_MEM ")\n", (memSizeType) sourceWindow););
  maxPos = window->width * window->height;
  result_size = maxPos * sizeof(uint32Type);
  if (unlikely(!ALLOC_BSTRI_SIZE_OK(result, result_size)))
    raise_error(MEMORY_ERROR);
  else
  { result->size = result_size;
    image_data = (uint32Type *) result->mem;
    if (window->buffer && window->buffer->content)
    { for (pos = 0; pos < maxPos; pos++)
      {
#ifdef USE_ALPHA
        *image_data = window->buffer->content[pos];
#else
        pixel = window->buffer->content[pos];
        *image_data = pixel ^ pixel >> 24 << 24; // Necessary?
#endif
        image_data++;
      }
    }
    else
      for (pos = 0; pos < maxPos; pos++)
      { *image_data = 0xFF000000; // Black.
        image_data++;
      }
  }
  return result;
}

/**
 *  Create a new pixmap with the given 'width' and 'height' from 'sourceWindow'.
 *  A rectangle with the upper left corner at ('left', 'upper') and the given
 *  'width' and 'height' is copied from 'sourceWindow' to the new pixmap.
 *  The rectangle may extend to areas outside of 'sourceWindow'. The rectangle
 *  areas outside of 'sourceWindow' are colored with black.
 *  @return the created pixmap.
 *  @exception RANGE_ERROR If 'height' or 'width' are negative or zero.
 */
winType drwGetPixmap
( const_winType sourceWindow,
  intType left,
  intType upper,
  intType width,
  intType height
)
{
  way_winType pixmap, source;
  intType x = 0, y = 0, dx, dy, dPos, sPos;

  logFunction(printf("drwGetPixmap(" FMT_U_MEM ", " FMT_D ", " FMT_D
                       ", " FMT_D ", " FMT_D ")\n",
                       (memSizeType) sourceWindow, left, upper,
                       width, height););
  source = (way_winType) sourceWindow;
  pixmap = (way_winType) drwNewPixmap(width, height);
  if (pixmap && prepare_buffer_data(&waylandState, pixmap))
  { if (source->buffer && source->buffer->content)
    { clamp_common_area(source, pixmap, &left, &upper, &width, &height, &x, &y);
      // printf("left: %ld  upper: %ld  width: %ld  height: %ld  x: %ld  y: %ld\n", left, upper, width, height, x, y);
      for (dy = 0; dy < pixmap->height; dy++)
      { dPos = dy*pixmap->width;
        for (dx = 0; dx < pixmap->width; dx++)
          if
          ( left+dx >= 0 && left+dx < source->width &&
            upper+dy >= 0 && upper+dy < source->height
          )
          { sPos = (upper+dy)*source->width + left;
            pixmap->buffer->content[dPos+dx] = source->buffer->content[sPos+dx];
          }
          else
            pixmap->buffer->content[dPos+dx] = 0xFF000000; // Black.
      }
    }
    else
      for (dy = 0; dy < pixmap->height; dy++)
      { dPos = dy*pixmap->width;
        for (dx = 0; dx < pixmap->width; dx++)
          pixmap->buffer->content[dPos+dx] = 0xFF000000; // Black.
      }
  }
  return (winType) pixmap;
}

intType drwHeight (const_winType actual_window)
{
  return ((way_winType) actual_window)->height;
}

winType drwImage (int32Type *image_data, memSizeType width, memSizeType height, boolType hasAlphaChannel)
{
  way_winType pixmap = NULL;
  int32Type *pixel;
  memSizeType pos, endPos;

  logFunction(printf("drwImage(" FMT_U_MEM ", " FMT_U_MEM ", %d)\n",
                     width, height, hasAlphaChannel););
  pixmap = (way_winType) drwNewPixmap(width, height);
  if (pixmap != NULL && prepare_buffer_data(&waylandState, pixmap))
  { pixel = image_data;
    pos = 0;
    endPos = width*height;
    for (pos = 0; pos < endPos; pos++)
    { pixmap->buffer->content[pos] = *pixel;
      pixel++;
    }
  }
  else
  if (pixmap)
  { logError(printf("drwImage: failed to open a new buffer.\n"););
    raise_error(GRAPHIC_ERROR);
  }
  logFunction(printf("drwImage --> " FMT_U_MEM " (usage=" FMT_U ")\n",
                     (memSizeType) pixmap,
                     pixmap != NULL ? pixmap->usage_count : (uintType) 0););
  return (winType) pixmap;
}

/**
 *  Create a new pixmap with the given 'width' and 'height'.
 *  @return the created pixmap.
 *  @exception RANGE_ERROR If 'height' or 'width' are negative or zero.
 */
winType drwNewPixmap (intType width, intType height)
{
  way_winType pixmap = NULL;

  logFunction(printf("drwNewPixmap(" FMT_D ", " FMT_D ")\n",
                       width, height););
  if (unlikely(width < 1 || width > UINT_MAX || height < 1 || height > UINT_MAX))
  { logError(printf("drwNewPixmap(" FMT_D ", " FMT_D "): "
                    "Raises RANGE_ERROR\n",
                    width, height););
    raise_error(RANGE_ERROR);
  }
  else
  { if (!init_called)
      drawInit();

    if (unlikely(!ALLOC_RECORD2(pixmap, way_winRecord, count.win, count.win_bytes)))
      raise_error(MEMORY_ERROR);
    else
    { memset(pixmap, 0, sizeof(way_winRecord));
      pixmap->usage_count = 1;
      pixmap->isPixmap = TRUE;
      pixmap->width = width;
      pixmap->height = height;
      pixmap->buffer = NULL;
      // printf("  address: %p\n", pixmap);
      // drwClear((winType) pixmap, 0xFF000000); // Clear window to black.
    }
  }
  logFunction(printf("drwNewPixmap --> " FMT_U_MEM " (usage=" FMT_U ")\n",
                     (memSizeType) pixmap,
                     pixmap != NULL ? pixmap->usage_count : (uintType) 0););
  return (winType) pixmap;
}

void drwPArc
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle,
  intType col
)
{
  logFunction(printf("drwPArc(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D
                       ", %.4f, %.4f, " F_X(08) ")\n",
                       (memSizeType) actual_window, x, y, radius,
                       startAngle, sweepAngle, col););
  if (sweepAngle >= PI2 || sweepAngle < -PI2)
    drwPCircle(actual_window, x, y, radius, col);
  else
  if
  ( unlikely(radius < 0 || radius > UINT_MAX / 2 ||
    x < INT_MIN + radius || x > INT_MAX ||
    y < INT_MIN + radius || y > INT_MAX ||
    os_isnan(startAngle) || os_isnan(sweepAngle) ||
    startAngle < (floatType) INT_MIN / (23040.0 / PI2) ||
    startAngle > (floatType) INT_MAX / (23040.0 / PI2) ||
    sweepAngle < (floatType) INT_MIN / (23040.0 / PI2) ||
    sweepAngle > (floatType) INT_MAX / (23040.0 / PI2))
  )
  { logError(printf("drwPArc(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D
                      ", %.4f, %.4f, " F_X(08) "): Raises RANGE_ERROR\n",
                      (memSizeType) actual_window, x, y, radius,
                      startAngle, sweepAngle, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
      drawRawArc(window, x, y, radius, startAngle, sweepAngle, col);
  }
}

void drwPCircle (const_winType actual_window, intType x, intType y, intType radius, intType col)
{
  logFunction(printf("drwPCircle(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D ", " F_X(08) ")\n",
                (memSizeType) actual_window, x, y, radius, col););
  if (unlikely(radius < 0 || radius > UINT_MAX / 2 ||
                 x < INT_MIN + radius || x > INT_MAX ||
                 y < INT_MIN + radius || y > INT_MAX))
  { logError(printf("drwPCircle(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D ", " F_X(08) "): Raises RANGE_ERROR\n",
              (memSizeType) actual_window, x, y, radius, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { /* Jesko's method for the midpoint circle algorithm:
        t1 = r / 16
        xd = r
        yd = 0
        Repeat Until xd < yd
            Pixel (xd, yd) and all symmetric pixels are colored (8 times)
            yd = yd + 1
            t1 = t1 + yd
            t2 = t1 - xd
            If t2 >= 0
                t1 = t2
                xd = xd - 1
      */
      intType t1 = radius / 16, // This division makes for a smoother edge.
        t2 = 0,
        xd = radius,
        yd = 0;
      intType pos = 0, maxPos = window->buffer->width * window->buffer->height;
      intType xPos = 0, yPos = 0;

      while (xd >= yd)
      { // Using the coordinate, render the eight mirrored points.
        // Render right, downward A (plunges from the middle)
        xPos = x + xd; yPos = y + yd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;
        // Render right, upward A (rises from the middle)
        yPos = y - yd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;
        // Render left, upward A
        xPos = x - xd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;
        // Render left, downward A (these first four draw the "parenthesis" portion: ( )   ;)
        yPos = y + yd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;
        // Render right, downard B (touches the bottom)
        xPos = x + yd; yPos = y + xd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;
        // Render right, upward B (touches the top)
        yPos = y - xd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;
        // Render left, upward B
        xPos = x - yd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;
        // Render left, downward B
        yPos = y + xd; pos = yPos*window->buffer->width + xPos;
          if (pos < maxPos) window->buffer->content[pos] = col;

        // Adjust position.
        yd += 1;
        t1 += yd;
        t2 = t1 - xd;

        if (t2 >= 0)
        { t1 = t2;
          xd -= 1;
        }
      }

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-radius, y-radius, x+radius, y+radius); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

void drwPFArc
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle,
  intType width,
  intType col
)
{
  int dr;

  logFunction(printf("drwPFArc(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D
                       ", %.4f, %.4f, " FMT_D ", " F_X(08) ")\n",
                       (memSizeType) actual_window, x, y, radius,
                       startAngle, sweepAngle, width, col););
  if (unlikely(radius < 0 || radius > UINT_MAX / 2 ||
                 x < INT_MIN + radius || x > INT_MAX ||
                 y < INT_MIN + radius || y > INT_MAX ||
                 width < 1 || width > radius ||
                 os_isnan(startAngle) || os_isnan(sweepAngle) ||
                 startAngle < (floatType) INT_MIN / (23040.0 / (2.0 * PI)) ||
                 startAngle > (floatType) INT_MAX / (23040.0 / (2.0 * PI)) ||
                 sweepAngle < (floatType) INT_MIN / (23040.0 / (2.0 * PI)) ||
                 sweepAngle > (floatType) INT_MAX / (23040.0 / (2.0 * PI))))
  { logError(printf("drwPFArc(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D
                    ", %.4f, %.4f, " FMT_D ", " F_X(08) "): "
                    "Raises RANGE_ERROR\n",
                    (memSizeType) actual_window, x, y, radius,
                    startAngle, sweepAngle, width, col););
   raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { for (dr = 0; dr < width; dr++)
      { drawRawArc(window, x, y, radius-dr, startAngle, sweepAngle, col);
        // There's definitely a more efficient way to fill the gaps...
        if (dr < width)
        { drawRawArc(window, x-1, y, radius-dr, startAngle, sweepAngle, col);
          drawRawArc(window, x+1, y, radius-dr, startAngle, sweepAngle, col);
          drawRawArc(window, x, y-1, radius-dr, startAngle, sweepAngle, col);
          drawRawArc(window, x, y+1, radius-dr, startAngle, sweepAngle, col);
        }
      }

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-radius, y-radius, x+radius, y+radius); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

// Unfinished: should tie in with jesko's method to produce a shape similar to other arcs/circles.
// Renders a circle, but skips the area outside the line formed by start and end angle.
void drwPFArcChord
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle,
  intType col
)
{
  logFunction(printf("drwPFArcChord(" FMT_U_MEM ", " FMT_D ", " FMT_D
                     ", " FMT_D ", %.4f, %.4f, " F_X(08) ")\n",
                     (memSizeType) actual_window, x, y, radius,
                     startAngle, sweepAngle, col););
  if (!sweepAngle || sweepAngle >= PI2 || sweepAngle <= -PI2)
    drwPFCircle(actual_window, x, y, radius, col);
  else
  if (unlikely(radius < 0 || radius > UINT_MAX / 2 ||
               x < INT_MIN + radius || x > INT_MAX ||
               y < INT_MIN + radius || y > INT_MAX ||
               os_isnan(startAngle) || os_isnan(sweepAngle) ||
               startAngle < (floatType) INT_MIN / (23040.0 / (2.0 * PI)) ||
               startAngle > (floatType) INT_MAX / (23040.0 / (2.0 * PI)) ||
               sweepAngle < (floatType) INT_MIN / (23040.0 / (2.0 * PI)) ||
               sweepAngle > (floatType) INT_MAX / (23040.0 / (2.0 * PI))))
  { logError(printf("drwPFArcChord(" FMT_U_MEM ", " FMT_D ", " FMT_D
                    ", " FMT_D ", %.4f, %.4f, " F_X(08) "): "
                    "Raises RANGE_ERROR\n",
                    (memSizeType) actual_window, x, y, radius,
                    startAngle, sweepAngle, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { /* Angles are given as radians. Start angle of 0 is right, sweeps counter-clockwise.
      As we are dealing with radians, 2 * PI is a full turn (or 360 degrees). */
      boolType crossingBound = false;
      startAngle = fmod(startAngle, PI2);
      if (startAngle < 0)
        startAngle += PI2;
      floatType endAngle = startAngle + sweepAngle;
      if (endAngle < 0)
      { crossingBound = true;
        endAngle += PI2;
      }
      else
      if (endAngle > PI2)
      { crossingBound = true;
        endAngle = fmod(endAngle, PI2);
      }
      // printf("Start: %.10f  End: %.10f  Crossing: %d\n", startAngle, endAngle, crossingBound);

      // Ensure the start angle is earlier in the arc.
      if (startAngle > endAngle)
      { floatType tempAngle = endAngle;
        endAngle = startAngle;
        startAngle = tempAngle;
      }

      const float Turn4 = PI/2.0;
      float slope;
      int x1, y1, x2, y2, posSign = -1; // Default is to fill above the line.
      intType xPos, yPos, pos, intercept;
      const intType maxPos = window->buffer->width * window->buffer->height;
      // printf("45: %f  90: %f  135: %f  225: %f  335: %f\n", tanf(Turn4/2.0), tanf(Turn4), tanf(Turn4*1.5), tanf(Turn4*2.5), tanf(Turn4*3.5));

      // Find point A.
      // Need to handle values very close to Turn4 due to floating point rounding issues.
      if (fabs(Turn4-fmod(startAngle,Turn4)) < 0.000001) // Slopeless angle.
      { if (startAngle < 1.0 || startAngle > PI2-1.0) // 0
        { x1 = radius;
          y1 = 0;
        }
        else
        if (startAngle < PI - 1.0) // Turn4
        { x1 = 0;
          y1 = -radius;
        }
        else
        if (startAngle < PI + Turn4 - 1.0) // PI
        { x1 = -radius;
          y1 = 0;
        }
        else // PI + Turn4
        { x1 = 0;
          y1 = radius;
        }
      }
      else
      { slope = -tanf(startAngle);
        // printf("slope1: %.10f\n", slope);
        x1 = !slope ? radius : sqrt((float)(radius*radius) / (slope*slope + 1.0));
        if (startAngle > Turn4 && startAngle < PI*1.5)
          x1 = -x1;
        y1 = slope * (float)x1;
      }

      // Find point B.
      if (fabs(Turn4-fmod(endAngle,Turn4)) < 0.000001) // Slopeless angle.
      { if (endAngle < 1.0 || endAngle > PI2-1.0) // 0
        { x2 = radius;
          y2 = 0;
        }
        else
        if (endAngle < PI - 1.0) // Turn4
        { x2 = 0;
          y2 = -radius;
        }
        else
        if (endAngle < PI + Turn4 - 1.0) // PI
        { x2 = -radius;
          y2 = 0;
        }
        else // PI + Turn4
        { x2 = 0;
          y2 = radius;
        }
      }
      else
      { slope = -tanf(endAngle); // We want positive slope to indicate a slant of (\)
        // printf("slope2: %.10f\n", slope);
        x2 = !slope ? radius : sqrt((float)(radius*radius) / (slope*slope + 1.0));
        if (endAngle > Turn4 && endAngle < PI*1.5)
          x2 = -x2;
        y2 = slope * (float)x2;
      }

      // Slope of A to B.
      slope = x2-x1 ? (float)(y2-y1)/(float)(x2-x1) : 0.0;
      xPos = x;// - radius;
      yPos = y;// - radius;
      if (slope)
        intercept = (-y1 / slope) + x1;

      // Determine whether or not to fill below the line.
      if (slope)
      { if (slope > 0)
        { if (!crossingBound && startAngle >= Turn4)
            posSign = 1; // Fill below the line.
        }
        else
        if (crossingBound || startAngle >= PI)
          posSign = 1;
      }
      else // Horizontal line.
      if (y1 == y2)
      { if (crossingBound == (startAngle < PI))
          posSign = 1; // Fill below the line.
      }
      else // Vertical line
      if (crossingBound)
        posSign = 1; // Fill to the right.

      // printf("startAngle: %f  sweep: %f  end: %f  Turn4: %.10f  slope: %.10f  intercept: %ld  fmod: %.10f  div: %.10f  ex: %.10f\n", startAngle, sweepAngle, endAngle, Turn4, slope, intercept, fmod(startAngle, Turn4), startAngle/Turn4, fabs(Turn4-fmod(startAngle,Turn4)));
      // printf("x1: %d y1: %d   x2: %d y2: %d  sign: %d\n", x1, y1, x2, y2, posSign);

      for (int yd = -radius; yd <= radius; yd++)
        for (int xd = -radius; xd <= radius; xd++)
        { pos = (yPos + yd)*window->buffer->width + (xPos + xd);
          if (pos <= maxPos && xd*xd + yd*yd <= radius*radius)
          { if (slope)// y = mx + b
            { if (posSign == 1 && yd >= slope * (float)(xd - intercept) || posSign == -1 && yd <= slope * (float)(xd - intercept))
                window->buffer->content[pos] = col;
            }
            else // Horizontal line.
            if (y1 == y2)
            { if (posSign == 1 && yd >= y1 || posSign == -1 && yd <= y1)
                window->buffer->content[pos] = col;
            }
            else // Vertical line.
            if (posSign == 1 && xd >= x1 || posSign == -1 && xd <= x1)
                window->buffer->content[pos] = col;
          }
        }

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-radius, y-radius, x+radius, y+radius); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

// Could likely be optimized (specifically in the gap-filling, as the whole extra line isn't necessary).
void drwPFArcPieSlice
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle,
  intType col
)
{
  logFunction(printf("drwPFArcPieSlice(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D
                       ", %.4f, %.4f, " F_X(08) ")\n",
                       (memSizeType) actual_window, x, y, radius,
                       startAngle, sweepAngle, col););
  if (sweepAngle >= PI2 || sweepAngle <= -PI2)
    drwPFCircle(actual_window, x, y, radius, col);
  else
  if
  ( unlikely(radius < 0 || radius > UINT_MAX / 2 ||
    x < INT_MIN + radius || x > INT_MAX ||
    y < INT_MIN + radius || y > INT_MAX ||
    os_isnan(startAngle) || os_isnan(sweepAngle) ||
    startAngle < (floatType) INT_MIN / (23040.0 / PI2) ||
    startAngle > (floatType) INT_MAX / (23040.0 / PI2) ||
    sweepAngle < (floatType) INT_MIN / (23040.0 / PI2) ||
    sweepAngle > (floatType) INT_MAX / (23040.0 / PI2))
  )
  { logError(printf("drwPFArcPieSlice(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D
                      ", %.4f, %.4f, " F_X(08) "): Raises RANGE_ERROR\n",
                      (memSizeType) actual_window, x, y, radius,
                      startAngle, sweepAngle, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { /* Angles are given as radians. Start angle of 0 is right, sweeps counter-clockwise.
      As we are dealing with radians, 2 * PI is a full turn (or 360 degrees).
      Jesko's method works with 1/8 turns. */
      const float Turn8 = PI2 / 8.0;
      boolType crossingBound = false; // Used to control how we compare to the start/end angles.
      startAngle = fmod(startAngle, PI2); // Discard full turns.
      if (startAngle < 0) // When negative, find the corresponding positive angle.
        startAngle += PI2;
      floatType endAngle = startAngle + sweepAngle; // Sweep to end.
      if (endAngle < 0) // When negative, find corresponding, but also note that we are crossing the "bound" (being zero).
      { crossingBound = true;
        endAngle += PI2;
      }
      else
      if (endAngle > PI2) // Also crossing the bound when the end is greater than a full turn.
      { crossingBound = true;
        endAngle = fmod(endAngle, PI2);
      }

      // Ensure the start angle is earlier in the arc (for our later angle checks).
      if (startAngle > endAngle)
      { floatType tempAngle = endAngle;
        endAngle = startAngle;
        startAngle = tempAngle;
      }

      // Set up Jesko's variables.
      intType t1 = radius / 16, // This division makes for a smoother edge.
        t2 = 0,
        xd = radius,
        yd = 0;
      // And some of our own.
      intType xPos = 0, yPos = 0;
      floatType angle = 0.0;
      boolType drewPrior[8] = {false}; // Starts at the right, rotating counter-clockwise.
      boolType xChanged = false;

      while (xd >= yd)
      { // Using the coordinate, render the eight mirrored lines, when appropriate.
        // Render right, downward A (plunges from the middle)
        xPos = x + xd; yPos = y + yd; angle = PI2 - ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            // When both coordinates have changed, render an extra line to cover the gaps.
            if (xChanged && drewPrior[7])
              drawRawLine(window, x, y, xPos, yPos-1, col);
            drewPrior[7] = true;
          }
          else
            drewPrior[7] = false;
        // Render right, upward A (rises from the middle)
        yPos = y - yd; angle = ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            if (xChanged && drewPrior[0])
              drawRawLine(window, x, y, xPos, yPos+1, col);
            drewPrior[0] = true;
          }
          else
            drewPrior[0] = false;
        // Render left, upward A
        xPos = x - xd; angle = PI - ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            if (xChanged && drewPrior[3])
              drawRawLine(window, x, y, xPos, yPos+1, col);
            drewPrior[3] = true;
          }
          else
            drewPrior[3] = false;
        // Render left, downward A (these first four draw the "parenthesis" portion: ( )
        yPos = y + yd; angle = PI + ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            if (xChanged && drewPrior[4])
              drawRawLine(window, x, y, xPos, yPos-1, col);
            drewPrior[4] = true;
          }
          else
            drewPrior[4] = false;
        // Render right, downard B (touches the bottom)
        xPos = x + yd; yPos = y + xd; angle = PI2*0.75 + ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            if (xChanged && drewPrior[6])
              drawRawLine(window, x, y, xPos-1, yPos, col);
            drewPrior[6] = true;
          }
          else
            drewPrior[6] = false;
        // Render right, upward B (touches the top)
        yPos = y - xd; angle = PI*0.5 - ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            if (xChanged && drewPrior[1])
              drawRawLine(window, x, y, xPos-1, yPos, col);
            drewPrior[1] = true;
          }
          else
            drewPrior[1] = false;
        // Render left, upward B
        xPos = x - yd; angle = PI*0.5 + ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            if (xChanged && drewPrior[2])
              drawRawLine(window, x, y, xPos+1, yPos, col);
            drewPrior[2] = true;
          }
          else
            drewPrior[2] = false;
        // Render left, downward B
        yPos = y + xd; angle = PI2*0.75 - ((float)yd / (float)xd) * Turn8;
          if (!crossingBound && angle >= startAngle && angle <= endAngle || crossingBound && (angle <= startAngle || angle >= endAngle))
          { drawRawLine(window, x, y, xPos, yPos, col);
            if (xChanged && drewPrior[5])
              drawRawLine(window, x, y, xPos+1, yPos, col);
            drewPrior[5] = true;
          }
          else
            drewPrior[5] = false;

        // Adjust position.
        yd += 1;
        t1 += yd;
        t2 = t1 - xd;

        if (t2 >= 0)
        { t1 = t2;
          xd -= 1;
          xChanged = true;
        }
        else
          xChanged = false;
      }

      /* Fill in the gaps between the diagonal octant joins. These get missed in the main loop due
      to these lines existing between the last lines drawn. The extra orthogonal lines aren't needed
      because, the straight lines leave no gaps. */
      xd = sqrt((radius*radius) / 2.0),
      yd = xd;
      if (drewPrior[0] && drewPrior[1])
        drawRawLine(window, x, y, x+xd, y-yd, col);
      if (drewPrior[2] && drewPrior[3])
        drawRawLine(window, x, y, x-xd, y-yd, col);
      if (drewPrior[4] && drewPrior[5])
        drawRawLine(window, x, y, x-xd, y+yd, col);
      if (drewPrior[6] && drewPrior[7])
        drawRawLine(window, x, y, x+xd, y+yd, col);

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-radius, y-radius, x+radius, y+radius); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

void drwPFCircle
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  intType col
)
{
  logFunction(printf("drwPFCircle(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D ", " F_X(08) ")\n",
                (memSizeType) actual_window, x, y, radius, col););
  if (unlikely(radius < 0 || radius > UINT_MAX / 2 ||
                 x < INT_MIN + radius || x > INT_MAX ||
                 y < INT_MIN + radius || y > INT_MAX))
  { logError(printf("drwPFCircle(" FMT_U_MEM ", " FMT_D ", " FMT_D ", " FMT_D ", " F_X(08) "): Raises RANGE_ERROR\n",
              (memSizeType) actual_window, x, y, radius, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { // Again, using Jesko's method, but combining two horizontal points to form a line
      intType t1 = radius / 16,
        t2 = 0,
        xd = radius,
        yd = 0;
      intType maxPos = window->buffer->width * window->buffer->height;
      intType x1 = 0, x2 = 0, yPos = 0;

      while (xd >= yd)
      { // Using the coordinate, render the four lines.
        // Render inner, upward
        x1 = x - xd; yPos = y - yd;
        x2 = x + xd;
        for (intType z = 0, pos = yPos*window->buffer->width + x1; z <= x2-x1; z++, pos++)
          if (pos < maxPos)
            window->buffer->content[pos] = col;

        // Render inner, downward
        x1 = x - xd; yPos = y + yd;
        x2 = x + xd;
        for (intType z = 0, pos = yPos*window->buffer->width + x1; z <= x2-x1; z++, pos++)
          if (pos < maxPos)
            window->buffer->content[pos] = col;

        // Render outer, upward
        x1 = x - yd; yPos = y - xd;
        x2 = x + yd;
        for (intType z = 0, pos = yPos*window->buffer->width + x1; z <= x2-x1; z++, pos++)
          if (pos < maxPos)
            window->buffer->content[pos] = col;

        // Render outer, downward
        x1 = x - yd; yPos = y + xd;
        x2 = x + yd;
        for (intType z = 0, pos = yPos*window->buffer->width + x1; z <= x2-x1; z++, pos++)
          if (pos < maxPos)
            window->buffer->content[pos] = col;

        // Adjust position.
        yd += 1;
        t1 += yd;
        t2 = t1 - xd;

        if (t2 >= 0)
        { t1 = t2;
          xd -= 1;
        }
      }

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-radius, y-radius, x+radius, y+radius); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

// Does not yet have an action associated with it.
void drwPEllipse
( const_winType actual_window,
  intType x,
  intType y,
  intType width,
  intType height,
  intType col
)
{
  logFunction(printf("drwPEllipse(" FMT_U_MEM ", " FMT_D ", " FMT_D
                     ", " FMT_D ", " FMT_D ", " F_X(08) ")\n",
                     (memSizeType) actual_window, x, y,
                     width, height, col););
  if (unlikely(!inIntRange(x) || !inIntRange(y) ||
                 width < 1 || width > UINT_MAX ||
                 height < 1 || height > UINT_MAX))
  { logError(printf("drwPEllipse(" FMT_U_MEM ", " FMT_D ", " FMT_D
                    ", " FMT_D ", " FMT_D ", " F_X(08) "): "
                    "Raises RANGE_ERROR\n",
                    (memSizeType) actual_window, x, y,
                    width, height, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { /* Utilizing the Freeman approach as laid out in the paper
      There Is No Royal Road to Programs:
        https://cs.dartmouth.edu/~doug/155.pdf */
      intType xc = x, yc = y, a = width, b = height;
      /* e(x,y) = bˆ2*xˆ2 + aˆ2*yˆ2 - aˆ2*bˆ2 */
      x = 0, y = b;
      long a2 = (long)a*a, b2 = (long)b*b;
      long crit1 = -(a2/4 + a%2 + b2);
      long crit2 = -(b2/4 + b%2 + a2);
      long crit3 = -(b2/4 + b%2);
      long t = -a2*y; /* t = e(x+1/2,y-1/2) - (aˆ2+bˆ2)/4 */
      long dxt = 2*b2*x, dyt = -2*a2*y;
      long d2xt = 2*b2, d2yt = 2*a2;
      while(y>=0 && x<=a)
      { // Render points.
        drawRawPoint(window, xc+x, yc+y, col); // point(xc+x, yc+y);
        if (x!=0 || y!=0)
          drawRawPoint(window, xc-x, yc-y, col); // point(xc-x, yc-y);
        if (x!=0 && y!=0)
        { drawRawPoint(window, xc+x, yc-y, col); // point(xc+x, yc-y);
          drawRawPoint(window, xc-x, yc+y, col); // point(xc-x, yc+y);
        }
        if (t + b2*x <= crit1 || /* e(x+1,y-1/2) <= 0 */
            t + a2*y <= crit3) /* e(x+1/2,y) <= 0 */
        { // incx();
          x++;
          dxt += d2xt;
          t += dxt;
        }
        // Adjust variables.
        else if (t - a2*y > crit2) /* e(x+1/2,y-1) > 0 */
        { // incy();
          y--;
          dyt += d2yt;
          t += dyt;
        }
        else
        { // incx(); and incy();
          x++; dxt += d2xt; t += dxt;
          y--; dyt += d2yt; t += dyt;
        }
      }

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-width, y-height, x+width, y+height);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

void drwPFEllipse
( const_winType actual_window,
  intType x,
  intType y,
  intType width,
  intType height,
  intType col
)
{
  logFunction(printf("drwPFEllipse(" FMT_U_MEM ", " FMT_D ", " FMT_D
                     ", " FMT_D ", " FMT_D ", " F_X(08) ")\n",
                     (memSizeType) actual_window, x, y,
                     width, height, col););
  if (unlikely(!inIntRange(x) || !inIntRange(y) ||
                 width < 1 || width > UINT_MAX ||
                 height < 1 || height > UINT_MAX))
  { logError(printf("drwPFEllipse(" FMT_U_MEM ", " FMT_D ", " FMT_D
                    ", " FMT_D ", " FMT_D ", " F_X(08) "): "
                    "Raises RANGE_ERROR\n",
                    (memSizeType) actual_window, x, y,
                    width, height, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { // Utilizing the Freeman approach as laid out in: https://cs.dartmouth.edu/~doug/155.pdf
      intType a = width/2, b = height/2, xc = x+a, yc = y+b;
      /* e(x,y) = bˆ2*xˆ2 + aˆ2*yˆ2 - aˆ2*bˆ2 */
      x = 0, y = b;
      long a2 = (long)a*a, b2 = (long)b*b;
      long crit1 = -(a2/4 + a%2 + b2);
      long crit2 = -(b2/4 + b%2 + a2);
      long crit3 = -(b2/4 + b%2);
      long t = -a2*y; /* t = e(x+1/2,y-1/2) - (aˆ2+bˆ2)/4 */
      long dxt = 2*b2*x, dyt = -2*a2*y;
      long d2xt = 2*b2, d2yt = 2*a2;
      while(y>=0 && x<=a)
      { // Render lines (to fill the ellipse).
        if (x!=0 && y!=0)
        { drawRawLine(window, xc+x, yc+y, xc-x, yc+y, col);
          drawRawLine(window, xc+x, yc-y, xc-x, yc-y, col);
        }
        else if (x!=0)
          drawRawLine(window, xc+x, yc, xc-x, yc, col);
        else
        { drawRawPoint(window, xc, yc+y, col);
          if (y!=0)
            drawRawPoint(window, xc, yc-y, col);
        }
        // Adjust variables.
        if (t + b2*x <= crit1 || /* e(x+1,y-1/2) <= 0 */
            t + a2*y <= crit3) /* e(x+1/2,y) <= 0 */
        { // incx();
          x++;
          dxt += d2xt;
          t += dxt;
        }
        else if (t - a2*y > crit2) /* e(x+1/2,y-1) > 0 */
        { // incy();
          y--;
          dyt += d2yt;
          t += dyt;
        }
        else
        { // incx(); and incy();
          x++; dxt += d2xt; t += dxt;
          y--; dyt += d2yt; t += dyt;
        }
      }

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-width, y-height, x+width, y+height);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

// Copied from drw_emc.c
void drwPixelToRgb (intType col, intType *redLight, intType *greenLight, intType *blueLight)
{
  *redLight   = (intType) (( ((uintType) col) >> 16       ) << 8);
  *greenLight = (intType) (((((uintType) col) >>  8) & 255) << 8);
  *blueLight  = (intType) (( ((uintType) col)        & 255) << 8);
}

// Also gets used for the Seed7 function "box" (for each of the four lines).
void drwPLine (const_winType actual_window, intType x1, intType y1, intType x2, intType y2, intType col)
{
  logFunction(printf("drwPLine(" FMT_U_MEM ", " FMT_D ", " FMT_D
                       ", " FMT_D ", " FMT_D ", " F_X(08) ")\n",
                       (memSizeType) actual_window, x1, y1,
                       x2, y2, col););
  if (unlikely(!(inIntRange(x1) && inIntRange(y1) &&
                   inIntRange(x2) && inIntRange(y2))))
  { logError(printf("drwPLine(" FMT_U_MEM ", " FMT_D ", " FMT_D
                      ", " FMT_D ", " FMT_D ", " F_X(08) "): "
                      "Raises RANGE_ERROR\n",
                      (memSizeType) actual_window, x1, y1,
                      x2, y2, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { drawRawLine(window, x1, y1, x2, y2, col);

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x1, y1, x2-x1, y2-y1); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

// Unfinished.
intType drwPointerXpos (const_winType actual_window)
{
  return 0;
}

// Unfinished.
intType drwPointerYpos (const_winType actual_window)
{
  return 0;
}

void drwPolyLine (const_winType actual_window, intType x, intType y, bstriType point_list, intType col)
{
  int *coords;
  memSizeType numCoords, point;
  intType left = x, right = x, top = y, bottom = y,
    x1, y1, x2, y2;

  logFunction(printf("drwPolyLine(" FMT_U_MEM ", " FMT_D ", " FMT_D
                     ", " FMT_U_MEM ", " F_X(08) ")\n",
                     (memSizeType) actual_window, x, y,
                     (memSizeType) point_list, col););
  if (unlikely(!inShortRange(x) || !inShortRange(y)))
  { logError(printf("drwPolyLine(" FMT_U_MEM ", " FMT_D ", " FMT_D
                    ", " FMT_U_MEM ", " F_X(08) "): "
                    "Raises RANGE_ERROR\n",
                    (memSizeType) actual_window, x, y,
                    (memSizeType) point_list, col););
    raise_error(RANGE_ERROR);
  }
  else
  { coords = (int *) point_list->mem;
    numCoords = point_list->size / sizeof(int);

    if (numCoords >= 4)
    { way_winType window = (way_winType) actual_window;
      if (prepare_buffer_copy(&waylandState, window))
      { // Every line is drawn from the last point, relative to the given x and y.
        for (point = 2; point+1 < numCoords; point+=2)
        { x1 = x+coords[point-2];
          y1 = y+coords[point-1];
          x2 = x+coords[point];
          y2 = y+coords[point+1];
          drawRawLine(window, x1, y1, x2, y2, col);
          if (x1 < left) left = x1;
          else if (x1 > right) right = x1;
          if (x2 < left) left = x2;
          else if (x2 > right) right = x2;
          if (y1 < top) top = y1;
          else if (y1 > bottom) bottom = y1;
          if (y2 < top) top = y2;
          else if (y2 > bottom) bottom = y2;
        }

        if (!window->isPixmap)
        { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
          wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
          wl_surface_damage_buffer(window->surface, left, top, right-left, bottom-top); // INT32_MAX, INT32_MAX);
          wl_surface_commit(window->surface);
          // wl_display_flush(waylandState.display);
          // wl_display_dispatch_pending(waylandState.display);
        }
      }
    }
  }
}

void drwPPoint (const_winType actual_window, intType x, intType y, intType col)
{
  way_winType window;
  int pos;

  logFunction(printf("drwPPoint(" FMT_U_MEM ", " FMT_D ", " FMT_D
                     ", " F_X(08) ")\n",
                     (memSizeType) actual_window, x, y, col););
  if (unlikely(!inIntRange(x) || !inIntRange(y)))
  { logError(printf("drwPPoint(" FMT_U_MEM ", " FMT_D ", " FMT_D
                    ", " F_X(08) "): Raises RANGE_ERROR\n",
                    (memSizeType) actual_window, x, y, col););
    raise_error(RANGE_ERROR);
  }
  else
  { window = (way_winType) actual_window;
    pos = y*window->width + x;

    if (pos < window->width * window->height && prepare_buffer_copy(&waylandState, window))
    { window->buffer->content[pos] = col;

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x, y, 1, 1); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        // wl_display_flush(waylandState.display);
        // wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

void drwPRect (const_winType actual_window, intType x, intType y, intType width, intType height, intType col)
{
  logFunction(printf("drwPRect(" FMT_U_MEM ", " FMT_D ", " FMT_D
                       ", " FMT_D ", " FMT_D ", " F_X(08) ")\n",
                       (memSizeType) actual_window, x, y,
                       width, height, col););
  if (unlikely(!inIntRange(x) || !inIntRange(y) ||
                 width < 0 || width > UINT_MAX ||
                 height < 0 || height > UINT_MAX))
  { logError(printf("drwPRect(" FMT_U_MEM ", " FMT_D ", " FMT_D
                      ", " FMT_D ", " FMT_D ", " F_X(08) "): "
                      "Raises RANGE_ERROR\n",
                      (memSizeType) actual_window, x, y,
                      width, height, col););
    raise_error(RANGE_ERROR);
  }
  else
  { way_winType window = (way_winType) actual_window;

    if (prepare_buffer_copy(&waylandState, window))
    { for (int yPos = y; yPos < y + height; yPos++)
        for (int xPos = x; xPos < x + width; xPos++)
        { int pos = yPos*window->buffer->width + xPos;

          if (pos < window->buffer->width * window->buffer->height)
            window->buffer->content[pos] = col;
          else
            goto End;
        }
      End:;

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x, y, width, height); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        // wl_display_flush(waylandState.display);
        // wl_display_dispatch_pending(waylandState.display);
      }
    }
  }
}

void drwPut (const_winType destWindow, intType xDest, intType yDest, const_winType pixmap)
{
  if (unlikely(!inIntRange(xDest) || !inIntRange(yDest)))
    raise_error(RANGE_ERROR);
  else
  if (destWindow && pixmap)
  { way_winType destination = (way_winType) destWindow;
    way_winType source = (way_winType) pixmap;

    if
    ( xDest < destination->width && yDest < destination->height &&
      source->buffer && source->buffer->content &&
      prepare_buffer_copy(&waylandState, destination)
    )
    { // Copy the data to the destination.
      for (int y = 0; y < source->buffer->height && y+yDest < destination->height; y++)
        for (int x = 0; x < source->buffer->width && x+xDest < destination->width; x++)
        { int pos = y * source->buffer->width + x;
          int dx = xDest + x,
              dy = yDest + y,
              dpos = dy * destination->buffer->width + dx;
          destination->buffer->content[dpos] = source->buffer->content[pos];
        }

      // If the destination is a Wayland window, send the data.
      if (!destination->isPixmap)
      { wl_buffer_add_listener(destination->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(destination->surface, destination->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(destination->surface, xDest, yDest, source->buffer->width, source->buffer->height); // Damage the affected area (as only that section needs updating).
        wl_surface_commit(destination->surface);
        // wl_display_dispatch(waylandState.display);
      }
    }
  }
}

// Unfinished.
void drwPutScaled
( const_winType destWindow,
  intType xDest,
  intType yDest,
  intType width,
  intType height,
  const_winType pixmap
)
{}

/* Controls the internal processing of colours (looks like it's filling alpha with 0xff).
Code Copied from drw_emc.c */
intType drwRgbColor (intType redLight, intType greenLight, intType blueLight)
{
  return (intType) ((((((uintType) redLight)   >> 8) & 255) << 16) |
                    (((((uintType) greenLight) >> 8) & 255) <<  8) |
                     ((((uintType) blueLight)  >> 8) & 255)        | 0xff000000);
}

void drwSetCloseAction (winType actual_window, intType closeAction)
{ /* drwSetCloseAction */
  logFunction(printf("drwSetCloseAction(" FMT_U_MEM ", " FMT_D ")\n",
                     (memSizeType) actual_window, closeAction););
  if (closeAction < 0 || closeAction > 2) {
    logError(printf("drwSetCloseAction(" FMT_U_MEM ", " FMT_D "): "
                    "Close action not in allowed range\n",
                    (memSizeType) actual_window, closeAction););
    raise_error(RANGE_ERROR);
  } else {
    ((way_winType) actual_window)->close_action = (int) closeAction;
  } /* if */
} /* drwSetCloseAction */

void drwSetContent (const_winType actual_window, const_winType pixmap)
{
  logFunction(printf("drwSetContent(" FMT_U_MEM ", " FMT_U_MEM ")\n",
                     (memSizeType) actual_window, (memSizeType) pixmap););
  drwPut(actual_window, 0, 0, pixmap);
}

// Note: only handles the main window.
void drwSetCursorVisible (winType aWindow, boolType visible)
{
  if (waylandState.wlPointer && waylandState.pointerEnterId)
  { if (visible == waylandState.hidePointer)
    { if (!visible)
        wl_pointer_set_cursor(waylandState.wlPointer, waylandState.pointerEnterId, 0, 0, 0);
      else
      { struct wl_cursor *cursor;
        struct wl_cursor_theme *theme;

        // Load the standard theme
        theme = wl_cursor_theme_load(NULL, 24, waylandState.sharedMemory);

        if (theme)
        { cursor = wl_cursor_theme_get_cursor(theme, "default"); // Load the default image

          // If it doesn't yet exist, create the surface for the cursor.
          if (!waylandState.pointerSurface)
            waylandState.pointerSurface = wl_compositor_create_surface(waylandState.compositor);

          if (cursor && waylandState.pointerSurface)
          { // Set the surface's role to that of a pointer.
            wl_pointer_set_cursor(waylandState.wlPointer, waylandState.pointerEnterId, waylandState.pointerSurface, cursor->images[0]->hotspot_x, cursor->images[0]->hotspot_y);
            // Attach the cursor image to the surface.
            wl_surface_attach(waylandState.pointerSurface, wl_cursor_image_get_buffer(cursor->images[0]), 0, 0);
            wl_surface_commit(waylandState.pointerSurface);
          }
        }
      }

      waylandState.hidePointer = !visible;
    }
  }
}

// Unfinished. (move the pointer to a point within the window)
void drwSetPointerPos (const_winType aWindow, intType xPos, intType yPos)
{}

// Unfinished. (move the window to a point)
void drwSetPos (const_winType actual_window, intType xPos, intType yPos)
{}

void drwSetSize (winType actual_window, intType width, intType height)
{
  logFunction(printf("drwSetSize(" FMT_U_MEM ", " FMT_D ", " FMT_D ")\n", (memSizeType) actual_window, width, height););
  if (unlikely(width < 1 || width > INT_MAX || height < 1 || height > INT_MAX))
  { logError(printf("drwSetSize(" FMT_D ", " FMT_D "): "
                    "Illegal window dimensions\n",
                    width, height););
    raise_error(RANGE_ERROR);
  }
  else
    resizeWindow((way_winType) actual_window, width, height, FALSE);
}

void redrawWindow (way_winType window)
{
  if (window->width > 0 && window->height > 0)
  { uint32_t *oldContent = window->buffer ? window->buffer->content : 0;
    int oldWidth = window->width;
    int oldHeight = window->height;

    if (!window->buffer)
    { oldWidth = 0;
      oldHeight = 0;
    }

    if (prepare_buffer_data(&waylandState, window))
    { // Set the new area (if any) to black, while retaining the old data (if possible).
      for (int y = 0; y < window->buffer->height; y++)
        for (int x = 0; x < window->buffer->width; x++)
        { int pos = y * window->buffer->width + x,
              oldPos = y * oldWidth + x;

          if (oldContent && x < oldWidth && y < oldHeight)
            window->buffer->content[pos] = oldContent[oldPos];
          else
            window->buffer->content[pos] = 0x0;
        }

      wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
      wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0); // Apply the buffer to the surface.
      wl_surface_damage_buffer(window->surface, 0, 0, window->buffer->width, window->buffer->height); // INT32_MAX, INT32_MAX);
      wl_surface_commit(window->surface);

      if (wl_display_flush(waylandState.display))
        wl_display_dispatch(waylandState.display);
    }
  }
}

// Unfinished? X11 retains the buffer data when a window gets shrunk, so resizing back up restores the data.
void resizeWindow (way_winType window, intType width, intType height, bool triggerKey)
{
  uint32_t *oldContent = window->buffer ? window->buffer->content : 0;
  int oldWidth = window->width;
  int oldHeight = window->height;
  window->width = width;
  window->height = height;
  // struct BufferData *data = wayland_prepare_buffer_data(&waylandState, window); //(way_winType) actual_window);

  if (prepare_buffer_data(&waylandState, window))
  { // Set the new area (if any) to black, while retaining the old data (if possible).
    for (int y = 0; y < window->buffer->height; y++)
      for (int x = 0; x < window->buffer->width; x++)
      { int pos = y * window->buffer->width + x,
            oldPos = y * oldWidth + x;

        if (oldContent && x < oldWidth && y < oldHeight)
          window->buffer->content[pos] = oldContent[oldPos];
        else
          window->buffer->content[pos] = 0x0;
      }

    if (!window->isPixmap)
    { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
      wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0); // Apply the buffer, which will resize the surface.

      // Update only the changed regions.
      if (window->width > oldWidth)
        wl_surface_damage_buffer(window->surface, oldWidth, 0, window->buffer->width - oldWidth, window->buffer->height);
      if (window->height > oldHeight)
        wl_surface_damage_buffer(window->surface, 0, oldHeight, window->buffer->width, window->buffer->height - oldHeight); // INT32_MAX, INT32_MAX);

      wl_surface_commit(window->surface);
      if (wl_display_flush(waylandState.display))
        wl_display_dispatch(waylandState.display);
    }
  }

  if (triggerKey && (window->width != oldWidth || window->height != oldHeight))
    expand_key_history(&waylandState, K_RESIZE);
}

// Unfinished.
void drwSetTransparentColor (winType pixmap, intType col)
{}

void drwSetWindowName (winType aWindow, const const_striType windowName)
{
  way_winType window = (way_winType) aWindow;

  if (window->xdgTopLevel)
  { errInfoType error = OKAY_NO_ERROR;
    os_striType newName = stri_to_os_stri(windowName, &error);

    if (error == OKAY_NO_ERROR)
      xdg_toplevel_set_title(window->xdgTopLevel, newName);
  }
}

// Unfinished.
void drwText
( const_winType actual_window,
  intType x,
  intType y,
  const const_striType stri,
  intType col,
  intType bkcol
)
{}

// Unfinished. (should draw below all other windows)
void drwToBottom (const_winType actual_window)
{}

// Unfinished. (should draw above all other windows)
void drwToTop (const_winType actual_window)
{}

intType drwWidth (const_winType actual_window)
{
  return ((way_winType) actual_window)->width;
}

// Unfinished. (top-left x coordinate)
intType drwXPos (const_winType actual_window)
{
  return 0;
}

// Unfinished. (top-left y coordinate)
intType drwYPos (const_winType actual_window)
{
  return 0;
}

// Mostly copied from drw_emc.c
winType generateEmptyWindow (void)
{
  way_winType newWindow;

  if (unlikely(!ALLOC_RECORD2(newWindow, way_winRecord, count.win, count.win_bytes)))
    raise_error(MEMORY_ERROR);
  else
  { memset(newWindow, 0, sizeof(way_winRecord));
    newWindow->usage_count = 0;
    newWindow->isPixmap = TRUE;
    /*newWindow->window = 0;
    newWindow->is_subwindow = FALSE;
    newWindow->is_substitute = FALSE;
    newWindow->parentWindow = NULL;
    newWindow->ignoreFirstResize = 0;
    newWindow->creationTimestamp = 0;*/
    newWindow->width = 0;
    newWindow->height = 0;
  }
  return (winType) newWindow;
}

void setResizeReturnsKey (winType resizeWindow, boolType active)
{ /* setResizeReturnsKey */
  ((way_winType) resizeWindow)->resizeReturnsKey = active;
} /* setResizeReturnsKey */
