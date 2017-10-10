#include <stdio.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <string.h>
#include <signal.h>

#define WIDTH 256
#define HEIGHT 256
GLubyte image[64][64][4];

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_shell *shell = NULL;
static EGLDisplay egl_display;
static char running = 1;

struct window {
  EGLContext egl_context;
  struct wl_surface *surface;
  struct wl_shell_surface *shell_surface;
  struct wl_subsurface *sub_surface;
  struct wl_egl_window *egl_window;
  EGLSurface egl_surface;
};

// listeners
static void registry_add_object (void *data, struct wl_registry *registry,
                    uint32_t name, const char *interface,  uint32_t version) {
  if (!strcmp(interface,"wl_compositor")) {
    compositor = wl_registry_bind (registry, name, &wl_compositor_interface, 1);
  } else if (!strcmp(interface,"wl_shell")) {
    shell = wl_registry_bind (registry, name, &wl_shell_interface, 1);
  } else if (!strcmp(interface,"wl_shell")) {
    shell = wl_registry_bind (registry, name, &wl_shell_interface, 1);
  }
}

static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {

}

static struct wl_registry_listener registry_listener = {
  &registry_add_object,
  &registry_remove_object
};

static void shell_surface_ping (void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
  wl_shell_surface_pong (shell_surface, serial);
}

static void shell_surface_configure (void *data, struct wl_shell_surface *shell_surface,
                                   uint32_t edges, int32_t width, int32_t height) {
  struct window *window = data;
  wl_egl_window_resize (window->egl_window, width, height, 0, 0);
}

static void shell_surface_popup_done (void *data, struct wl_shell_surface *shell_surface) {

}

static struct wl_shell_surface_listener shell_surface_listener = {
  &shell_surface_ping,
  &shell_surface_configure,
  &shell_surface_popup_done
};

static void create_window (struct window *window, int32_t width, int32_t height) {
  EGLConfig config;
  EGLint num_config;
  EGLint attributes[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_NONE
  };

  eglBindAPI (EGL_OPENGL_API);
  eglChooseConfig (egl_display, attributes, &config, 1, &num_config);
  window->egl_context = eglCreateContext (egl_display, config, EGL_NO_CONTEXT, NULL);

  window->surface = wl_compositor_create_surface (compositor);
  window->shell_surface = wl_shell_get_shell_surface (shell, window->surface);
  wl_shell_surface_add_listener (window->shell_surface, &shell_surface_listener, window);
  wl_shell_surface_set_toplevel (window->shell_surface);
  window->egl_window = wl_egl_window_create (window->surface, width, height);
  window->egl_surface = eglCreateWindowSurface (egl_display, config, window->egl_window, NULL);
  eglMakeCurrent (egl_display, window->egl_surface, window->egl_surface, window->egl_context);
}

static void delete_window (struct window *window) {
  eglDestroySurface (egl_display, window->egl_surface);
  wl_egl_window_destroy (window->egl_window);
  wl_shell_surface_destroy (window->shell_surface);
  wl_surface_destroy (window->surface);
  eglDestroyContext (egl_display, window->egl_context);
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

static void draw_window (struct window *window) {
  glViewport(0, 0, WIDTH, HEIGHT);

  glClearColor (0.5, 0.5, 0.5, 0.5);
  glClear (GL_COLOR_BUFFER_BIT);

  static const GLfloat texcoord[4][2] = {
    { 0, 0 },
    { 1, 0 },
    { 0, 1 },
    { 1, 1 }
  };
  static const GLfloat vertex[4][2] = {
    { -0.5, -0.5 },
    {  0.5, -0.5 },
    { -0.5,  0.5 },
    {  0.5,  0.5 }
  };

  glVertexPointer(2, GL_FLOAT, 0, vertex);
  glTexCoordPointer(2, GL_FLOAT, 0, texcoord);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  eglSwapBuffers (egl_display, window->egl_surface);
}

static void signal_int(int signum)
{
  running = 0;
}

int main () {
  struct sigaction sigint;
  EGLint major, minor;

  display = wl_display_connect (NULL);
  struct wl_registry *registry = wl_display_get_registry (display);
  wl_registry_add_listener (registry, &registry_listener, NULL);
  wl_display_roundtrip (display);

  egl_display = eglGetDisplay (display);
  eglInitialize (egl_display, &major, &minor);

  struct window window;
  create_window (&window, WIDTH, HEIGHT);

  sigint.sa_handler = signal_int;
  sigemptyset(&sigint.sa_mask);
  sigint.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sigint, NULL);
  create_texture();

  while (running) {
    wl_display_dispatch_pending (display);
    draw_window (&window);
  }

  delete_window (&window);
  eglTerminate (egl_display);
  wl_display_disconnect (display);
  return 0;
}
