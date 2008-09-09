#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <iostream>

#include <libprojectM/projectM.hpp>
#include <pulse/pulseaudio.h>
#include <SDL/SDL.h>

#include "pmsh.hh"
#include "pulse.hh"
#include "ConfigFile.h" // local version

char binary_name[] = "pmsh";
std::string config_path;
state global;

int main(int argc, char **argv) {
    projectM *pm;
    pthread_t renderer;
    pa_threaded_mainloop *pa;
    config cfg;

    config_path = find_config();
    cfg = read_config(config_path);

    pthread_mutex_init(&(global.mutex), NULL);
    
    init_sdl(cfg);
    pm = init_projectm(cfg);
    // fixme: duh
    global.pm = pm;
    
    pa = init_pulseaudio(pm);

    pthread_create(&renderer, NULL, render, pm);
    
    obey(pm);

    cleanup();

    return 0;
}

// take commands from user
void obey(projectM *pm) {
    for (;;) {
        std::string resp = ask();

        // FIXME: separate function to use global state
        xlock(&(global.mutex));
        act(pm, resp);
        xunlock(&(global.mutex));
    }
}


std::string ask() {
    std::string line;

    std::cout << "> ";
    std::cout.flush();

    getline(std::cin, line);
    
    return line;
}

void act(projectM *pm, std::string line) {
    if (line.empty()) {
        // just eat the line
    }

    else if (line == "x") {
        cleanup();
        exit(0);
    }

    else if (line.at(0) == 'l') {
        if (line.size() > 2)
            cmd_load(pm, line.substr(2));
        else
            warn("command 'l' requires an argument");
    }

    else if (line == "r")
        cmd_reload(pm);
            
    else if (line == "c")
        pm->clearPlaylist();

    else if (line == "i")
        cmd_info(pm);

    else warn("unrecognized command");
}

void cmd_load(projectM *pm, std::string path) {
    std::cout << "Loading file: " << path << std::endl;

    // must use 3 or crash
    int idx = pm->addPresetURL(path, "current", 3);
    pm->selectPreset(idx);
    pm->setPresetLock(true);
}
 
void cmd_reload(projectM *pm) {
    unsigned int idx;
    bool not_empty = pm->selectedPresetIndex(idx);
    
    if (not_empty) {
        pm->selectPreset(idx);
    } else {
        warn("cannot reload empty playlist");
    }
}

void cmd_info(projectM *pm) {
    unsigned int idx;

    printf("PM playlist size: %d\n", pm->getPlaylistSize());
    printf("Locked: %d\n", pm->isPresetLocked());
    printf("Valid: %d\n", pm->presetPositionValid());
    //printf("Queued: %d\n", pm->isPresetQueued());

    bool not_end = pm->selectedPresetIndex(idx);
    if (not_end) {
        std::cout << "Selected preset index: " << idx << std::endl;
        std::cout << "Current URL: "<< pm->getPresetURL(idx) << std::endl;
    }
}


// perform the render loop (called in thread)
void *render(void *arg) {
    projectM *pm = (projectM *) arg;
    
    for (;;) {
        handle_event();

        xlock(&(global.mutex));
        //add_pcm(pm);
        pm->renderFrame();
        xunlock(&(global.mutex));
        
        SDL_GL_SwapBuffers();
        // do something
    }
}

void handle_event() {
    SDL_Event evt;

    while (SDL_PollEvent(&evt)) {
        switch (evt.type) {
            case SDL_QUIT:
                cleanup();
                exit(0);
            default:
                // eat this event
                break;
        }
    }
}

/*
void add_pcm(projectM *pm) {
    short data[2][512];

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 512; j++)
            data[i][j] = (short) rand();
    
    pm->pcm()->addPCM16(data);
}
*/

void init_sdl(config cfg) {
    SDL_Surface *ctx;
    int ret;

    ret = SDL_Init(SDL_INIT_VIDEO);
    if (ret == -1)  die("cannot initialize SDL: %s", SDL_GetError());

    ctx = SDL_SetVideoMode(cfg.width, cfg.height, 16, SDL_OPENGL);
    if (ctx == NULL) die("cannot set video mode: %s", SDL_GetError());

    SDL_WM_SetCaption("pmsh", NULL);
}

