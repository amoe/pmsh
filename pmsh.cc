#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <iostream>

#include <libprojectM/projectM.hpp>
#include <pulse/pulseaudio.h>
#include <SDL/SDL.h>

#include "pmsh.hh"
#include "command.hh"
#include "pulse.hh"
#include "ConfigFile.h" // local version

char binary_name[] = "pmsh";
std::string config_path;
state global;

// main() needs to be in a separate file, because if we want to run unit tests
// for the routines in pmsh.cc, we need to be able to define main() and
// also link to pmsh.o - we could also use conditional compilation
int pmsh_main(int argc, char **argv) {
    projectM *pm;
    pa_threaded_mainloop *pa;
    config cfg;

    global.terminated = false;

    config_path = find_config();
    cfg = read_config(config_path);

    global.mutex = SDL_CreateMutex();
    
    init_sdl(cfg);
    pm = init_projectm(cfg);
    // fixme: duh
    global.pm = pm;
    
    pa = init_pulseaudio(pm);

    global.renderer = SDL_CreateThread(render, pm);
    
    obey(pm);

    cleanup();

    return 0;
}

// take commands from user
void obey(projectM *pm) {
    for (;;) {
        std::string resp = ask();

        // FIXME: separate function to use global state
        xlock(global.mutex);
        act(pm, resp);
        xunlock(global.mutex);
    }
}


std::string ask() {
    std::string line;

    std::cout << "> ";
    std::cout.flush();

    getline(std::cin, line);
    
    return line;
}

// fixme: switch on line.at(0) skipping whitespace
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

    else if (line == "o") {
        std::cout << "toggling lock" << std::endl;
        pm->setPresetLock(!pm->isPresetLocked());
    }

    else if (line == "n") {
        cmd_next(pm);
    }
    
    else if (line == "p") {
        cmd_prev(pm);
    }

    else if (line.at(0) == 'd') {
        if (line.size() > 2)
            cmd_dir(pm, line.substr(2));
        else
            warn("command 'd' requires an argument");
    }

    else if (line == "r")
        cmd_reload(pm);

            
    else if (line == "c")
        pm->clearPlaylist();

    else if (line == "i")
        cmd_info(pm);

    else if (line == "f")
        cmd_fullscreen();

    else warn("unrecognized command");
}

void move(projectM *pm, int increment) {
    unsigned int idx;
    bool valid = pm->selectedPresetIndex(idx);

    if (valid) {
        pm->selectPreset(idx + increment);
    } else {
        warn("cannot move: %s", error_playlist_invalid());
    }
}

// perform the render loop (called in thread)
int render(void *arg) {
    projectM *pm = (projectM *) arg;
    
    while (!global.terminated) {
        handle_event();

        xlock(global.mutex);
        pm->renderFrame();
        SDL_GL_SwapBuffers();
        xunlock(global.mutex);
    }
}

void handle_event() {
    SDL_Event evt;

    while (SDL_PollEvent(&evt)) {
        switch (evt.type) {
            case SDL_QUIT:
                cleanup();
                puts("exiting");
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

char *error_playlist_invalid() {
    if (global.pm->getPlaylistSize() == 0) {
        return "playlist is empty";
    } else if (!global.pm->presetPositionValid()) {
        return "preset index is invalid";
    } else {
        return "unknown error";
    }
}


void xlock(SDL_mutex *mutex) {
    int ret;

    ret = SDL_LockMutex(mutex);
    if (ret != 0)  die("cannot lock mutex: %s", SDL_GetError());
}

void xunlock(SDL_mutex *mutex) {
    int ret;
    
    ret = SDL_UnlockMutex(mutex);
    if (ret != 0)  die("cannot unlock mutex: %s", SDL_GetError());
}

// Caution:
// The order of cleaning stuff up is _very_ sensitive.
// Minor tweaks cause deadlocks, segfaults, X errors, double frees, pretty much
// anything you can imagine.
void cleanup() {
    int ret;


    puts("entering cleanup()");

    // WARNING: this lock MUST be released before obtaining the lock on the
    // mainloop, otherwise there's an extremely difficult to detect deadlock
    // with cb_stream_read().

    xunlock(global.mutex);

    // This can take a few seconds, don't despair
    puts("requesting lock");
    pa_threaded_mainloop_lock(global.threaded_mainloop);
    puts("got lock");

    if (global.stream) {
        puts("disconnecting & unreffing stream");
        ret = pa_stream_disconnect(global.stream);
        if (ret != 0)  die_pulse("cannot disconnect pulse stream");
        pa_stream_unref(global.stream);
    }

    // We SHOULD be able to delete projectM here, since cb_stream_read() is no
    // longer called.  But it doesn't work that way in reality
    
    if (global.context) {
        puts("disconnecting context");
        pa_context_disconnect(global.context);
        puts("unreffing context");
        pa_context_unref(global.context);
    }
    
    // Crash occurs after unlocking mainloop
    // So possibly this is in the wrong order?
    /*
    if (global.mainloop_api) {
        puts("requesting quit from api");
        global.mainloop_api->quit(global.mainloop_api, 0);
    }

    puts("unlocking mainloop");
    pa_threaded_mainloop_unlock(global.threaded_mainloop);
    */


    puts("unlocking mainloop");
    pa_threaded_mainloop_unlock(global.threaded_mainloop);

    if (global.mainloop_api) {
        puts("requesting quit from api");
        global.mainloop_api->quit(global.mainloop_api, 0);
    }

    puts("stopping mainloop");
    if (global.threaded_mainloop) {
        pa_threaded_mainloop_stop(global.threaded_mainloop);
    }


    puts("destroying mutex");
    SDL_DestroyMutex(global.mutex);

    puts("terminating render thread");
    global.terminated = true;
    SDL_WaitThread(global.renderer, NULL);

    puts("deleting projectm instance");
    delete global.pm;
    
    // IMPORTANT:
    /* According to the SDL docs, a user should always shut down the library
       with SDL_Quit().  However, calling SDL_Quit() either before or after
       deleting the projectM instance causes a hard lock on my system.  Is this
       related to the 'fullscreen quit kills mouse' issue? */

    puts("shutting down SDL");
    SDL_Quit();
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

    // Would be nice to check these returns somehow
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

    // We do not check return values here since we are dying anyway
    fprintf(stderr, "%s: ", binary_name);
    vfprintf(stderr, format, args);
    putc('\n', stderr);

    va_end(args);

    cleanup();
    exit(EXIT_FAILURE);
}
