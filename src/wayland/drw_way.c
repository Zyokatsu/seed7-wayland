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

/*#include "stdio.h"
#include "limits.h"*/

void gkbInitKeyboard (void);

/*- Globals ----------------------------
--------------------------------------*/
#define PI  3.141592653589793238462643383279502884197
#define PI2 6.283185307179586476925286766559005768394
static winType globalEmptyWindow = 0;
bool init_called = FALSE;
struct ClientState waylandState = { 0 }; // Initial state (all nulls/zeros).
//way_winType primaryWindow = 0;
const bool AlphaEnabled = FALSE;

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

intType drwScreenHeight (void)
{
  if (!init_called)
    drawInit();
  puts("Screen height called.");
  return (intType) waylandState.outputHeight;
}

intType drwScreenWidth (void)
{
  if (!init_called)
    drawInit();
  puts("Screen width called.");
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
      printf("  Window initialized: %p", newWindow);

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
void drwArc
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle
)
{}

// Unfinishied.
void drwArc2
( const_winType actual_window,
  intType x1,
  intType y1,
  intType x2,
  intType y2,
  intType radius
)
{}

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

// Unfinished.
void drwClear (winType actual_window, intType col)
{
  if (!init_called)
    drawInit();

  way_winType window = (way_winType) actual_window;
  printf("Clear color: %ld\n", col);

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

void drwColor (intType col)
{}

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

// Unfinished.
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
  puts("Draw copy area.");
}

// Unfinished?
winType drwEmpty (void)
{
  return globalEmptyWindow;
}

void drwFArcChord
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle
)
{}

void drwFArcPieSlice
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle
)
{}

void drwFCircle (const_winType actual_window, intType x, intType y, intType radius)
{}

void drwFEllipse (const_winType actual_window, intType x, intType y, intType width, intType height)
{}

// Unfinished.
void drwFPolyLine (const_winType actual_window, intType x, intType y, bstriType point_list, intType col)
{}

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
    { intType result = window->buffer->content[pos];

      // FF 00 00 00 = 4278190080
      // Shift the first three bytes off (3*8 = 24) then negate the 4th byte (which is the alpha).
      return AlphaEnabled ? result : result ^ result >> 24 << 24; // Necessary?
    }
  }

  return 0;
}

// Unfinished.
bstriType drwGetPixelData (const_winType sourceWindow)
{
  puts("Draw get pixel data called.");
  bstriType result = NULL;
  return result;
}

// Unfinished.
winType drwGetPixmap
( const_winType sourceWindow,
  intType left,
  intType upper,
  intType width,
  intType height
)
{
  way_winType pixmap = NULL;
  return (winType) pixmap;
}

intType drwHeight (const_winType actual_window)
{
  return ((way_winType) actual_window)->height;
}

// Unfinished.
winType drwImage (int32Type *image_data, memSizeType width, memSizeType height, boolType hasAlphaChannel)
{
  way_winType pixmap = NULL;
  return (winType) pixmap;
}

// Unfinished.
void drwLine (const_winType actual_window, intType x1, intType y1, intType x2, intType y2)
{}

// Unfinished.
winType drwNewPixmap (intType width, intType height)
{
  puts("drwNewPixmap called.");
  way_winType pixmap = NULL;

  if (unlikely(!inIntRange(width) || !inIntRange(height) || width < 1 || height < 1))
    raise_error(RANGE_ERROR);
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
    }
  }

  return (winType) pixmap;
}

// Could likey be optimized.
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
  if (sweepAngle >= PI2)
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
    { /* Angles are given as radians. Start angle of 0 is right, sweeps counter-clockwise.
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
      // printf("Start angle: %f   End angle: %f\n", startAngle, endAngle);

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

      if (!window->isPixmap)
      { wl_buffer_add_listener(window->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(window->surface, window->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(window->surface, x-radius, y-radius, x+radius, y+radius); // INT32_MAX, INT32_MAX);
        wl_surface_commit(window->surface);
        //wl_display_flush(waylandState.display);
        //wl_display_dispatch_pending(waylandState.display);
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

// Unfinished.
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
{}

// Unfinished.
void drwPFArcChord
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle,
  intType col
)
{}

// Unfinished.
void drwPFArcPieSlice
( const_winType actual_window,
  intType x,
  intType y,
  intType radius,
  floatType startAngle,
  floatType sweepAngle,
  intType col
)
{}

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

// Unfinished.
void drwPFEllipse
( const_winType actual_window,
  intType x,
  intType y,
  intType width,
  intType height,
  intType col
)
{
  puts("drwPFEllipse called.");
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
    { /* Utilizing the slope-intercept formula:
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
      else // Diagonal line.
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
      }

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
void drwPoint (const_winType actual_window, intType x, intType y)
{}

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

// Unfinished.
void drwPolyLine (const_winType actual_window, intType x, intType y, bstriType point_list, intType col)
{
  puts("DrwPolyLine called.");
}

// Unfinished.
void drwPPoint (const_winType actual_window, intType x, intType y, intType col)
{}

// Unfinished.
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
    //printf("DrwPRect colour: %ld, x: %ld, y: %ld, w: %ld, h: %ld\n", col, x, y, width, height);

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

    if (prepare_buffer_copy(&waylandState, destination) && source->buffer && source->buffer->content)
    { // Copy the data to the destination.
      for (int y = 0, loop = 1; y < source->buffer->height && loop; y++)
        for (int x = 0; x < source->buffer->width; x++)
        { int pos = y * source->buffer->width + x;
          int dx = xDest + x,
              dy = yDest + y,
              dpos = dy * destination->buffer->width + dx;


          if (dx < destination->width && dy < destination->height)
            destination->buffer->content[dpos] = source->buffer->content[pos];
          else
          { loop = 0;
            break;
          }
        }

      // If the destination is a Wayland window, send the data.
      if (!destination->isPixmap)
      { wl_buffer_add_listener(destination->buffer->waylandData, &waylandBufferListener, NULL); // Add listener to destroy buffer.
        wl_surface_attach(destination->surface, destination->buffer->waylandData, 0, 0);
        wl_surface_damage_buffer(destination->surface, xDest, yDest, source->buffer->width, source->buffer->height); // Damage the affected area (as only that section needs updating).
        wl_surface_commit(destination->surface);

        wl_display_dispatch(waylandState.display);
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

/* Unfinished.
void drwRect (const_winType actual_window, intType x, intType y, intType width, intType height)
{}*/

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

// Unfinished.
void drwSetContent (const_winType actual_window, const_winType pixmap)
{}

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

// Note: only deals with the primary window.
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
  return ((way_winType) actual_window)->height;
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