/**
 * wSocket C SDK — Implementation
 *
 * @file wsocket.c
 * @brief Core client, channel, presence, and push implementation.
 */

#include "wsocket.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <curl/curl.h>

/* ─── Internal cJSON (minimal inline) ────────────────────── */
/* For a full build, link against cJSON library or include cJSON.c */

/* We use a minimal JSON helper here; for production link cJSON */
static char *json_obj_2(const char *k1, const char *v1, const char *k2, const char *v2) {
    size_t len = strlen(k1) + strlen(v1) + strlen(k2) + strlen(v2) + 32;
    char *buf = (char *)malloc(len);
    snprintf(buf, len, "{\"%s\":\"%s\",\"%s\":\"%s\"}", k1, v1, k2, v2);
    return buf;
}

/* ─── Internal Structures ────────────────────────────────── */

#define MAX_CHANNELS 256

typedef struct wsocket_channel {
    char name[256];
    wsocket_message_cb message_cb;
    void *message_ud;
    wsocket_history_cb history_cb;
    void *history_ud;
    wsocket_presence_cb on_enter_cb;
    void *on_enter_ud;
    wsocket_presence_cb on_leave_cb;
    void *on_leave_ud;
    wsocket_presence_cb on_update_cb;
    void *on_update_ud;
    wsocket_members_cb on_members_cb;
    void *on_members_ud;
} wsocket_channel_t_impl;

struct wsocket_client {
    char url[1024];
    char api_key[256];
    wsocket_options_t options;
    int connected;
    int reconnect_attempts;
    int64_t last_message_ts;

    wsocket_channel_t_impl channels[MAX_CHANNELS];
    int channel_count;
    char subscribed[MAX_CHANNELS][256];
    int subscribed_count;

    wsocket_connect_cb on_connect_cb;
    void *on_connect_ud;
    wsocket_disconnect_cb on_disconnect_cb;
    void *on_disconnect_ud;
    wsocket_error_cb on_error_cb;
    void *on_error_ud;

    /* WebSocket handle (platform-specific) */
    void *ws_handle;
    pthread_t recv_thread;
    int running;
};

struct wsocket_push {
    char base_url[512];
    char token[256];
    char app_id[128];
};

/* ─── Client ─────────────────────────────────────────────── */

wsocket_client_t *wsocket_create(const char *url, const char *api_key,
                                  const wsocket_options_t *options) {
    wsocket_client_t *c = (wsocket_client_t *)calloc(1, sizeof(wsocket_client_t));
    if (!c) return NULL;

    strncpy(c->url, url, sizeof(c->url) - 1);
    strncpy(c->api_key, api_key, sizeof(c->api_key) - 1);

    if (options) {
        c->options = *options;
    } else {
        c->options.auto_reconnect = 1;
        c->options.max_reconnect_attempts = 10;
        c->options.reconnect_delay_ms = 1000;
        c->options.token = NULL;
        c->options.recover = 1;
    }

    c->connected = 0;
    c->reconnect_attempts = 0;
    c->last_message_ts = 0;
    c->channel_count = 0;
    c->subscribed_count = 0;
    c->running = 0;
    c->ws_handle = NULL;

    return c;
}

void wsocket_destroy(wsocket_client_t *client) {
    if (!client) return;
    wsocket_disconnect(client);
    free(client);
}

int wsocket_connect(wsocket_client_t *client) {
    if (!client) return -1;

    /* Build WebSocket URL with query params */
    char ws_url[2048];
    const char *sep = strchr(client->url, '?') ? "&" : "?";
    snprintf(ws_url, sizeof(ws_url), "%s%skey=%s", client->url, sep, client->api_key);
    if (client->options.token) {
        char tmp[2048];
        snprintf(tmp, sizeof(tmp), "%s&token=%s", ws_url, client->options.token);
        strncpy(ws_url, tmp, sizeof(ws_url) - 1);
    }

    /*
     * NOTE: WebSocket connection implementation depends on the library used.
     * With libwebsockets, you would create an lws_context and connect here.
     * This is a stub implementation for the SDK structure.
     */

    client->connected = 1;
    client->reconnect_attempts = 0;

    if (client->on_connect_cb) {
        client->on_connect_cb(client->on_connect_ud);
    }

    /* Re-subscribe to channels */
    for (int i = 0; i < client->subscribed_count; i++) {
        /* Send subscribe message for each */
    }

    return 0;
}

void wsocket_disconnect(wsocket_client_t *client) {
    if (!client) return;
    client->connected = 0;
    client->running = 0;
}

void wsocket_run(wsocket_client_t *client) {
    if (!client) return;
    client->running = 1;
    while (client->running && client->connected) {
        /* Event loop — process WebSocket frames */
        usleep(10000); /* 10ms tick */
    }
}

