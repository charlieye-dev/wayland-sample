#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <math.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <SOIL/SOIL.h>

#define WIDTH 720
#define HEIGHT 480

GLuint textureId;
GLuint program;
GLint samplerLoc;

static const char *vert_shader_text =
	"uniform mat4 rotation;\n"
	"attribute vec4 pos;\n"
	"attribute vec2 color;\n"
	"varying vec2 v_color;\n"
	"void main() {\n"
	"  gl_Position = pos;\n"
	"  v_color = color;\n"
	"}\n";

static const char *frag_shader_text =
	"precision mediump float;\n"
	"varying vec2 v_color;\n"
    "uniform sampler2D s_texture;\n"
	"void main() {\n"
	"  gl_FragColor = texture2D(s_texture, v_color);\n"
	"}\n";

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
  EGLConfig conf;
};

// listeners
static void registry_add_object(void *data, struct wl_registry *registry,
                    uint32_t name, const char *interface,  uint32_t version) {
  if (!strcmp(interface,"wl_compositor")) {
    compositor = wl_registry_bind (registry, name, &wl_compositor_interface, 1);
  } else if (!strcmp(interface,"wl_shell")) {
    shell = wl_registry_bind (registry, name, &wl_shell_interface, 1);
  } else if (!strcmp(interface,"wl_shell")) {
    shell = wl_registry_bind (registry, name, &wl_shell_interface, 1);
  }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) {

}

static struct wl_registry_listener registry_listener = {
  &registry_add_object,
  &registry_remove_object
};

static void shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_configure(void *data, struct wl_shell_surface *shell_surface,
                                   uint32_t edges, int32_t width, int32_t height) {
  struct window *window = data;
  wl_egl_window_resize(window->egl_window, width, height, 0, 0);
}

static void shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface) {

}

static struct wl_shell_surface_listener shell_surface_listener = {
  &shell_surface_ping,
  &shell_surface_configure,
  &shell_surface_popup_done
};

static void create_window(struct window *window, int32_t width, int32_t height) {
  window->surface = wl_compositor_create_surface(compositor);
  window->shell_surface = wl_shell_get_shell_surface(shell, window->surface);
  wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener, window);
  wl_shell_surface_set_toplevel(window->shell_surface);
  window->egl_window = wl_egl_window_create (window->surface, width, height);
  window->egl_surface = eglCreateWindowSurface (egl_display, window->conf, window->egl_window, NULL);
  eglMakeCurrent (egl_display, window->egl_surface, window->egl_surface, window->egl_context);
}

static void delete_window (struct window *window) {
  eglDestroySurface (egl_display, window->egl_surface);
  wl_egl_window_destroy (window->egl_window);
  wl_shell_surface_destroy (window->shell_surface);
  wl_surface_destroy (window->surface);
  eglDestroyContext (egl_display, window->egl_context);
}

static GLuint create_shader(const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(1);
	}

	return shader;
}

static void init_gl()
{
	GLuint frag, vert;
	GLint status;

	frag = create_shader(frag_shader_text, GL_FRAGMENT_SHADER);
	vert = create_shader(vert_shader_text, GL_VERTEX_SHADER);

	program = glCreateProgram();
	glAttachShader(program, frag);
	glAttachShader(program, vert);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}

	glUseProgram(program);

	glBindAttribLocation(program, 0, "pos");
	glBindAttribLocation(program, 1, "color");
	glLinkProgram(program);

	samplerLoc = glGetUniformLocation(program, "s_texture");
}

void create_texture() {
  int width, height;

  unsigned char* image = SOIL_load_image("./image.png", &width, &height, 0, SOIL_LOAD_RGB);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1 );
  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_2D, textureId);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void draw_window(struct window *window) {
  static const GLfloat verts[4][2] = {
		{ -0.8, -0.8 },
		{  0.8, -0.8 },
		{  0.8,  0.8 },
		{ -0.8,  0.8 }
  };
  static const GLfloat colors[4][2] = {
		{ 0.0, 0.0 },
		{ 1.0, 0.0 },
		{ 1.0, 1.0 },
		{ 0.0, 1.0 }
  };

  glUseProgram(program);


  glViewport(0, 0, WIDTH, HEIGHT);
  glClearColor(0.0, 0.0, 0.0, 0.5);
  glClear(GL_COLOR_BUFFER_BIT);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, colors);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureId);
  glUniform1i(samplerLoc, 0);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  glDeleteTextures(1, &textureId);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  eglSwapBuffers(egl_display, window->egl_surface);
}

static void signal_int(int signum)
{
  running = 0;
}

static void init_egl(struct window *window) {
  EGLint major, minor, n, count, i, size;
  EGLConfig *configs;
  EGLBoolean ret;

  static const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION,
    2,
    EGL_NONE
  };

  EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_ALPHA_SIZE, 1,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  egl_display = eglGetDisplay(display);
  assert(egl_display);

  ret = eglInitialize(egl_display, &major, &minor);
  assert(ret == EGL_TRUE);
  ret = eglBindAPI(EGL_OPENGL_ES_API);
  assert(ret == EGL_TRUE);

  if (!eglGetConfigs(egl_display, NULL, 0, &count) || count < 1)
    assert(0);

  configs = calloc(count, sizeof *configs);
  assert(configs);

  ret = eglChooseConfig(egl_display, config_attribs, configs, count, &n);
  assert(ret && n >= 1);

  for(i = 0; i < n; i++) {
    eglGetConfigAttrib(egl_display, configs[i], EGL_BUFFER_SIZE, &size);
    window->conf = configs[i];
  }

  window->egl_context = eglCreateContext(egl_display, window->conf, EGL_NO_CONTEXT, context_attribs);
  assert(window->egl_context);
}

static void init_wayland() {
  display = wl_display_connect(NULL);
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener (registry, &registry_listener, NULL);
  wl_display_roundtrip (display);
}

int main() {
  struct sigaction sigint;
  struct window window;

  init_wayland();

  init_egl(&window);
  create_window(&window, WIDTH, HEIGHT);

  init_gl();

  sigint.sa_handler = signal_int;
  sigemptyset(&sigint.sa_mask);
  sigint.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sigint, NULL);

  while (running) {
    wl_display_dispatch_pending(display);
    create_texture();
    draw_window(&window);
  }

  delete_window(&window);
  eglTerminate(egl_display);
  wl_display_disconnect(display);
  return 0;
}
