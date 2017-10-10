#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <string.h>
#include <signal.h>

#define WIDTH 256
#define HEIGHT 256
GLubyte image[64][64][4];
static int running = 1;

struct display {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shell *shell;
  EGLDisplay egl_display;
  EGLContext egl_context;
};

struct window {
  struct wl_surface *surface;
  struct wl_shell_surface *shell_surface;
  struct wl_egl_window *egl_window;
  EGLSurface egl_surface;
};

struct display *tdisplay = NULL;

using namespace std;

/* Handle signal. */
static void signal_int(int signum) {
  running = 0;
}

static void registry_add_object (void *data, struct wl_registry *registry,
                    uint32_t name, const char *interface,  uint32_t version) {
  if (!strcmp(interface,"wl_compositor")) {
      tdisplay->compositor =
            (struct wl_compositor*)wl_registry_bind (registry, name, &wl_compositor_interface, 1);
  } else if (!strcmp(interface,"wl_shell")) {
      tdisplay->shell =
            (struct wl_shell*)wl_registry_bind (registry, name, &wl_shell_interface, 1);
  }
}

static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {

}

static struct wl_registry_listener registry_listener = {
  &registry_add_object,
  &registry_remove_object
};

static void shell_surface_ping (void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
  printf("%d\n", __LINE__);
  wl_shell_surface_pong (shell_surface, serial);
  printf("%d\n", __LINE__);
}

static void shell_surface_configure (void *data, struct wl_shell_surface *shell_surface,
                                   uint32_t edges, int32_t width, int32_t height) {
  struct window *window = (struct window *)data;
  printf("%d\n", __LINE__);
  wl_egl_window_resize (window->egl_window, WIDTH, HEIGHT, 0, 0);
  printf("%d\n", __LINE__);
}

static void shell_surface_popup_done (void *data, struct wl_shell_surface *shell_surface) {

}

static struct wl_shell_surface_listener shell_surface_listener = {
  &shell_surface_ping,
  &shell_surface_configure,
  &shell_surface_popup_done
};


class CrVideoTunnelAction {
public:
  CrVideoTunnelAction ();
  void Init();
  void InitWayland();
  void InitEGL();
  void Final();
  void CreateTexture();
  void CreateSurface();
  void ReDraw();
  void Render(unsigned int fd);
  void HandleSignal();
private:
  struct sigaction sigint;
  struct display *display = NULL;
  struct window *window = NULL;
};

CrVideoTunnelAction::CrVideoTunnelAction() {
  display = (struct display*)malloc(sizeof(*display));
  tdisplay = (struct display*)malloc(sizeof(*tdisplay));
  window = (struct window*)malloc(sizeof(*window));
}

void CrVideoTunnelAction::HandleSignal() {
  sigint.sa_handler = signal_int;
  sigemptyset(&sigint.sa_mask);
  sigint.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sigint, NULL);

}

void CrVideoTunnelAction::Final() {
  eglDestroySurface (display->egl_display, window->egl_surface);
  wl_egl_window_destroy (window->egl_window);
  wl_shell_surface_destroy (window->shell_surface);
  wl_surface_destroy (window->surface);
  eglDestroyContext (display->egl_display, display->egl_context);
  eglTerminate (display->egl_display);
  wl_display_disconnect (display->display);
  wl_display_flush(display->display);
}

void CrVideoTunnelAction::InitWayland() {
  display->display = wl_display_connect(NULL);
  assert(display->display);

  display->registry = wl_display_get_registry (display->display);
  assert(display->registry);

  wl_registry_add_listener(display->registry, &registry_listener, NULL);
  wl_display_roundtrip(display->display);
  display->compositor = tdisplay->compositor;
  display->shell = tdisplay->shell;
}

void CrVideoTunnelAction::InitEGL() {
  EGLint major, minor;

  display->egl_display = eglGetDisplay (display->display);
  assert(display->egl_display);

  eglInitialize (display->egl_display, &major, &minor);
  eglBindAPI(EGL_OPENGL_API);
}

void CrVideoTunnelAction::Init() {
  InitWayland();
  InitEGL();
}

void CrVideoTunnelAction::CreateSurface() {
  EGLConfig config;
  EGLint num_config;
  EGLint attributes[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_NONE};

  eglChooseConfig(display->egl_display, attributes, &config, 1, &num_config);
  display->egl_context = eglCreateContext(display->egl_display, config, EGL_NO_CONTEXT, NULL);
  assert(display->egl_context);

  window->surface = wl_compositor_create_surface (display->compositor);
  assert(window->surface);

  window->shell_surface = wl_shell_get_shell_surface (display->shell, window->surface);
  assert(window->shell_surface);

  wl_shell_surface_add_listener (window->shell_surface, &shell_surface_listener, window);
  wl_shell_surface_set_toplevel (window->shell_surface);
  window->egl_window = wl_egl_window_create (window->surface,
                                    WIDTH, HEIGHT);
  assert(window->egl_window);

  window->egl_surface = eglCreateWindowSurface (display->egl_display,
                                    config, window->egl_window, NULL);
  assert(window->egl_surface);

  eglMakeCurrent (display->egl_display, window->egl_surface,
                                    window->egl_surface, display->egl_context);
}

void CrVideoTunnelAction::CreateTexture() {
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

void CrVideoTunnelAction::ReDraw() {
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

  eglSwapBuffers (display->egl_display, window->egl_surface);
}

int main() {
  CrVideoTunnelAction *tunnel_action = new CrVideoTunnelAction();

  tunnel_action->Init();
  tunnel_action->CreateSurface();
  tunnel_action->CreateTexture();
  tunnel_action->HandleSignal();

  while(running) {
    tunnel_action->ReDraw();
  }

  tunnel_action->Final();
  delete tunnel_action;

}
