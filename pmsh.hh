// Mutex accessible through both threads, to lock the projectM pointer
extern pthread_mutex_t mtx;

struct config {
    int width;
    int height;
};

void obey(projectM *pm);
std::string ask();
void act(projectM *pm, std::string line);
void cmd_load(projectM *pm, std::string path);
void cmd_reload(projectM *pm);
void cmd_info(projectM *pm);
void *render(void *arg);
void xlock(pthread_mutex_t *mutex);
void xunlock(pthread_mutex_t *mutex);
void fatal();
projectM *init_projectm(config cfg);
void cleanup();
void init_projectm();
void init_sdl(config cfg);
config read_config();
void debug_info(projectM *pm);
void handle_event();
void add_pcm(projectM *pm);

void warn(const char *format, ...);
void die(const char *format, ...);
char *append_error_conversion(char *format);


#define PMSH_CONFIG_PATH "/home/amoe/etc/pmsh.cf"
#define TEST_PRESET      "/home/amoe/milkdrop/test.milk"

