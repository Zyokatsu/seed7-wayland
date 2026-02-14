#include "buffer.h"
#include "shared_memory.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
// Copied from ../rtl_err.h
#define raise_error(num) raise_error2(num, __FILE__, __LINE__)
void raise_error2 (int exception_num, const_cstriType fileName, int line);

/*- Wayland Buffers and Drawing --------
--------------------------------------*/
void wayland_buffer_release (void *data, struct wl_buffer *wl_buffer)
{
  // Sent by the compositor when it's no longer using this buffer.
  wl_buffer_destroy(wl_buffer);
}

const struct wl_buffer_listener waylandBufferListener =
{ .release = wayland_buffer_release };

/*struct wl_buffer *create_wayland_buffer (struct ClientState *state, int width, int height)
{
  struct BufferData buffer = {0};
  buffer.width = width;
  buffer.height = height;

  const int PixelBytes = 4; // WL_SHM_FORMAT_[X/A]RGB8888 requires 4 bytes per pixel.
  int stride = buffer.width * PixelBytes;

  buffer.contentSize = buffer.width * buffer.height * PixelBytes;
  int fd = allocate_shm_file(buffer.contentSize);

  if (fd != -1)
  { buffer.content = mmap(NULL, buffer.contentSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (buffer.content != MAP_FAILED)
    { struct wl_shm_pool *pool = wl_shm_create_pool(state->sharedMemory, fd, buffer.contentSize);
      buffer.waylandData = wl_shm_pool_create_buffer(pool, 0, buffer.width, buffer.height, stride, AlphaEnabled ? WL_SHM_FORMAT_ARGB8888 : WL_SHM_FORMAT_XRGB8888);
      wl_shm_pool_destroy(pool);
    }

    close(fd);
  }

  return buffer.waylandData;
}*/

boolType prepare_buffer_data (struct ClientState *state, way_winType window)
{
  struct BufferData *buffer = NULL;
  const int PixelBytes = 4; // WL_SHM_FORMAT_[X/A]RGB8888 requires 4 bytes per pixel.
  boolType resizeNeeded = FALSE;

  // Ensure state, when needed.
  if (!window->isPixmap && !state)
    return FALSE;

  // Prepare a new buffer object (if needed).
  if (!window->isPixmap || !window->buffer)
  { buffer = malloc(sizeof(*buffer));
    window->buffer = buffer;
    window->buffer->waylandData = NULL;
    resizeNeeded = TRUE;

    if (!buffer)
      return FALSE;
  }

  if
  ( resizeNeeded ||
    !window->isPixmap ||
    window->buffer->width != window->width ||
    window->buffer->height != window->height
  )
  { resizeNeeded = TRUE;
    window->buffer->width = window->width;
    window->buffer->height = window->height;
    window->buffer->contentSize = window->buffer->width * window->buffer->height * PixelBytes;
  }

  // Pixmap's don't need as much fiddling.
  if (window->isPixmap)
  { if (resizeNeeded)
    { window->buffer->content = malloc(window->buffer->contentSize);

      if (!window->buffer->content)
      { free(window->buffer);
        window->buffer = NULL;
        buffer = NULL;
        raise_error(MEMORY_ERROR);
        return FALSE;
      }
    }
  }
  // Actual windows require a wl_buffer.
  else
  { int stride = window->buffer->width * PixelBytes;
    int fd = allocate_shm_file(window->buffer->contentSize);

    if (fd != -1)
    { window->buffer->content = mmap(NULL, window->buffer->contentSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

      if (window->buffer->content != MAP_FAILED)
      { struct wl_shm_pool *pool = wl_shm_create_pool(state->sharedMemory, fd, buffer->contentSize);
#ifdef USE_ALPHA
        buffer->waylandData = wl_shm_pool_create_buffer(pool, 0, buffer->width, buffer->height, stride, WL_SHM_FORMAT_ARGB8888);
#else
        buffer->waylandData = wl_shm_pool_create_buffer(pool, 0, buffer->width, buffer->height, stride, WL_SHM_FORMAT_XRGB8888);
#endif
        wl_shm_pool_destroy(pool);
      }

      close(fd);
    }


    if (!buffer->waylandData)
    { free(buffer);
      buffer = NULL;
      window->buffer = NULL;
      raise_error(MEMORY_ERROR);
      return FALSE;
    }
  }

  return TRUE;
}

/* Creates a new buffer while mimicing the content of the old one.
(Should research to see if creating a new buffer is really necessary.) */
boolType prepare_buffer_copy (struct ClientState *state, way_winType window)
{
  uint32_t *oldContent = window->buffer ? window->buffer->content : 0;

  if (prepare_buffer_data(state, window))
  { // Copy over the old data.
    if (oldContent)
      for (int y = 0; y < window->buffer->height; y++)
        for (int x = 0; x < window->buffer->width; x++)
        { int pos = y * window->buffer->width + x;
          window->buffer->content[pos] = oldContent[pos];
        }
    return TRUE;
  }

  return FALSE;
}