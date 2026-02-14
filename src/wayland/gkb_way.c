/********************************************************************/
/*                                                                  */
/*  gkb_way.c     Keyboard and mouse access for Wayland.            */
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
/*  File: seed7/src/wayland/gkb_way.c                               */
/*  Content: Keyboard and mouse access for Wayland.                 */
/*                                                                  */
/********************************************************************/
// To do: gkbWindow needs to be implemented, and lock-on key states are not recorded/reported.

#include "../version.h"

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <X11/keysym.h>
#include <linux/input-event-codes.h>

#include "../common.h"
#include "../data_rtl.h"
#include "../os_decls.h"
#include "../hsh_rtl.h"
#include "../rtl_err.h"
#include "keyboard_globals.h"
#include "state.h"

#define KeyBuffer 10

extern struct ClientState waylandState;
extern void setResizeReturnsKey (winType resizeWindow, boolType active);
extern void drwSetCloseAction (winType actual_window, intType closeAction);

// Event poll to check for keyboard events.
bool poll_events (bool block)
{
  bool result = FALSE;

  if (waylandState.display)
  { struct pollfd fds[1];
    fds[0].fd = wl_display_get_fd(waylandState.display);
    fds[0].events = POLLIN;

    result = poll(fds, 1, block ? -1 : 0) > 0;

    if (result)
    { wl_display_flush(waylandState.display);
      wl_display_dispatch(waylandState.display);
      // result = waylandState.lastKey;
    }
  }

  return result;
}

bool key_history_populated ()
{
  return waylandState.keyHistory && waylandState.keyHistory->keys &&
    waylandState.keyHistory->keys->use > 0;
}

void wait_for_key ()
{
  while (!key_history_populated())
    poll_events(TRUE);
}

// Translate special key-codes from X11 to Seed7.
uint32_t translate_key (uint32_t key)
{
  switch (key)
  { case XK_Alt_L: return K_LEFT_ALT;
    case XK_Alt_R: return K_RIGHT_ALT;
    case XK_BackSpace: return K_BS;
    case XK_Control_L: return K_LEFT_CONTROL;
    case XK_Control_R: return K_RIGHT_CONTROL;
    case XK_Escape: return K_ESC;
    case XK_Return:
    case XK_Linefeed:
      return K_NL;
    case XK_Shift_L: return K_LEFT_SHIFT;
    case XK_Shift_R: return K_RIGHT_SHIFT;
    case XK_Super_L: return K_LEFT_SUPER;
    case XK_Super_R: return K_RIGHT_SUPER;
    case XK_Tab: return K_TAB;

    case XK_F1: return K_F1;
    case XK_F2: return K_F2;
    case XK_F3: return K_F3;
    case XK_F4: return K_F4;
    case XK_F5: return K_F5;
    case XK_F6: return K_F6;
    case XK_F7: return K_F7;
    case XK_F8: return K_F8;
    case XK_F9: return K_F9;
    case XK_F10: return K_F10;
    case XK_F11: return K_F11;
    case XK_F12: return K_F12;

    case XK_Left: return K_LEFT;
    case XK_Right: return K_RIGHT;
    case XK_Up: return K_UP;
    case XK_Down: return K_DOWN;
    case XK_Home: return K_HOME;
    case XK_End: return K_END;
    case XK_Prior: return K_PGUP;
    case XK_Next: return K_PGDN;
    case XK_Insert: return K_INS;
    case XK_Delete: return K_DEL;
    case XK_Menu: return K_MENU;
    case XK_Print: return K_PRINT;
    case XK_Pause: return K_PAUSE;

    case XK_KP_Left: return K_LEFT;
    case XK_KP_Right: return K_RIGHT;
    case XK_KP_Up: return K_UP;
    case XK_KP_Down: return K_DOWN;
    case XK_KP_Home: return K_HOME;
    case XK_KP_End: return K_END;
    case XK_KP_Prior: return K_PGUP;
    case XK_KP_Next: return K_PGDN;
    case XK_KP_Insert: return K_INS;
    case XK_KP_Delete: return K_DEL;
    case XK_KP_Begin: return K_PAD_CENTER;
    case XK_KP_4: return K_LEFT;
    case XK_KP_6: return K_RIGHT;
    case XK_KP_8: return K_UP;
    case XK_KP_2: return K_DOWN;
    case XK_KP_7: return K_HOME;
    case XK_KP_1: return K_END;
    case XK_KP_9: return K_PGUP;
    case XK_KP_3: return K_PGDN;
    case XK_KP_0: return K_INS;
    case XK_KP_5: return K_PAD_CENTER;
    case XK_KP_Enter: return K_NL;
    case XK_KP_Decimal: return K_DEL;

    default: return key;
  }
}