int wsocket_is_connected(wsocket_client_t *client) {
    return client ? client->connected : 0;
}

void wsocket_on_connect(wsocket_client_t *client, wsocket_connect_cb cb, void *userdata) {
    if (!client) return;
    client->on_connect_cb = cb;
    client->on_connect_ud = userdata;
}

void wsocket_on_disconnect(wsocket_client_t *client, wsocket_disconnect_cb cb, void *userdata) {
    if (!client) return;
    client->on_disconnect_cb = cb;
    client->on_disconnect_ud = userdata;
}

void wsocket_on_error(wsocket_client_t *client, wsocket_error_cb cb, void *userdata) {
    if (!client) return;
    client->on_error_cb = cb;
    client->on_error_ud = userdata;
}

/* ─── Channel ────────────────────────────────────────────── */

static wsocket_channel_t_impl *find_or_create_channel(wsocket_client_t *client, const char *name) {
    for (int i = 0; i < client->channel_count; i++) {
        if (strcmp(client->channels[i].name, name) == 0) {
            return &client->channels[i];
        }
    }
    if (client->channel_count >= MAX_CHANNELS) return NULL;
    wsocket_channel_t_impl *ch = &client->channels[client->channel_count++];
    memset(ch, 0, sizeof(*ch));
    strncpy(ch->name, name, sizeof(ch->name) - 1);
    return ch;
}

wsocket_channel_t *wsocket_channel(wsocket_client_t *client, const char *name) {
    if (!client || !name) return NULL;
    return (wsocket_channel_t *)find_or_create_channel(client, name);
}

void wsocket_subscribe(wsocket_channel_t *ch, wsocket_message_cb cb, void *userdata) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    impl->message_cb = cb;
    impl->message_ud = userdata;
    /* Send subscribe JSON via WebSocket */
}

void wsocket_unsubscribe(wsocket_channel_t *ch) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    impl->message_cb = NULL;
    impl->message_ud = NULL;
    /* Send unsubscribe JSON via WebSocket */
}

void wsocket_publish(wsocket_channel_t *ch, const char *data, int persist) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl || !data) return;
    /* Build and send publish JSON via WebSocket */
    (void)persist;
}

void wsocket_history(wsocket_channel_t *ch, int limit, wsocket_history_cb cb, void *userdata) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    impl->history_cb = cb;
    impl->history_ud = userdata;
    /* Send history request JSON via WebSocket */
    (void)limit;
}

/* ─── Presence ───────────────────────────────────────────── */

void wsocket_presence_enter(wsocket_channel_t *ch, const char *data) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    (void)data;
    /* Send presence.enter JSON */
}

void wsocket_presence_leave(wsocket_channel_t *ch) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    /* Send presence.leave JSON */
}

void wsocket_presence_update(wsocket_channel_t *ch, const char *data) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    (void)data;
    /* Send presence.update JSON */
}

void wsocket_presence_get(wsocket_channel_t *ch) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    /* Send presence.get JSON */
}

void wsocket_presence_on_enter(wsocket_channel_t *ch, wsocket_presence_cb cb, void *userdata) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    impl->on_enter_cb = cb;
    impl->on_enter_ud = userdata;
}

void wsocket_presence_on_leave(wsocket_channel_t *ch, wsocket_presence_cb cb, void *userdata) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    impl->on_leave_cb = cb;
    impl->on_leave_ud = userdata;
}

void wsocket_presence_on_update(wsocket_channel_t *ch, wsocket_presence_cb cb, void *userdata) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    impl->on_update_cb = cb;
    impl->on_update_ud = userdata;
}

void wsocket_presence_on_members(wsocket_channel_t *ch, wsocket_members_cb cb, void *userdata) {
    wsocket_channel_t_impl *impl = (wsocket_channel_t_impl *)ch;
    if (!impl) return;
    impl->on_members_cb = cb;
    impl->on_members_ud = userdata;
}

/* ─── Push Client ────────────────────────────────────────── */

wsocket_push_t *wsocket_push_create(const char *base_url, const char *token, const char *app_id) {
    wsocket_push_t *p = (wsocket_push_t *)calloc(1, sizeof(wsocket_push_t));
    if (!p) return NULL;
    strncpy(p->base_url, base_url, sizeof(p->base_url) - 1);
    strncpy(p->token, token, sizeof(p->token) - 1);
    strncpy(p->app_id, app_id, sizeof(p->app_id) - 1);
    return p;
}

void wsocket_push_destroy(wsocket_push_t *push) {
    free(push);
}

/* ─── Response buffer for GET requests ───────────────────── */

