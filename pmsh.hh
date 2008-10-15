struct config {
    int width;
    int height;
};

int pmsh_main(int argc, char **argv);

void obey(projectM *pm);
std::string ask();
void act(projectM *pm, std::string line);

void move(projectM *pm, int increment);

void *render(void *arg);
void xlock(pthread_mutex_t *mutex);
void xunlock(pthread_mutex_t *mutex);
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

void warn(const char *format, ...);
void die(const char *format, ...);
char *append_error_conversion(char *format);


#define TEST_PRESET      "/home/amoe/milkdrop/test.milk"

