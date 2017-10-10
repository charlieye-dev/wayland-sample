/*
 * Wayland subsurface example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>

struct wl_compositor *compositor = NULL;
struct wl_subcompositor *subcompositor = NULL;
struct wl_shell *shell;
struct wl_shm *shm;
struct wl_buffer *buffer;

static int running = 1;
GLubyte image[64][64][4];
void *shm_data;

int WIDTH = 320;
int HEIGHT = 320;
int i = 0;

struct display {
  struct wl_display *display;
  struct wl_registry *registry;
  EGLDisplay egl_display;
  EGLContext egl_context;
};

struct window {
  EGLSurface egl_surface;
  struct wl_surface *main_surface;
  struct wl_surface *sub_surface;
  struct wl_subsurface *subsurface;
  struct wl_shell_surface *shell_surface;
  struct wl_egl_window *egl_window;
  struct display *display;
};

void paint_pixels(uint32_t *pixel) {
  int x, y;

  for (y = 0; y < HEIGHT; y++) {
    for (x = 0; x < WIDTH; x++) {
      int mx = x / 1;
      int my = y / 1;
      uint32_t color = 0;
      if (mx % 2 == 0 && my % 2 == 0) {
        uint32_t code = (mx / 2) % 8; // X axis determines a color code from 0 to 7.
        uint32_t red = code & 1 ? 0xff0000 : 0;
        uint32_t green = code & 2 ? 0x00ff00 : 0;
        uint32_t blue = code & 4 ? 0x0000ff : 0;
        uint32_t alpha = (my / 2) % 8 * 32 << 24; // Y axis determines alpha value from 0 to 0xf0
        color = alpha + red + green + blue;
      }
      pixel[x + (y * WIDTH)] = color;
    }
  }
}

/*
* Return a newly created and resized file descriptor.
* For real use case, mkstemp instead of open is better.
* See create_tmpfile_cloexec in https://cgit.freedesktop.org/wayland/weston/tree/shared/os-compatibility.c
*/
int create_shared_fd(off_t size)
{
  char name[1024] = "";

  const char *path = getenv("XDG_RUNTIME_DIR");
  if (!path) {
    fprintf(stderr, "Please define environment variable XDG_RUNTIME_DIR\n");
    exit(1);
  }

  strcpy(name, path);
  strcat(name, "/shm-test");

  int fd = open(name, O_RDWR | O_EXCL | O_CREAT);
  if (fd < 0) {
    fprintf(stderr, "File cannot open: %s\n", name);
    exit(1);
  } else {
    unlink(name);
  }

  if (ftruncate(fd, size) < 0) {
    fprintf(stderr, "ftruncate failed: fd=%i, size=%li\n", fd, size);
    close(fd);
    exit(1);
  }

  return fd;
}

/*
 * Create a window and return the attached buffer
 */
void create_shm_buffer(struct wl_surface *surface) {
  int stride = WIDTH * 4; // 4 bytes per pixel
  int size = stride * HEIGHT;

  int fd = create_shared_fd(size);
  shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_data == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %m\n");
    close(fd);
    exit(1);
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
  buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);

  return;
}

void shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
}

struct wl_shm_listener shm_listener = {
  shm_format
};

/*
 * Event listeners for wl_registry_add_listener
 */
void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
  if (strcmp(interface, "wl_compositor") == 0) {
    compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
  } else if (strcmp(interface, "wl_shell") == 0) {
    shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
  } else if (strcmp(interface, "wl_shm") == 0) {
    shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    wl_shm_add_listener(shm, &shm_listener, NULL);
  } else if (!strcmp(interface,"wl_subcompositor")) {
    subcompositor = wl_registry_bind (registry, id, &wl_subcompositor_interface, 1);
  }

  printf("%s\n", interface);
}

void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
}

const struct wl_registry_listener registry_listener = {
  global_registry_handler,
  global_registry_remover
};

/*
 * Event listeners for wl_shell_surface_add_listener
 */
void handle_ping(void *_data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
  wl_shell_surface_pong(shell_surface, serial);
}

void handle_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
}

void handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

const struct wl_shell_surface_listener shell_surface_listener = {
  handle_ping,
  handle_configure,
  handle_popup_done
};

void create_shell_surface(struct window *window) {
  window->shell_surface = wl_shell_get_shell_surface(shell, window->main_surface);
  if (window->shell_surface == NULL) {
    fprintf(stderr, "Can't create shell surface\n");
    exit(1);
  }

  wl_shell_surface_set_toplevel(window->shell_surface);
  wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener, NULL);
}

