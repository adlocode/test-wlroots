#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/render/egl.h>

struct _TestServer
{
  struct wl_display *display;
  struct wl_event_loop *wl_event_loop;
  
  struct wlr_backend *backend;
  
  struct wlr_renderer *renderer;
  
  struct wl_listener new_output;
  
  struct wlr_output_layout *output_layout;
  struct wl_list outputs;
};

typedef struct _TestServer TestServer;

struct _Output
{
  struct wlr_output *wlr_output;
  TestServer *server;
  struct timespec last_frame;
  float color[4];
  int dec;
  
  struct wl_listener destroy;
  struct wl_listener frame;
  
  struct wl_list link;
};

typedef struct _Output Output;

static void output_destroy_notify (struct wl_listener *listener,
                                   void               *data);
static void output_frame_notify (struct wl_listener *listener,
                                 void               *data);

static void new_output_notify (struct wl_listener *listener,
                               void               *data)
{
  TestServer *server = wl_container_of (listener, server, new_output);
  struct wlr_output *wlr_output = data;
  
  if (!wl_list_empty (&wlr_output->modes))
		{
			struct wlr_output_mode *mode =
				wl_container_of(wlr_output->modes.prev, mode, link);
			wlr_output_set_mode(wlr_output, mode);
		}

	Output *output = calloc(1, sizeof(Output));
	clock_gettime(CLOCK_MONOTONIC, &output->last_frame);
	output->server = server;
	output->wlr_output = wlr_output;
	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
  
  wlr_output_create_global (wlr_output);
}

static void output_destroy_notify (struct wl_listener *listener,
                                   void               *data)
{
	Output *output = wl_container_of(listener, output, destroy);
	wl_list_remove (&output->link);
	wl_list_remove (&output->destroy.link);
	wl_list_remove(&output->frame.link);
	free (output);
}

static void output_frame_notify (struct wl_listener *listener,
                                 void               *data)
{
	Output *output = wl_container_of (listener, output, frame);
	struct wlr_output *wlr_output = data;
	struct wlr_renderer *renderer = wlr_backend_get_renderer (wlr_output->backend);

	wlr_output_attach_render (wlr_output, NULL);
  
  int width, height;
  wlr_output_effective_resolution (output->wlr_output, &width, &height);
	wlr_renderer_begin (renderer, width, height);

	float color[4] = {1.0, 0, 0, 1.0};
	wlr_renderer_clear(renderer, color);
  
  wlr_renderer_end (renderer);

	wlr_output_commit (output->wlr_output);
	
}

typedef struct _Output Output;

int main (int    argc,
          char **argv)
{
  TestServer *server;
  
  server = malloc (sizeof (TestServer));
  
  server->display = wl_display_create ();
  assert (server->display);
  server->wl_event_loop = wl_display_get_event_loop (server->display);
  assert (server->wl_event_loop);
  
  server->backend = wlr_backend_autocreate (server->display, NULL);
  assert (server->backend);
  
  wl_list_init (&server->outputs);
  
  server->new_output.notify = new_output_notify;
  wl_signal_add (&server->backend->events.new_output, &server->new_output);
  
  if (!wlr_backend_start (server->backend))
    {
      fprintf (stderr, "Failed to start backend\n");
      wl_display_destroy (server->display);
      return 1;
    }
  
  wl_display_run (server->display);
  wl_display_destroy (server->display);
  return 0;
}
