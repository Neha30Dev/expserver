#include "xps_connection.h"

void connection_loop_read_handler(void *ptr);

void connection_loop_write_handler(void *ptr);

void connection_loop_close_handler(void *ptr);

void connection_read_handler(void *ptr);

void connection_write_handler(void *ptr);

xps_connection_t *xps_connection_create(xps_core_t *core, u_int sock_fd) {

    xps_connection_t *connection = malloc(sizeof(xps_connection_t));
    if (connection == NULL) {
        logger(LOG_ERROR, "xps_connection_create()", "malloc() failed for 'connection'");
        return NULL;
    }

    // Init values
    connection->core = core;
    connection->sock_fd = sock_fd;
    connection->listener = NULL;
    connection->remote_ip = get_remote_ip(sock_fd);
    connection->write_buff_list = xps_buffer_list_create();
    connection->read_ready = false;
    connection->write_ready = false;
    connection->send_handler = connection_write_handler;
    connection->recv_handler = connection_read_handler;

    xps_loop_attach(core->loop, sock_fd, EPOLLIN | EPOLLOUT | EPOLLET, connection, connection_loop_read_handler, connection_loop_write_handler, connection_loop_close_handler);

    vec_push(&core->connections, connection);

    logger(LOG_DEBUG, "xps_connection_create()", "created connection");
    return connection;

}

void xps_connection_destroy(xps_connection_t *connection) {

    /* validate params */
    assert(connection != NULL);

    vec_void_t *connections = &connection->core->connections;

    // Set connection to NULL in 'connections' list
    for (int i = 0; i < connections->length; i++) {
        xps_connection_t *curr = connections->data[i];
        if (curr == connection) {
            connections->data[i] = NULL;  
        }
    }

    /* detach connection from loop */
    xps_loop_detach(connection->core->loop, connection->sock_fd);

    /* close connection socket FD */
    close(connection->sock_fd);

    /* free connection->remote_ip */
    free(connection->remote_ip);

    // Free all buffers in write_buff_list
    if (connection->write_buff_list) {
        xps_buffer_list_destroy(connection->write_buff_list);
        connection->write_buff_list = NULL;
    }

    /* free connection instance */
    free(connection);

    logger(LOG_DEBUG, "xps_connection_destroy()", "destroyed connection");

}

void connection_read_handler(void *ptr) {

    assert(ptr != NULL);

    xps_connection_t *connection = ptr;

        if (!connection->write_buff_list) return;
    

    /* validate params */
    u_char buff[4096];

    long read_n = recv(connection->sock_fd, buff, sizeof(buff)-1, 0);


    if (read_n < 0) {
        logger(LOG_ERROR, "xps_connection_read_handler()", "recv() failed");
        perror("Error message");
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            connection->read_ready = false;
            return;
        }
        else {
            xps_connection_destroy(connection);
            return;
        }
    }

    if (read_n == 0) {
        logger(LOG_INFO, "connection_read_handler()", "peer closed connection");
        xps_connection_destroy(connection);
        return;
    }

    buff[read_n] = '\0';

    if (read_n > 0 && buff[read_n - 1] == '\n') {
        read_n--;
        buff[read_n] = '\0';
    }

    /* print client message */
    printf("[CLIENT MESSAGE] %s\n", buff);

    /* reverse client message */
    for (int start = 0, end = read_n-1; start < end; start++, end--) {
        char temp = buff[start];
        buff[start] = buff[end];
        buff[end] = temp;
    }

    if (read_n < sizeof(buff) - 1) {
        buff[read_n] = '\n';
        read_n++;
    }

    xps_buffer_t *buffer = xps_buffer_create(read_n,read_n,NULL);
    memcpy(buffer->data, buff, read_n);
    xps_buffer_list_append(connection->write_buff_list,buffer);

    //logger(LOG_INFO, "connection_write_handler()", "writing");

}

void connection_write_handler(void *ptr) {

    assert(ptr != NULL);

    xps_connection_t *connection = ptr;


    if (!connection->write_buff_list || connection->write_buff_list->len == 0)
        return;

        
    xps_buffer_t *buffer = xps_buffer_list_read(connection->write_buff_list,connection->write_buff_list->len);

    if (!buffer) return;

    long bytes_written = 0;
    long message_len = buffer->len;
    while (bytes_written < message_len) {
        long write_n =  send(connection->sock_fd, buffer->data+bytes_written, message_len-bytes_written, 0);
        if (write_n == -1) {
            logger(LOG_ERROR, "xps_connection_read_handler()", "send() failed");
            perror("Error message");
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                connection->write_ready = false;
                break;
            }
            else {
                xps_connection_destroy(connection);
                xps_buffer_destroy(buffer);
                return;
            }
        }
        bytes_written += write_n;
        xps_buffer_list_clear(connection->write_buff_list, write_n);
    }
    xps_buffer_destroy(buffer);
}

void connection_loop_close_handler(void *ptr) {

    assert(ptr != NULL);

    xps_connection_t *connection = ptr;

    xps_connection_destroy(connection);
}

void connection_loop_read_handler(void* ptr) {
    assert(ptr != NULL);
    xps_connection_t *connection = ptr;
	connection->read_ready=true;
}

void connection_loop_write_handler(void* ptr) {
    assert(ptr != NULL);
    xps_connection_t *connection = ptr;
    connection->write_ready=true;
}