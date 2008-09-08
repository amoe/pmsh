struct state {
    pthread_mutex_t      mutex;
    pa_threaded_mainloop *threaded_mainloop;
    pa_mainloop_api      *mainloop_api;
    pa_context           *context;
    pa_stream            *stream;
};

void run_pulseaudio();
pa_threaded_mainloop *init_pulseaudio(projectM *pm);
bool is_monitor_source(const char *name);

// pa callbacks
void cb_context_state(pa_context *c, void *userdata);
void cb_context_source_info_list(
    pa_context *c, const pa_source_info *i, int eol, void *userdata
);
void cb_stream_read(pa_stream *p, size_t bytes, void *userdata);

void die_pulse(char *message);

extern state global;
