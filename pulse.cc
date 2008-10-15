#include <string>

#include <libprojectM/projectM.hpp>
#include <pulse/pulseaudio.h>

#include "pmsh.hh"
#include "pulse.hh"


// FIXME: return value is useless
pa_threaded_mainloop *init_pulseaudio(projectM *pm) {
    int ret;

    global.threaded_mainloop = pa_threaded_mainloop_new();
    if (!global.threaded_mainloop) die_pulse("cannot create threaded mainloop");

    pa_threaded_mainloop_lock(global.threaded_mainloop);

    global.mainloop_api = pa_threaded_mainloop_get_api(global.threaded_mainloop);
    if (!global.mainloop_api) die_pulse("cannot get mainloop api");

    // connect to server, all options at defaults
    global.context = pa_context_new(global.mainloop_api, "pmsh");
    if (!global.context) die_pulse("cannot create context");

    pa_context_set_state_callback(global.context, cb_context_state, pm);

    // XXX: undocumented return value
    ret = pa_context_connect(global.context, NULL, (pa_context_flags_t) 0, NULL);
    if (ret != 0) die_pulse("cannot connect pulse context");


    pa_threaded_mainloop_unlock(global.threaded_mainloop);

    ret = pa_threaded_mainloop_start(global.threaded_mainloop);
    if (ret != 0) die_pulse("cannot start main loop");

  return global.threaded_mainloop;
}


void die_pulse(char *message) {
    // unfreed msg is cleaned by program exit
    char *msg = append_error_conversion(message);
    die(msg, pa_strerror(pa_context_errno(global.context)));
}

void cb_context_state(pa_context *c, void *userdata) {
    pa_operation *o;

    puts("state cb");
    
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
            puts("stream ready");
            o = pa_context_get_source_info_list(
                c, cb_context_source_info_list, userdata
            );

            pa_operation_unref(o);

            break;
        default:
            // FIXME: handle all other events
            break;
    }
}

void cb_context_source_info_list(
    pa_context *c, const pa_source_info *i, int eol, void *userdata
) {
    int ret;
    puts("source info");

    if (!eol && is_monitor_source(i->name)) {
        global.stream = pa_stream_new(
            c, "input", &(i->sample_spec), &(i->channel_map)
        );
        if (!global.stream) die_pulse("cannot create pulse stream");

        pa_stream_set_read_callback(global.stream, cb_stream_read, userdata);

        ret = pa_stream_connect_record(
            global.stream, i->name, NULL, (pa_stream_flags_t) 0
        );
        if (ret != 0) die_pulse("cannot connect stream for recording");
    }
}

void cb_stream_read(pa_stream *p, size_t bytes, void *userdata) {
    projectM *pm = (projectM *) userdata;
    const void *data;
    int ret;

    printf("adding %d bytes of data\n", bytes);

    ret = pa_stream_peek(p, &data, &bytes);
    if (ret != 0) die_pulse("cannot peek pulse stream data");

    ret = pa_stream_drop(p);
    if (ret != 0) die_pulse("cannot drop pulse stream data");

    xlock(&(global.mutex));
    pm->pcm()->addPCMfloat((float *) data, bytes / (sizeof(float)));
    xunlock(&(global.mutex));
}

bool is_monitor_source(const char *name) {
    std::string str(name);
    std::string::size_type loc = str.find(".monitor", 0);
    return loc != std::string::npos;
}
