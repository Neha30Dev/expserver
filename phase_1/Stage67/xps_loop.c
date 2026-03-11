#include "xps_loop.h"

loop_event_t *loop_event_create(u_int fd, void *ptr, xps_handler_t read_cb, xps_handler_t write_cb, xps_handler_t close_cb) {
    assert(ptr != NULL);

    // Alloc memory for 'event' instance
    loop_event_t *event = malloc(sizeof(loop_event_t));
    if (event == NULL) {
        logger(LOG_ERROR, "event_create()", "malloc() failed for 'event'");
        return NULL;
    }

    event->fd = fd;
    event->read_cb = read_cb;
    event->write_cb = write_cb;
    event->close_cb = close_cb;
    event->ptr = ptr;

    logger(LOG_DEBUG, "event_create()", "created event");

    return event;
}

void loop_event_destroy(loop_event_t *event) {
    assert(event != NULL);

    free(event);

    logger(LOG_DEBUG, "event_destroy()", "destroyed event");
}

xps_loop_t *xps_loop_create(xps_core_t *core) {
    assert(core != NULL);

    xps_loop_t *loop = malloc(sizeof(xps_loop_t));
    if(loop == NULL){
        logger(LOG_ERROR, "xps_loop_create()", "malloc() failed for loop");
        return NULL;
    }

    loop->core = core;
    loop->epoll_fd=epoll_create1(0);
    if(loop->epoll_fd < 0){
        logger(LOG_ERROR, "xps_loop_create()", "epoll_create1() failed");
        free(loop);
        return NULL;
    }

    vec_init(&loop->events);

    loop->n_null_events = 0;

    return loop;

}

void xps_loop_destroy(xps_loop_t *loop) {
    assert(loop != NULL);

    for(int i=0; i<loop->events.length; i++){
        loop_event_t *event = loop->events.data[i];

        if(event!=NULL){
            loop_event_destroy(event);
        }
    }

    vec_deinit(&loop->events);

    close(loop->epoll_fd);

    free(loop);

}


int xps_loop_attach(xps_loop_t *loop, u_int fd, int event_flags, void *ptr, xps_handler_t read_cb, xps_handler_t write_cb, xps_handler_t close_cb) {
    assert(loop != NULL);
    assert(ptr != NULL);

    loop_event_t *event = loop_event_create(fd,ptr, read_cb, write_cb, close_cb);

    if (event == NULL) {
        logger(LOG_ERROR, "xps_loop_attach()", "loop_event_create() failed");
        return E_FAIL;
    }

    struct epoll_event epoll_event;
    epoll_event.events=event_flags;
    epoll_event.data.ptr=event;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
        logger(LOG_ERROR, "xps_loop_attach()", "epoll_ctl() failed");
        perror("Error message");
        loop_event_destroy(event);
        return E_FAIL;
    }

    vec_push(&loop->events, event);

    return OK;
}


int xps_loop_detach(xps_loop_t *loop, u_int fd) {
    assert(loop != NULL);

    for(int i=0;i<loop->events.length;i++){
        loop_event_t *curr = loop->events.data[i];
        if(curr != NULL && curr->fd == fd){
            if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
                logger(LOG_ERROR, "xps_loop_detach()", "epoll_ctl() failed");
                perror("Error message");
                return E_FAIL;
            }

            loop_event_destroy(curr);
            loop->events.data[i] = NULL;
            loop->n_null_events++;
            return OK;
        }
    }

    return E_FAIL;

}

void xps_loop_run(xps_loop_t *loop) {
    assert(loop!=NULL);

    while (1) {
        bool has_ready_connections = handle_connections(loop);
        int timeout=-1;
        if(has_ready_connections) timeout=0;
        logger(LOG_DEBUG, "xps_loop_run()", "epoll wait");
        int n_events = epoll_wait(loop->epoll_fd, loop->epoll_events, MAX_EPOLL_EVENTS, timeout);
        logger(LOG_DEBUG, "xps_loop_run()", "epoll wait over");

        logger(LOG_DEBUG, "xps_loop_run()", "handling %d events", n_events);

        // Handle events
        for (int i = 0; i < n_events; i++) {
            logger(LOG_DEBUG, "xps_loop_run()", "handling event no. %d", i + 1);

            struct epoll_event curr_epoll_event = loop->epoll_events[i];
            loop_event_t *curr_event = curr_epoll_event.data.ptr;

            // Check if event still exists. Could have been destroyed due to prev event
            int curr_event_idx = -1;

            for(int j=0;j<loop->events.length;j++){
                if(loop->events.data[j] == curr_event){
                    curr_event_idx = j;
                    break;
                }
            }

            if (curr_event_idx == -1) {
                logger(LOG_DEBUG, "handle_epoll_events()", "event not found. skipping");
                continue;
            }

            //Close event
            if (curr_epoll_event.events & (EPOLLERR | EPOLLHUP)) {
                logger(LOG_DEBUG, "handle_epoll_events()", "EVENT / close");
                if (curr_event->close_cb != NULL)
                    // Pass the ptr from loop_event_t as a parameter to the callback
                    curr_event->close_cb(curr_event->ptr);
            }

            if (curr_event == NULL)
                continue;

            // Read event
            if (curr_epoll_event.events & EPOLLIN) {
                logger(LOG_DEBUG, "handle_epoll_events()", "EVENT / read");
                if (curr_event->read_cb != NULL)
                    // Pass the ptr from loop_event_t as a parameter to the callback
                    curr_event->read_cb(curr_event->ptr);
            }

            if (curr_event == NULL)
                continue;
        
            //Write event
            if (curr_epoll_event.events & EPOLLOUT) {
                logger(LOG_DEBUG, "handle_epoll_events()", "EVENT / write");
                if (curr_event->write_cb != NULL)
                    // Pass the ptr from loop_event_t as a parameter to the callback
                    curr_event->write_cb(curr_event->ptr);
            }
        }
    }
}

bool handle_connections(xps_loop_t* loop) {

    vec_void_t *connections = &loop->core->connections;

    for (int i = 0; i < connections->length; i++) {
        xps_connection_t* connection = connections->data[i];

        if (connection == NULL)
            continue;

        if (connection->read_ready == true)
            connection->recv_handler(connection);

        if (connections->data[i] == NULL)
            continue;

        if (connection->write_ready == true && connection->write_buff_list->len > 0)
            connection->send_handler(connection);
    }

    for (int i = 0; i < connections->length; i++) {
        xps_connection_t* connection = connections->data[i];
        
        if (connection == NULL)
            continue;

        if (connection->read_ready == true)
            return true;

        if (connection->write_ready == true && connection->write_buff_list->len > 0)
            return true;
    }

    return false;
}