uint32_t translate_mouse_button (uint32_t button)
{
  // Note: Seed7 seems to use #4 for scroll-wheel forward and #5 for scroll-wheel backward.
  switch (button)
  { case BTN_LEFT: return K_MOUSE1;
    case BTN_MIDDLE: return K_MOUSE2;
    case BTN_RIGHT: return K_MOUSE3;
    case BTN_SIDE: return K_MOUSE_BACK;
    case BTN_EXTRA: return K_MOUSE_FWD;
    case BTN_FORWARD: return K_MOUSE4;
    case BTN_BACK: return K_MOUSE5;
    default: return K_UNDEF;
  }
}

// Populates the keys-pressed array.
void add_pressed_key (struct ClientState *state, uint32_t key)
{
  if (!state->keysPressed)
  { // Allocating a buffer to reduce the number of times we must use malloc.
    struct KeyArray *newPressed = malloc(sizeof *state->keysPressed + sizeof(uint32_t) * KeyBuffer);

    if (newPressed)
    { newPressed->content[0] = key;
      newPressed->size = KeyBuffer;
      newPressed->use = 1;
      state->keysPressed = newPressed;
    }
    else
      raise_error(MEMORY_ERROR);
  }
  else
  if (state->keysPressed->use >= state->keysPressed->size)
  { state->keysPressed->use = state->keysPressed->size;
    // It's unlikely we'll go above the buffer size for pressed keys, so only allocating 1 more when needed should be fine.
    struct KeyArray *newPressed = malloc(sizeof *state->keysPressed + sizeof *state->keysPressed->content * (state->keysPressed->size+1));

    if (newPressed)
    { for (int unsigned x = 0; x < state->keysPressed->use; x++)
        newPressed->content[x] = state->keysPressed->content[x];
      newPressed->content[state->keysPressed->use] = key;
      newPressed->size = state->keysPressed->size + 1;
      newPressed->use = state->keysPressed->use + 1;
      free(state->keysPressed);
      state->keysPressed = newPressed;
    }
    else
      raise_error(MEMORY_ERROR);
  }
  else
  { state->keysPressed->content[state->keysPressed->use] = key;
    state->keysPressed->use++;
  }

  /*for (int unsigned x = 0; x < state->keysPressed->use; x++)
    printf("Now pressed: %c\n", state->keysPressed->content[x]);
  puts("");*/
}

void remove_pressed_key (struct ClientState *state, uint32_t key)
{
  if (state->keysPressed && state->keysPressed->use > 0)
  { // Search for the key in the pressed array.
    for (int unsigned x = 0; x < state->keysPressed->use; x++)
      if (state->keysPressed->content[x] == key)
      { // Shrink back down to the buffer.
        if (state->keysPressed->use > KeyBuffer)
        { struct KeyArray *newPressed = 0;
          newPressed = malloc(sizeof *state->keysPressed + sizeof *state->keysPressed->content * (state->keysPressed->size-1));

          if (newPressed)
          { for (int unsigned y = 0; y < state->keysPressed->use - 1; y++)
              newPressed->content[y] = state->keysPressed->content[y + (y >= x ? 1 : 0)];
            newPressed->size = state->keysPressed->size-1;
            newPressed->use = state->keysPressed->use-1;
            free(state->keysPressed);
            state->keysPressed = newPressed;
          }
          else
            raise_error(MEMORY_ERROR);
        }
        else // Or continue to use the buffer.
        { for (int unsigned y = x; y < state->keysPressed->use - 1; y++)
            state->keysPressed->content[y] = state->keysPressed->content[y+1];
          state->keysPressed->use--;
        }
        break;
      }

    /*for (int unsigned x = 0; x < state->keysPressed->use; x++)
      printf("Still pressed: %c\n", state->keysPressed->content[x]);
    puts("");*/
  }
}

