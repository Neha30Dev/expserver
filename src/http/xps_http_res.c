#include "xps_http_res.h"

xps_http_res_t *xps_http_res_create(xps_core_t *core, u_int code) {
    assert(core!=NULL);

    xps_http_res_t* http_res= malloc(sizeof(xps_http_res_t));
    if (!http_res){
        logger(LOG_ERROR, "xps_http_res_create()",
           "failed to alloc memory for http_res. malloc() returned NULL");
        return NULL;
    }
    memset(http_res, 0, sizeof(xps_http_res_t));
    vec_init(&http_res->headers);
    http_res->body = NULL;

    const char *status_text;

    switch (code) {
        case HTTP_OK: status_text = "OK"; break;
        case HTTP_CREATED: status_text = "Created"; break;

        case HTTP_MOVED_PERMANENTLY: status_text = "Moved Permanently"; break;
        case HTTP_MOVED_TEMPORARILY: status_text = "Found"; break;
        case HTTP_NOT_MODIFIED: status_text = "Not Modified"; break;

        case HTTP_BAD_REQUEST: status_text = "Bad Request"; break;
        case HTTP_UNAUTHORIZED: status_text = "Unauthorized"; break;
        case HTTP_FORBIDDEN: status_text = "Forbidden"; break;
        case HTTP_NOT_FOUND: status_text = "Not Found"; break;

        case HTTP_INTERNAL_SERVER_ERROR: status_text = "Internal Server Error"; break;
        case HTTP_NOT_IMPLEMENTED: status_text = "Not Implemented"; break;
        case HTTP_BAD_GATEWAY: status_text = "Bad Gateway"; break;
        case HTTP_SERVICE_UNAVAILABLE: status_text = "Service Unavailable"; break;

        default: status_text = "OK"; break;
    }

    // ✅ response_line is fixed array
    snprintf(http_res->response_line,
            sizeof(http_res->response_line),
            "HTTP/1.1 %u %s", code, status_text);

    char time_buf[128];
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);

    strftime(time_buf, sizeof(time_buf),
            "%a, %d %b %Y %H:%M:%S GMT", gmt);

    xps_http_set_header(&(http_res->headers), "Date", time_buf);
    xps_http_set_header(&(http_res->headers), "Server", SERVER_NAME);
    xps_http_set_header(&(http_res->headers), "Access-Control-Allow-Origin", "*");
    
    return http_res;

}

void xps_http_res_destroy(xps_http_res_t *res){
    assert(res!=NULL);

    for(int i=0;i<res->headers.length;i++){
        xps_keyval_t *header = res->headers.data[i];
        if(header){
            if (header->key) free(header->key);
            if (header->val) free(header->val);
            free(header);
        }
    }

    vec_deinit(&res->headers);
    if(res->body){
        xps_buffer_destroy(res->body);
    }
    free(res);
}

xps_buffer_t *xps_http_res_serialize(xps_http_res_t *res){
    assert(res != NULL);

    xps_buffer_t *headers_str = xps_http_serialize_headers(&res->headers);
    if (headers_str == NULL) {
        logger(LOG_ERROR, "xps_http_res_serialize()", "failed to serialize headers");
        return NULL;
    }
    size_t body_len = (res->body != NULL) ? res->body->len : 0;

    size_t final_len = strlen(res->response_line) + 1 + headers_str->len + 1 + body_len; + 1;  
    xps_buffer_t *buff = xps_buffer_create(final_len,0,NULL);
    if (buff == NULL) {
        logger(LOG_ERROR, "xps_http_res_serialize()", "failed to create buffer instance");
        xps_buffer_destroy(headers_str);
        return NULL;
    }
    memcpy(buff->pos, res->response_line, strlen(res->response_line));
    buff->pos += strlen(res->response_line);
    memcpy(buff->pos, "\n",1);
    buff->pos += 1;
    memcpy(buff->pos, headers_str->data,headers_str->len);
    buff->pos += headers_str->len;
    memcpy(buff->pos, "\n",1);
    buff->pos += 1;
    if (res->body != NULL) {
        memcpy(buff->pos, res->body->data, res->body->len);
        buff->pos += res->body->len;
    }
    memcpy(buff->pos, "\n",1);
    buff->pos += 1;
    xps_buffer_destroy(headers_str);
    return buff;
}
