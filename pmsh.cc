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

// Include order: C headers, C++ headers, lib headers, project headers

state global;

int main(int argc, char **argv) {
    projectM *pm;
    pthread_t renderer;
    pa_threaded_mainloop *pa;
    config cfg;

    cfg = read_config();

    pthread_mutex_init(&(global.mutex), NULL);
    
    init_sdl(cfg);
    pm = init_projectm(cfg);
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

config read_config() {
    config ret;
    ConfigFile cf(PMSH_CONFIG_PATH);

    ret.width  = cf.read<int>("Window Width");
    ret.height = cf.read<int>("Window Height");

    return ret;
}

projectM *init_projectm(config cfg) {
    projectM *ret;

    ret = new projectM(PMSH_CONFIG_PATH, 0);
    
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

void cleanup() {
    // First, clean up all pulse business, starting with the stream
    int ret;

    // This freezes the whole program.  Why????  Where is it blocking?
    // NB!!!! problem only occurs when PA is running & music is playing!
    // Otherwise the lock is fine
    pa_threaded_mainloop_lock(global.threaded_mainloop);
    puts("got lock");
    pa_threaded_mainloop_unlock(global.threaded_mainloop);

/*
    if (global.stream) {
        ret = pa_stream_disconnect(global.stream);
        if (ret != 0)  die_pulse("cannot disconnect pulse stream");
        pa_stream_unref(global.stream);
    }

    // Something here causes a segfault, no idea what
    // Could it be because this is being run from the original thread?
    // Maybe we need to lock the PA objects first or summat
    if (global.context) {
        pa_context_disconnect(global.context);
        pa_context_unref(global.context);
    }
    

    if (global.mainloop_api) {
        global.mainloop_api->quit(global.mainloop_api, 0);
    }

    pa_threaded_mainloop_unlock(global.threaded_mainloop);

    if (global.threaded_mainloop) {
        pa_threaded_mainloop_stop(global.threaded_mainloop);
    }

    pthread_mutex_destroy(&(global.mutex));
    
    SDL_Quit();
*/
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