typedef struct {
    char *data;
    size_t size;
} response_buf_t;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    response_buf_t *buf = (response_buf_t *)userp;
    char *tmp = (char *)realloc(buf->data, buf->size + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

static struct curl_slist *push_headers(wsocket_push_t *push) {
    struct curl_slist *headers = NULL;
    char auth_header[600];
    char app_header[300];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", push->token);
    snprintf(app_header, sizeof(app_header), "X-App-Id: %s", push->app_id);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, app_header);
    return headers;
}

static int push_post(wsocket_push_t *push, const char *path, const char *body) {
    if (!push || !path || !body) return -1;

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/push/%s", push->base_url, path);

    struct curl_slist *headers = push_headers(push);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}

static int push_request(wsocket_push_t *push, const char *method, const char *full_url,
                        char *out_buf, size_t buf_size) {
    if (!push || !method || !full_url) return -1;

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist *headers = push_headers(push);
    response_buf_t resp = {NULL, 0};

    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK && out_buf && buf_size > 0 && resp.data) {
        strncpy(out_buf, resp.data, buf_size - 1);
        out_buf[buf_size - 1] = '\0';
    }

    free(resp.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}

int wsocket_push_register_fcm(wsocket_push_t *push, const char *device_token, const char *member_id) {
    char body[1024];
    snprintf(body, sizeof(body),
        "{\"memberId\":\"%s\",\"platform\":\"fcm\",\"subscription\":{\"deviceToken\":\"%s\"}}",
        member_id, device_token);
    return push_post(push, "register", body);
}

int wsocket_push_register_apns(wsocket_push_t *push, const char *device_token, const char *member_id) {
    char body[1024];
    snprintf(body, sizeof(body),
        "{\"memberId\":\"%s\",\"platform\":\"apns\",\"subscription\":{\"deviceToken\":\"%s\"}}",
        member_id, device_token);
    return push_post(push, "register", body);
}

int wsocket_push_send(wsocket_push_t *push, const char *member_id, const char *payload) {
    char body[2048];
    snprintf(body, sizeof(body),
        "{\"memberId\":\"%s\",\"payload\":%s}", member_id, payload);
    return push_post(push, "send", body);
}

int wsocket_push_broadcast(wsocket_push_t *push, const char *payload) {
    char body[2048];
    snprintf(body, sizeof(body), "{\"payload\":%s}", payload);
    return push_post(push, "broadcast", body);
}

int wsocket_push_unregister(wsocket_push_t *push, const char *member_id, const char *platform) {
    char body[1024];
    if (platform && strlen(platform) > 0) {
        snprintf(body, sizeof(body),
            "{\"memberId\":\"%s\",\"platform\":\"%s\"}", member_id, platform);
    } else {
        snprintf(body, sizeof(body), "{\"memberId\":\"%s\"}", member_id);
    }
    return push_post(push, "unregister", body);
}

int wsocket_push_delete_subscription(wsocket_push_t *push, const char *subscription_id) {
    if (!push || !subscription_id) return -1;
    char url[1024];
    snprintf(url, sizeof(url), "%s/api/push/subscriptions/%s", push->base_url, subscription_id);
    return push_request(push, "DELETE", url, NULL, 0);
}

int wsocket_push_add_channel(wsocket_push_t *push, const char *subscription_id, const char *channel) {
    if (!push || !subscription_id || !channel) return -1;
    char body[1024];
    snprintf(body, sizeof(body), "{\"subscriptionId\":\"%s\",\"channel\":\"%s\"}", subscription_id, channel);
    return push_post(push, "channels/add", body);
}

int wsocket_push_remove_channel(wsocket_push_t *push, const char *subscription_id, const char *channel) {
    if (!push || !subscription_id || !channel) return -1;
    char body[1024];
    snprintf(body, sizeof(body), "{\"subscriptionId\":\"%s\",\"channel\":\"%s\"}", subscription_id, channel);
    return push_post(push, "channels/remove", body);
}

int wsocket_push_get_vapid_key(wsocket_push_t *push, char *out_buf, size_t buf_size) {
    if (!push || !out_buf || buf_size == 0) return -1;
    char url[1024];
    snprintf(url, sizeof(url), "%s/api/push/vapid-key", push->base_url);
    return push_request(push, "GET", url, out_buf, buf_size);
}

int wsocket_push_list_subscriptions(wsocket_push_t *push, const char *member_id,
                                     char *out_buf, size_t buf_size) {
    if (!push || !member_id || !out_buf || buf_size == 0) return -1;
    char url[1024];
    snprintf(url, sizeof(url), "%s/api/push/subscriptions?memberId=%s", push->base_url, member_id);
    return push_request(push, "GET", url, out_buf, buf_size);
}