void expand_key_history (struct ClientState *state, uint32_t key)
{
  if (state)
  { if (!state->keyHistory)
    { struct KeyHistory *newHistory = malloc(sizeof *state->keyHistory);

      if (newHistory)
      { state->keyHistory = newHistory;
        state->keyHistory->age = 0;
        state->keyHistory->keys = 0;
      }
      else
      { raise_error(MEMORY_ERROR);
        return;
      }
    }

    const int unsigned KeyLimit = 200; // We don't want to record keys for all time, so set a limit.
    const int unsigned KeySpan = (!state->keyHistory->keys || state->keyHistory->keys->use + 1 < KeyLimit)
      ? (!state->keyHistory->keys ? KeyBuffer : state->keyHistory->keys->use + 5) // Allocate multiple at a time.
      : KeyLimit;

    if (KeySpan > (state->keyHistory->keys ? state->keyHistory->keys->size : 0))
    { struct KeyArray *newKeys = malloc(sizeof *state->keyHistory->keys + sizeof(uint32_t) * KeySpan);

      if (newKeys)
      { const int unsigned oldUse = state->keyHistory->keys ? state->keyHistory->keys->use: 0;
        newKeys->size = KeySpan;
        newKeys->use = oldUse + 1;

        if (state->keyHistory->keys)
          for (int unsigned x = 0; x < state->keyHistory->keys->use; x++)
            newKeys->content[x] = state->keyHistory->keys->content[x];

        newKeys->content[oldUse] = key;

        free(state->keyHistory->keys);
        state->keyHistory->keys = newKeys;
      }
      else
        raise_error(MEMORY_ERROR);
    }
    else
    if (KeySpan == KeyLimit)
    { // Shift the keys back one (dropping the oldest key).
      if (state->keyHistory->keys)
        for (int unsigned x = 0; x < state->keyHistory->keys->size - 1; x++)
          state->keyHistory->keys->content[x] = state->keyHistory->keys->content[x+1];

      state->keyHistory->keys->content[state->keyHistory->keys->size-1] = key;
    }
    else
    { state->keyHistory->keys->content[state->keyHistory->keys->use] = key;
      state->keyHistory->keys->use++;
    }
  }
}

// Used for buttons and keys, the key parameter should be the Seed7 value (post-translation).
void alter_switch_state (struct ClientState *state, uint32_t key, bool pressed)
{
  /*printf
  ( "Alter key state called.\n"
    "  state: %p\n"
    "  key: %u\n"
    "  pressed: %d\n"
    "  initialHistory: %p\n",
    state, key, pressed, state->keyHistory
  );*/
  if (state)
  { if (pressed)
    { add_pressed_key(state, key);
      expand_key_history(state, key);
    }
    else
      remove_pressed_key(state, key);
  }
}

boolType alt_pressed (struct ClientState *state)
{
  if (state->keysPressed && state->keysPressed->use > 0)
  { for (int unsigned x = 0; x < state->keysPressed->use; x++)
      if
      ( state->keysPressed->content[x] == K_ALT ||
        state->keysPressed->content[x] == K_LEFT_ALT ||
        state->keysPressed->content[x] == K_RIGHT_ALT
      )
        return TRUE;
  }
  return FALSE;
}

boolType control_pressed (struct ClientState *state)
{
  if (state->keysPressed && state->keysPressed->use > 0)
  { for (int unsigned x = 0; x < state->keysPressed->use; x++)
      if
      ( state->keysPressed->content[x] == K_CONTROL ||
        state->keysPressed->content[x] == K_LEFT_CONTROL ||
        state->keysPressed->content[x] == K_RIGHT_CONTROL
      )
        return TRUE;
  }
  return FALSE;
}

boolType shift_pressed (struct ClientState *state)
{
  if (state->keysPressed && state->keysPressed->use > 0)
  { for (int unsigned x = 0; x < state->keysPressed->use; x++)
      if
      ( state->keysPressed->content[x] == K_SHIFT ||
        state->keysPressed->content[x] == K_LEFT_SHIFT ||
        state->keysPressed->content[x] == K_RIGHT_SHIFT
      )
        return TRUE;
  }
  return FALSE;
}