void draw_main_surface(struct window *window) {
  create_shm_buffer(window->main_surface);
  paint_pixels(shm_data);

  wl_surface_attach(window->main_surface, buffer, 0, 0);
  wl_surface_damage(window->main_surface, 0, 0, WIDTH, HEIGHT);
  wl_surface_commit(window->main_surface);
}

void create_main_surface(struct window *window) {
  window->main_surface = wl_compositor_create_surface(compositor);
  if (window->main_surface == NULL) {
    fprintf(stderr, "Can't create sub surface\n");
    exit(1);
  }

  create_shell_surface(window);

}

void create_sub_surface(struct window *window) {
  window->display->egl_display = eglGetDisplay (window->display->display);
  eglInitialize (window->display->egl_display, NULL, NULL);

  EGLConfig config;
  EGLint num_config;
  EGLint attributes[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_NONE
  };

  eglBindAPI (EGL_OPENGL_API);
  eglChooseConfig (window->display->egl_display, attributes, &config, 1, &num_config);
  window->display->egl_context = eglCreateContext (window->display->egl_display, config, EGL_NO_CONTEXT, NULL);

  window->sub_surface = wl_compositor_create_surface(compositor);

  window->egl_window = wl_egl_window_create (window->sub_surface, 160, 160);
  window->egl_surface = eglCreateWindowSurface (window->display->egl_display, config, window->egl_window, NULL);
  eglMakeCurrent (window->display->egl_display, window->egl_surface, window->egl_surface, window->display->egl_context);
}

void impl_subsurface(struct window *window) {
  /*attach to parent surface.*/
  window->subsurface = wl_subcompositor_get_subsurface(subcompositor, window->sub_surface, window->main_surface);

  /*set subsurface position.*/
  wl_subsurface_set_position(window->subsurface, 160, 160);

  /*set subsurface place.*/
//  wl_subsurface_place_below(window->subsurface, window->main_surface);
  wl_subsurface_place_above(window->subsurface, window->main_surface);
}

static void create_texture() {
  GLuint texture;
  int i, j;

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  for(i = 0; i < 64; i++) {
    for(j = 0; j < 64; j++) {
      image[i][j][0] = (GLubyte) 255;
      image[i][j][1] = 0;
      image[i][j][2] = 0;
      image[i][j][3] = 0;
    }
  }

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

static void draw_sub_surface (struct window *window) {
  glViewport(0, 0, 160, 160);

  glClearColor (0.5, 0.5, 0.5, 0.5);
  glClear (GL_COLOR_BUFFER_BIT);

  static const GLfloat texcoord[4][2] = {
    { 0, 0 },
    { 1, 0 },
    { 0, 1 },
    { 1, 1 }
  };
  static const GLfloat vertex[4][2] = {
    { -1.0, -1.0 },
    {  1.0, -1.0 },
    { -1.0,  1.0 },
    {  1.0,  1.0 }
  };

  glVertexPointer(2, GL_FLOAT, 0, vertex);
  glTexCoordPointer(2, GL_FLOAT, 0, texcoord);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  eglSwapBuffers (window->display->egl_display, window->egl_surface);
}

static void signal_int(int signum)
{
  running = 0;
}

int main(int argc, char **argv) {
  struct sigaction sigint;
  struct display display;
  struct window window;

  sigint.sa_handler = signal_int;
  sigemptyset(&sigint.sa_mask);
  sigint.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sigint, NULL);

  display.display = wl_display_connect(NULL);
  if (display.display == NULL) {
    fprintf(stderr, "Can't connect to display\n");
    exit(1);
  }

  display.registry = wl_display_get_registry(display.display);
  wl_registry_add_listener(display.registry, &registry_listener, NULL);

  wl_display_dispatch(display.display);
  wl_display_roundtrip(display.display);

  if(compositor == NULL) {
    fprintf(stderr, "Can't find compositor\n");
    exit(1);
  } else if (subcompositor == NULL) {
    fprintf(stderr, "Can't find subcompositor\n");
    exit(1);
  }

  window.display = &display;

  create_main_surface(&window);
  create_sub_surface(&window);

  impl_subsurface(&window);

  create_texture();

  while(running) {
    wl_display_dispatch_pending(display.display);
    draw_main_surface(&window);
    draw_sub_surface(&window);
  }

  wl_surface_destroy(window.main_surface);
  wl_surface_destroy(window.sub_surface);
  wl_subsurface_destroy(window.subsurface);
  wl_display_disconnect(display.display);
  printf("disconnected from display\n");

  exit(0);
}
