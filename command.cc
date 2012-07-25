// command code goes in this file

#include <errno.h>
#include <string.h>
#include <dirent.h>

#include <iostream>

#include <libprojectM/projectM.hpp>
#include <SDL/SDL.h>
#include <pulse/pulseaudio.h>

#include "pulse.hh"
#include "command.hh"
#include "pmsh.hh"


void cmd_next(projectM *pm) {
    move(pm, +1);
}

void cmd_prev(projectM *pm) {
    move(pm, -1);
}

void cmd_load(projectM *pm, std::string path) {
    std::cout << "loading file: " << path << '\n';

    try {
        // just here to pacify the code; apparently 3 is mandatory
        int idx = pm->addPresetURL(path, "current", global.rating);
        pm->selectPreset(idx);
        pm->setPresetLock(true);
    } catch (int e) {
        warn("cannot load preset file '%s'", path.c_str());
    }
}

void cmd_dir(projectM *pm, std::string path) {
    DIR *dir = opendir(path.c_str());
    struct dirent *de;
    int ret;

    if (!dir) die("cannot open directory: %s", strerror(errno));

    while ((de = readdir(dir)) != NULL) {
        std::string abs = path + "/" + de->d_name;
        std::cout << abs << '\n';

        pm->addPresetURL(abs, "cmd_dir", global.rating);
    }

    ret = closedir(dir);
    if (ret != 0) die("cannot close directory: %s", strerror(errno));
}

void cmd_reload(projectM *pm) {
    unsigned int idx;
    bool not_empty = pm->selectedPresetIndex(idx);
    
    if (not_empty) {
        pm->selectPreset(idx);
    } else {
        warn("cannot reload: %s", error_playlist_invalid());
    }
}

void cmd_info(projectM *pm) {
    unsigned int idx;

    printf("PM playlist size: %d\n", pm->getPlaylistSize());
    printf("Locked: %d\n", pm->isPresetLocked());
    printf("Valid: %d\n", pm->presetPositionValid());
    //printf("Queued: %d\n", pm->isPresetQueued());

    bool valid = pm->selectedPresetIndex(idx);
    if (valid) {
        std::cout << "Selected preset index: " << idx << '\n';
        std::cout << "Current URL: "<< pm->getPresetURL(idx) << '\n';
    } else {
        warn(error_playlist_invalid());
    }
}

void cmd_fullscreen() {
    SDL_Surface *ctx;
    Uint32 flags;

    std::cout << "toggling fullscreen mode" << '\n';

    global.fullscreen = !global.fullscreen;

    flags = SDL_OPENGL;
    if (global.fullscreen) flags |= SDL_FULLSCREEN;

    ctx = SDL_SetVideoMode(
        global.window_width, global.window_height, 0, flags
    );
    if (ctx == NULL) die("cannot set video mode: %s", SDL_GetError());
    SDL_ShowCursor(global.fullscreen ? SDL_DISABLE : SDL_ENABLE);
    
    global.pm->projectM_resetGL(global.window_width, global.window_height);
}
