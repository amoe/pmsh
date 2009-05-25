#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <aio.h>
#include <errno.h>

#include <libprojectM/projectM.hpp>
#include <pulse/pulseaudio.h>
#include <SDL/SDL.h>

#include "pulse.hh"
#include "pmsh.hh"
#include "command.hh"
#include "ConfigFile.h" // local version

std::string config_path;
state global;

// main() needs to be in a separate file, because if we want to run unit tests
// for the routines in pmsh.cc, we need to be able to define main() and
// also link to pmsh.o - we could also use conditional compilation
int pmsh_main(int argc, char **argv) {
    pa_threaded_mainloop *pa;
    config cfg;

    global.terminated = false;

    get_options(argc, argv);

    config_path = find_config();
    cfg = read_config(config_path);

    global.window_width = cfg.width;
    global.window_height = cfg.height;

    global.mutex = SDL_CreateMutex();
    
    init_sdl(cfg);
    global.pm = init_projectm(cfg);
    
    pa = init_pulseaudio(global.pm);

    global.reader = SDL_CreateThread(obey, global.pm);
    render(global.pm);

    cleanup();

    return 0;
}


void get_options(int argc, char **argv) {
    int c;
    
    while ((c = getopt(argc, argv, "V")) != -1) {
        switch (c) {
            case 'V':
                version();
                exit(EXIT_SUCCESS);
                break;                 // not reached
            case '?':
                exit(EXIT_FAILURE);    // getopt() has printed an error
                break;
        }
    }
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


config read_config(std::string path) {
    try {
        config ret;
        ConfigFile cf(path);

        ret.width  = cf.read<int>("Window Width");
        ret.height = cf.read<int>("Window Height");

        return ret;
    } catch (ConfigFile::file_not_found e) {
        die(
            "config file not found: please create ~/.pmsh.inp or "
            "~/etc/pmsh.inp"
        );
    }
}


void init_sdl(config cfg) {
   SDL_Surface *ctx;
    int ret;

    ret = SDL_Init(SDL_INIT_VIDEO);
    if (ret == -1)  die("cannot initialize SDL: %s", SDL_GetError());

    ctx = SDL_SetVideoMode(cfg.width, cfg.height, 0, SDL_OPENGL);
    if (ctx == NULL) die("cannot set video mode: %s", SDL_GetError());

    SDL_WM_SetCaption("pmsh", NULL);
}


projectM *init_projectm(config cfg) {
    projectM *ret;

    ret = new projectM(config_path, 0);
    
    return ret;
}


// perform the render loop
void render(projectM *pm) {
    while (!global.terminated) {
        handle_event();

        xlock(global.mutex);

        pm->renderFrame();
        SDL_GL_SwapBuffers();

        xunlock(global.mutex);
    }
    //puts("renderer: quitting");
}


// take commands from user.  runs in separate thread.
int obey(void *arg) {
    projectM *pm = (projectM *) arg;

    while (!global.terminated && !std::cin.eof()) {
        std::string resp = ask();

        // FIXME, is it right to lock here?  Some commands might not mutate
        // state, and it requires the dubious hack of unlocking in cleanup()
        xlock(global.mutex);
        try {
            act(pm, resp);
        } catch (int e) {
            std::cerr << "exception: " << e << '\n';
        }
        xunlock(global.mutex);
    }

    // Since we've exited with a ^D if we get here, output an NL to make the
    // cleanup text look prettier
    std::cout << '\n';

    return 0;
}


std::string ask() {
    std::string line;

    std::cout << "> ";
    std::cout.flush();

    line = getline_interruptible();
    // This line blocks, which prevents the loop from checking the terminated
    // condition.
    //getline(std::cin, line);
    
    return line;
}

std::string getline_interruptible() {
    char *buf   = NULL;
    size_t size = 0;
    struct aiocb aiocb;
    std::string ret;

    memset(&aiocb, 0, sizeof(aiocb));

    aiocb.aio_fildes = fileno(stdin);
    aiocb.aio_offset = 0;
    aiocb.aio_buf    = buf;
    aiocb.aio_nbytes = 1;
    
    do {
        int     c;
        int     ret1;
        ssize_t ret2;

        aiocb.aio_buf = &c;

        ret1 = aio_read(&aiocb);
        if (ret1 != 0) {
            perror("aio_read");
            exit(1);
        }

        while (aio_error(&aiocb) == EINPROGRESS) {
            minisleep();
            if (global.terminated) {
                ret1 = aio_cancel(fileno(stdin), &aiocb);

                if (ret1 == AIO_CANCELED) {
                    puts("canceled AIO operation successfully");
                } else if (ret1 == AIO_NOTCANCELED) {
                    puts("at least one operation cannot be canceled");
                } else if (ret1 == AIO_ALLDONE) {
                    puts("all operations already completed");
                } else {
                    puts("aio_cancel failed, plz check errno");
                }
                
                return std::string();        // Return the empty string, which
                                             // causes another check on the
                                             // termination condition in obey().
            }
        }

        if ((ret2 = aio_return(&aiocb)) != -1) {
            // all good
        } else {
            die("cannot find AIO return status: %s", strerror(errno));
        }
        
        buf = (char *) realloc(buf, ++size);
        buf[size - 1] = c;
    } while (buf[size - 1] != '\n');

    buf[size - 1] = '\0';
    ret = buf;

    return ret;
}

void act(projectM *pm, std::string line) {
    std::string whitespace = " \t";
    std::string::size_type idx;
    char cmd;
    
    idx = line.find_first_not_of(whitespace);
    if (idx == std::string::npos)  return;
    cmd = line.at(idx);

    switch (cmd) {
    case 'x': {
        global.terminated = true;
        break;
    }

    case 'o': {
        std::cout << "toggling lock" << '\n';
        pm->setPresetLock(!pm->isPresetLocked());
        break;
    }

    case 'n': {
        cmd_next(pm);
        break;
    }

    case 'p': {
        cmd_prev(pm);
        break;
    }

    case 'r': {
        cmd_reload(pm);
        break;
    }

    case 'c': {
        pm->clearPlaylist();
        break;
    }

    case 'i': {
        cmd_info(pm);
        break;
    }

    case 'f': {
        cmd_fullscreen();
        break;
    }

    case 'l': {
        if (line.size() > (idx + 2))
            cmd_load(pm, line.substr(idx + 2));
        else
            warn("command '%c' requires an argument", cmd);

        break;
    }

    case 'd': {
        if (line.size() > (idx + 2))
            cmd_dir(pm, line.substr(idx + 2));
        else
            warn("command '%c' requires an argument", cmd);

        break;
    }

    default: {
        warn("unrecognized command");
    }
        
    }
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


void handle_event() {
    SDL_Event evt;

    while (SDL_PollEvent(&evt)) {
        switch (evt.type) {
            case SDL_KEYDOWN:
                keypress(evt.key.keysym.sym);
                break;
            case SDL_QUIT:
                global.terminated = true;
                break;
            default:
                // eat this event
                break;
        }
    }
}

void keypress(SDLKey k) {
    switch (k) {
        case SDLK_f:
            cmd_fullscreen();
            break;
        case SDLK_x:
            global.terminated = true;
            break;
    default:
        // eat this event
        break;
            
    }
}

/*
// Add random samples for testing
void add_pcm(projectM *pm) {
    short data[2][512];

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 512; j++)
            data[i][j] = (short) rand();
    
    pm->pcm()->addPCM16(data);
}
*/



bool file_exists(std::string path) {
    std::ifstream input;
    bool ret;

    input.open(path.c_str());
    ret = (bool) input;
    input.close();

    return ret;
}


const char *error_playlist_invalid() {
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

// Particularly important:
// 1.  SDL_Quit() MUST be called after terminating the render thread, since
// both renderFrame() and SDL_GL_SwapBuffers() will segfault if called either
// concurrently with or after it.
// 2.  If an SDL mutex is destroyed and another thread tries to lock it, the
// lock will hang indefinitely.  It will NOT give an error, despite the lock
// being destroyed - it looks just like a deadlock, but is not.
void cleanup() {
    int ret;

    std::cout << "cleaning up... ";
    std::cout.flush();

    // WARNING: this lock MUST be released before obtaining the lock on the
    // mainloop, otherwise there's an extremely difficult to detect deadlock
    // with cb_stream_read().
    puts("unlocking global mutex");
    xunlock(global.mutex);

    // This can take a few seconds, don't despair
    puts("requesting pulse lock");
    pa_threaded_mainloop_lock(global.threaded_mainloop);
    puts("got pulse lock");

    if (global.stream) {
        puts("disconnecting & unreffing pulse stream");
        ret = pa_stream_disconnect(global.stream);
        if (ret != 0)  die_pulse("cannot disconnect pulse stream");
        pa_stream_unref(global.stream);
    }
    
    if (global.context) {
        puts("disconnecting context");
        pa_context_disconnect(global.context);
        puts("unreffing context");
        pa_context_unref(global.context);
    }
    
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


    puts("terminating render thread");
    global.terminated = true;
    SDL_WaitThread(global.reader, NULL);

    // We can only destroy the mutex after terminating the rendering thread in
    // an "almost obvious" fact
    puts("destroying mutex");
    SDL_DestroyMutex(global.mutex);

    puts("deleting projectm instance");
    delete global.pm;
    
    puts("shutting down SDL");
    SDL_Quit();

    // ;)
    std::cout << " done.\n";
}

// Sleep a small amount of time.
void minisleep() {
    struct timespec rqtp;
    rqtp.tv_sec = 0;
    rqtp.tv_nsec = 10000000;
    nanosleep(&rqtp, NULL);
}

// FIXME: needs to be rewritten in C++ style
char *append_error_conversion(const char *format) {
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
    fprintf(stderr, "warning: ");
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
    fprintf(stderr, "error: ");
    vfprintf(stderr, format, args);
    putc('\n', stderr);

    va_end(args);

    //cleanup();
    exit(EXIT_FAILURE);
}


void version() {
    std::cout << "pmsh (the projectM shell) v" << PMSH_VERSION << '\n';
}

