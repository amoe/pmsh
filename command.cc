// command code goes in this file

#include <errno.h>
#include <string.h>
#include <dirent.h>

#include <libprojectM/projectM.hpp>
#include <SDL/SDL.h>

#include "command.hh"
#include "pmsh.hh"

void cmd_next(projectM *pm) {
    move(pm, +1);
}

void cmd_prev(projectM *pm) {
    move(pm, -1);
}

void cmd_load(projectM *pm, std::string path) {
    std::cout << "Loading file: " << path << std::endl;

    // must use 3 or crash
    int idx = pm->addPresetURL(path, "current", 3);
    pm->selectPreset(idx);
    pm->setPresetLock(true);
}

void cmd_dir(projectM *pm, std::string path) {
    DIR *dir = opendir(path.c_str());
    struct dirent *de;
    int ret;

    if (!dir) die("cannot open directory: %s", strerror(errno));

    while ((de = readdir(dir)) != NULL) {
        std::string abs = path + "/" + de->d_name;
        std::cout << abs << std::endl;

        pm->addPresetURL(abs, "cmd_dir", 3);
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
        std::cout << "Selected preset index: " << idx << std::endl;
        std::cout << "Current URL: "<< pm->getPresetURL(idx) << std::endl;
    } else {
        warn(error_playlist_invalid());
    }
}

void cmd_fullscreen() {
    std::cout << "Setting fullscreen mode" << '\n';
}