// Called by the Wayland key event hook (for presses and releases).
void alter_key_state (struct ClientState *state, uint32_t key, bool pressed)
{
  alter_switch_state(state, translate_key(key), pressed);
}

void alter_mouse_button_state (struct ClientState *state, uint32_t button, bool pressed)
{
  alter_switch_state(state, translate_mouse_button(button), pressed);
}

void record_mouse_movement (struct ClientState *state, int x, int y)
{
  state->mousePoint.x = x;
  state->mousePoint.y = y;
}

void trigger_mouse_scroll (struct ClientState *state, bool forward)
{
  expand_key_history(state, forward ? K_MOUSE4 : K_MOUSE5);
}

void gkbInitKeyboard (void)
{
  logFunction(printf("gkbInitKeyboard()\n"););
  waylandState.mousePoint.x = 0;
  waylandState.mousePoint.y = 0;
}

// Unfinished: doesn't yet handle shift/num/scroll lock on.
boolType gkbButtonPressed (charType button)
{
  charType target = button;
  int shift = 0, control = 0, alt = 0;
  boolType targetFound = FALSE;

  // Shift types.
  if (target == K_SHIFT)
  { targetFound = TRUE;
    shift = -1;
  }
  if (target >= K_SFT_F1 && target <= K_SFT_F12)
  { target = K_F1 + target - K_SFT_F1;
    shift = -1;
  }
  else
  if (target >= K_SFT_LEFT && target <= K_SFT_PAD_CENTER)
  { target = K_LEFT + target - K_SFT_LEFT;
    shift = -1;
  }
  else
  if (target >= K_SFT_MOUSE1 && target <= K_SFT_MOUSE_BACK)
  { target = K_MOUSE1 + target - K_SFT_MOUSE1;
    shift = -1;
  }
  else // Control types.
  if (target == K_CONTROL)
  { targetFound = TRUE;
    control = -1;
  }
  else
  if (target >= K_CTL_A && target <= K_CTL_Z)
  { target = 'a' + target - K_CTL_A;
    control = -1;
  }
  else
  if (target >= K_CTL_0 && target <= K_CTL_9)
  { target = '0' + target - K_CTL_0;
    control = -1;
  }
  else
  if (target >= K_CTL_F1 && target <= K_CTL_F12)
  { target = K_F1 + target - K_CTL_F1;
    control = -1;
  }
  else
  if (target >= K_CTL_LEFT && target <= K_CTL_PAD_CENTER)
  { target = K_LEFT + target - K_CTL_LEFT;
    control = -1;
  }
  else
  if (target >= K_CTL_MOUSE1 && target <= K_CTL_MOUSE_BACK)
  { target = K_MOUSE1 + target - K_CTL_MOUSE1;
    control = -1;
  }
  else // Alt types.
  if (target == K_ALT)
  { targetFound = TRUE;
    alt = -1;
  }
  else
  if (target >= K_ALT_A && target <= K_ALT_Z)
  { target = 'a' + target - K_ALT_A;
    alt = -1;
  }
  else
  if (target >= K_ALT_0 && target <= K_ALT_9)
  { target = '0' + target - K_ALT_0;
    alt = -1;
  }
  else
  if (target >= K_ALT_F1 && target <= K_ALT_F12)
  { target = K_F1 + target - K_ALT_F1;
    alt = -1;
  }
  else
  if (target >= K_ALT_LEFT && target <= K_ALT_PAD_CENTER)
  { target = K_LEFT + target - K_ALT_LEFT;
    alt = -1;
  }
  else
  if (target >= K_ALT_MOUSE1 && target <= K_ALT_MOUSE_BACK)
  { target = K_MOUSE1 + target - K_ALT_MOUSE1;
    alt = -1;
  }
  else
    switch (target)
    { case K_SFT_NL: target = K_NL; shift = -1; break;
      case K_SFT_BS: target = K_BS; shift = -1; break;
      case K_SFT_TAB: target = K_TAB; shift = -1; break;
      case K_SFT_ESC: target = K_ESC; shift = -1; break;
      case K_SFT_MENU: target = K_MENU; shift = -1; break;
      case K_SFT_PRINT: target = K_PRINT; shift = -1; break;
      case K_SFT_PAUSE: target = K_PAUSE; shift = -1; break;
      case K_CTL_NL: target = K_NL; control = -1; break;
      case K_CTL_BS: target = K_BS; control = -1; break;
      case K_CTL_TAB: target = K_TAB; control = -1; break;
      case K_CTL_ESC: target = K_ESC; control = -1; break;
      case K_CTL_MENU: target = K_MENU; control = -1; break;
      case K_CTL_PRINT: target = K_PRINT; control = -1; break;
      case K_CTL_PAUSE: target = K_PAUSE; control = -1; break;
      case K_ALT_NL: target = K_NL; alt = -1; break;
      case K_ALT_BS: target = K_BS; alt = -1; break;
      case K_ALT_TAB: target = K_TAB; alt = -1; break;
      case K_ALT_ESC: target = K_ESC; alt = -1; break;
      case K_ALT_MENU: target = K_MENU; alt = -1; break;
      case K_ALT_PRINT: target = K_PRINT; alt = -1; break;
      case K_ALT_PAUSE: target = K_PAUSE; alt = -1; break;
    }

  if (waylandState.keysPressed && waylandState.keysPressed->use > 0)
  { uint32_t *key = waylandState.keysPressed->content;
    for (int unsigned x = 0; x < waylandState.keysPressed->use; x++, key++)
    { // These three keys share the same codes as CTL_H, CTL_I, and CTL_J (which would have been converted above).
      if (*key == button && (button == K_BS || button == K_TAB || button == K_NL))
        return true;
      else
      if (*key == target)
        targetFound = TRUE;
      else
      { switch (*key)
        { case K_LEFT_SHIFT:
          case K_RIGHT_SHIFT:
            shift = 1;
            break;
          case K_LEFT_CONTROL:
          case K_RIGHT_CONTROL:
            control = 1;
            break;
          case K_LEFT_ALT:
          case K_RIGHT_ALT:
            alt = 1;
            break;
          default:
            if ((control || alt) && target >= 'a' && target <= 'z' && *key >= 'A' && *key <= 'Z' && *key - 'A' == target - 'a')
              targetFound = TRUE;
        }
      }
      if (targetFound && shift >= 0 && control >= 0 && alt >= 0)
        return TRUE;
    }
  }

  return FALSE;
}