config read_config(std::string path) {
    config ret;
    ConfigFile cf(path);

    ret.width  = cf.read<int>("Window Width");
    ret.height = cf.read<int>("Window Height");

    return ret;
}

std::string find_config() {
    char *home = getenv("HOME");
    std::string etc_path(home), dot_path(home);

    etc_path.append("/etc/pmsh.inp");
    dot_path.append("/.pmsh.inp");

    if (file_exists(etc_path))
        return etc_path;
    else
        return dot_path;
}

bool file_exists(std::string path) {
    std::ifstream input;
    bool ret;

    input.open(path.c_str());
    ret = (bool) input;
    input.close();

    return ret;
}

projectM *init_projectm(config cfg) {
    projectM *ret;

    ret = new projectM(config_path, 0);
    
    return ret;
}

void xlock(pthread_mutex_t *mutex) {
    int ret;

    ret = pthread_mutex_lock(mutex);
    if (ret != 0)  die("cannot lock mutex: %s", strerror(errno));
}

void xunlock(pthread_mutex_t *mutex) {
    int ret;
    
    ret = pthread_mutex_unlock(mutex);
    if (ret != 0)  die("cannot unlock mutex: %s", strerror(errno));
}

void fatal() {
    perror("pmsh");
    exit(1);
}

// Caution:
// The order of cleaning stuff up is _very_ sensitive.
// Minor tweaks cause deadlocks, segfaults, X errors, double frees, pretty much
// anything you can imagine.
void cleanup() {
    int ret;

    // segfault?
    //delete global.pm;

    // WARNING: this lock MUST be released before obtaining the lock on the
    // mainloop, otherwise there's an extremely difficult to detect deadlock
    // with cb_stream_read().

    xunlock(&(global.mutex));

    // This can take a few seconds, don't despair
    puts("requesting lock");
    pa_threaded_mainloop_lock(global.threaded_mainloop);
    puts("got lock");
    //pa_threaded_mainloop_unlock(global.threaded_mainloop);

    if (global.stream) {
        puts("disconnecting & unreffing stream");
        ret = pa_stream_disconnect(global.stream);
        if (ret != 0)  die_pulse("cannot disconnect pulse stream");
        pa_stream_unref(global.stream);
    }

    // Once stream is DCed, cb_stream_read() will no longer be called.  So it's
    // safe to do the following and disregard the lock
    
    // Something here causes a segfault, no idea what
    // Could it be because this is being run from the original thread?
    // Maybe we need to lock the PA objects first or summat
    if (global.context) {
        puts("disconnecting context");
        pa_context_disconnect(global.context);
        puts("unreffing context");
        pa_context_unref(global.context);
    }
    
    if (global.mainloop_api) {
        puts("requesting quit from api");
        global.mainloop_api->quit(global.mainloop_api, 0);
    }

    puts("unlocking mainloop");
    pa_threaded_mainloop_unlock(global.threaded_mainloop);

    puts("stopping mainloop");
    if (global.threaded_mainloop) {
        pa_threaded_mainloop_stop(global.threaded_mainloop);
    }


    pthread_mutex_destroy(&(global.mutex));
    SDL_Quit();

    puts("deleting projectm instance");
    delete global.pm;
}

// FIXME: needs to be rewritten in C++ style
char *append_error_conversion(char *format) {
    char conv[] = ": %s";
    size_t len = strlen(format) + sizeof(conv);
    char *ret = (char *) malloc(len);

    strcpy(ret, format);
    strcat(ret, conv);
    
    return ret;
}


// Would be nice to have die() call warn(), but not so easy to do using the
// varargs mechanism
void warn(const char *format, ...) {
    va_list args;
    va_start(args, format);

    // XXX: we do not check return values here since we are dying anyway
    fprintf(stderr, "%s: ", binary_name);
    vfprintf(stderr, format, args);
    putc('\n', stderr);

    va_end(args);
}

// C99 mandates that we use restrict on the format pointer, but most C++
// compilers will mark it as an error, including G++
void die(const char *format, ...) {
    va_list args;
    va_start(args, format);

    // XXX: we do not check return values here since we are dying anyway
    fprintf(stderr, "%s: ", binary_name);
    vfprintf(stderr, format, args);
    putc('\n', stderr);

    va_end(args);

    cleanup();
    exit(EXIT_FAILURE);
}
