// fixme: eliminate this
struct config {
    int width;
    int height;
};

int pmsh_main(int argc, char **argv);

void obey(projectM *pm);
std::string ask();
void act(projectM *pm, std::string line);

void move(projectM *pm, int increment);

int render(void *arg);
void xlock(SDL_mutex *mutex);
void xunlock(SDL_mutex *mutex);
void fatal();
projectM *init_projectm(config cfg);
void cleanup();
void init_projectm();
void init_sdl(config cfg);
config read_config(std::string path);
void debug_info(projectM *pm);
void handle_event();
void add_pcm(projectM *pm);

bool file_exists(std::string path);
std::string find_config();

char *error_playlist_invalid();
void keypress(SDLKey k);

void warn(const char *format, ...);
void die(const char *format, ...);
char *append_error_conversion(char *format);

void get_options(int argc, char **argv);
void version();


#define TEST_PRESET      "/home/amoe/milkdrop/test.milk"

#define PMSH_VERSION 1