intType gkbClickedXpos (void)
{
  return (intType) waylandState.mousePoint.x;
}

intType gkbClickedYpos (void)
{
  return (intType) waylandState.mousePoint.y;
}

// Reads a character as would be seen when typed into a terminal.
charType gkbGetc (void)
{
  charType result = K_NONE;
  boolType getNextChar = FALSE;
  int unsigned newSize;

  do
  { if (!key_history_populated())
      wait_for_key();

    result = waylandState.keyHistory->keys->content[0];
    // Skip over control characters.
    if (result >= K_SHIFT && result <= K_SCROLL_LOCK)
      getNextChar = TRUE;
    else
    { getNextChar = FALSE;
      // Special Shift/Control/Alt combos.
      if (shift_pressed(&waylandState))
      { if (result >= K_F1 && result <= K_F12)
          result = K_ALT_F1 + result - K_F1;
        else
        if (result >= K_LEFT && result <= K_PAD_CENTER)
          result = K_ALT_LEFT + result - K_LEFT;
        else
          switch (result)
          { case K_NL: result = K_SFT_NL; break;
            case K_BS: result = K_SFT_BS; break;
            case K_TAB: result = K_SFT_TAB; break;
            case K_ESC: result = K_ALT_ESC; break;
            case K_MENU: result = K_ALT_MENU; break;
            case K_PRINT: result = K_ALT_PRINT; break;
            case K_PAUSE: result = K_ALT_PAUSE; break;
          }
      }
      else
      if (control_pressed(&waylandState))
      { if (result >= 'a' && result <= 'z')
          result = K_CTL_A + result - 'a';
        else
        if (result >= 'A' && result <= 'Z')
          result = K_CTL_A + result - 'A';
        else
        if (result >= '0' && result <= '9')
          result = K_CTL_0 + result - '0';
        else
        if (result >= K_F1 && result <= K_F12)
          result = K_CTL_F1 + result - K_F1;
        else
        if (result >= K_LEFT && result <= K_PAD_CENTER)
          result = K_CTL_LEFT + result - K_LEFT;
        else
          switch (result)
          { case K_NL: result = K_CTL_NL; break;
            case K_BS: result = K_CTL_BS; break;
            case K_TAB: result = K_CTL_TAB; break;
            case K_ESC: result = K_CTL_ESC; break;
            case K_MENU: result = K_CTL_MENU; break;
            case K_PRINT: result = K_CTL_PRINT; break;
            case K_PAUSE: result = K_CTL_PAUSE; break;
          }
      }
      else
      if (alt_pressed(&waylandState))
      { if (result >= 'a' && result <= 'z')
          result = K_ALT_A + result - 'a';
        else
        if (result >= 'A' && result <= 'Z')
          result = K_ALT_A + result - 'A';
        else
        if (result >= '0' && result <= '9')
          result = K_ALT_0 + result - '0';
        else
        if (result >= K_F1 && result <= K_F12)
          result = K_ALT_F1 + result - K_F1;
        else
        if (result >= K_LEFT && result <= K_PAD_CENTER)
          result = K_ALT_LEFT + result - K_LEFT;
        else
          switch (result)
          { case K_NL: result = K_ALT_NL; break;
            case K_BS: result = K_ALT_BS; break;
            case K_TAB: result = K_ALT_TAB; break;
            case K_ESC: result = K_ALT_ESC; break;
            case K_MENU: result = K_ALT_MENU; break;
            case K_PRINT: result = K_ALT_PRINT; break;
            case K_PAUSE: result = K_ALT_PAUSE; break;
          }
      }
    }

    // Remove the key from the history (reallocating at buffer size or every 10 characters beyond it).
    if
    ( waylandState.keyHistory->keys->use == KeyBuffer + 1 ||
      waylandState.keyHistory->keys->use > KeyBuffer && (waylandState.keyHistory->keys->use-KeyBuffer) % 10 == 0
    )
    { newSize = (waylandState.keyHistory->keys->use == KeyBuffer + 1) ? KeyBuffer : waylandState.keyHistory->keys->use;
      struct KeyArray *newKeys = malloc(sizeof *waylandState.keyHistory->keys + sizeof *waylandState.keyHistory->keys->content * newSize);

      if (newKeys)
      { newKeys->size = newSize;
        newKeys->use = (newSize == waylandState.keyHistory->keys->use) ? newSize-1 : newSize;

        for (int unsigned x = 0; x < newKeys->use; x++)
          newKeys->content[x] = waylandState.keyHistory->keys->content[x+1];

        free(waylandState.keyHistory->keys);
        waylandState.keyHistory->keys = newKeys;
      }
      else
      { // puts("Failed to allocate key history's reduced key block.");
        raise_error(MEMORY_ERROR);
      }
    }
    else
    { for (int unsigned x = 0; x < waylandState.keyHistory->keys->use - 1; x++)
        waylandState.keyHistory->keys->content[x] = waylandState.keyHistory->keys->content[x+1];
      waylandState.keyHistory->keys->use--;
    }
  } while (getNextChar);

  return result;
}

charType gkbRawGetc (void)
{
  return gkbGetc();
}

boolType gkbInputReady (void)
{
  return poll_events(FALSE) && waylandState.keyHistory && waylandState.keyHistory->keys &&
    waylandState.keyHistory->keys->use > 0;
}

void gkbSelectInput (winType aWindow, charType aKey, boolType active)
{
  if (aKey == K_RESIZE)
    setResizeReturnsKey(aWindow, active);
  else
  if (aKey == K_CLOSE)
  { if (active)
      drwSetCloseAction(aWindow, CLOSE_BUTTON_RETURNS_KEY);
    else
      drwSetCloseAction(aWindow, CLOSE_BUTTON_CLOSES_PROGRAM);
  }
  else
    raise_error(RANGE_ERROR);
}

// Unfinished.
winType gkbWindow (void)
{
  winType result = {0};
  return result;
}