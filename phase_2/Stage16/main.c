#include "xps.h"

xps_core_t *core;

void sigint_handler(int signum);

int core_create(xps_config_t *config);
void core_destroy(xps_core_t *core);

int main(int argc, char *argv[]) {
  signal(SIGINT, sigint_handler);
  xps_cliargs_t *cliargs = xps_cliargs_create(argc, argv);
  if (cliargs == NULL) {
      logger(LOG_ERROR, "main()", "xps_cliargs_create() failed");
      return -1;
  }
  xps_config_t *config = xps_config_create(cliargs->config_path);
  if (config == NULL) {
      logger(LOG_ERROR, "main()", "xps_config_create() failed");
      free(cliargs);
      return -1;
  }
  if (core_create(config)!=0){
    logger(LOG_ERROR, "main()", "core_create() failed");
    free(cliargs);
    free(config);
    return -1;
  }

  xps_core_start(core);

}

int core_create(xps_config_t *config) {
  assert(config!=NULL);
  core = xps_core_create(config);
  /*Create listeners*/
// Create listeners from config's _all_listeners
  for (int i = 0; i < config->_all_listeners.length; i++) {
    xps_config_listener_t *config_listener = config->_all_listeners.data[i];

    // Create the canonical listener (one per port)
    xps_listener_t *listener = xps_listener_create(config_listener->host, config_listener->port);
    if (listener == NULL) {
        logger(LOG_ERROR, "core_create()", "xps_listener_create() failed for %s:%u",
                config_listener->host, config_listener->port);
        return E_FAIL;
    }
    /*Duplicate (use dup(fd) to duplicate file descriptor) and add listeners to cores*/
    int dup_fd = dup(listener->sock_fd);
    if (dup_fd < 0) {
        logger(LOG_ERROR, "core_create()", "dup() failed for port %u", config_listener->port);
        xps_listener_destroy(listener);
        core_destroy(core);
        return E_FAIL;
    }
    xps_listener_t *dup_listener = malloc(sizeof(xps_listener_t));
    if (dup_listener == NULL) {
        logger(LOG_ERROR, "core_create()", "malloc() failed for dup_listener");
        close(dup_fd);
        xps_listener_destroy(listener);
        core_destroy(core);
        return E_FAIL;
    }
    dup_listener->core    = core;
    dup_listener->host    = config_listener->host;
    dup_listener->port    = config_listener->port;
    dup_listener->sock_fd = dup_fd;
    /*Attach dup_listener to loop*/
    if (xps_loop_attach(core->loop, dup_listener->sock_fd,  EPOLLIN | EPOLLET, dup_listener, listener_connection_handler, NULL, NULL) != OK) {
      logger(LOG_ERROR, "core_create()", "xps_loop_attach() failed for port %u", config_listener->port);
      free(dup_listener);
      close(dup_fd);
      xps_listener_destroy(listener);
      core_destroy(core);
      return E_FAIL;
    }
    /*Add listener to 'listeners' list of core*/
    vec_push(&core->listeners, dup_listener);
    /*Destory listeners*/
    close(listener->sock_fd);
    free(listener);
    logger(LOG_INFO, "core_create()", "listener attached on %s:%u", config_listener->host, config_listener->port);
  }
  return 0;
}

void core_destroy(xps_core_t *core) {
    assert(core != NULL);
    xps_core_destroy(core);
}

void sigint_handler(int signum) {
  logger(LOG_WARNING, "sigint_handler()", "SIGINT received");

  core_destroy(core);

  exit(EXIT_SUCCESS);
}