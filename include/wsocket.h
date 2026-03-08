/**
 * wSocket C SDK — Realtime Pub/Sub client with Presence, History, and Push.
 *
 * @file wsocket.h
 * @brief Public API for the wSocket C SDK.
 */

#ifndef WSOCKET_IO_H
#define WSOCKET_IO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Types ──────────────────────────────────────────────── */

typedef struct wsocket_client wsocket_client_t;
typedef struct wsocket_channel wsocket_channel_t;
typedef struct wsocket_push wsocket_push_t;

typedef struct {
    const char *id;
    const char *channel;
    int64_t timestamp;
} wsocket_meta_t;

typedef struct {
    const char *client_id;
    const char *data;      /* JSON string */
    int64_t joined_at;
} wsocket_presence_member_t;

typedef struct {
    const char *id;
    const char *channel;
    const char *data;       /* JSON string */
    const char *publisher_id;
    int64_t timestamp;
    int64_t sequence;
} wsocket_history_message_t;

typedef struct {
    const char *channel;
    wsocket_history_message_t *messages;
    int count;
    int has_more;
} wsocket_history_result_t;

typedef struct {
    int auto_reconnect;         /* default: 1 */
    int max_reconnect_attempts; /* default: 10 */
    int reconnect_delay_ms;     /* default: 1000 */
    const char *token;          /* auth token, nullable */
    int recover;                /* default: 1 */
} wsocket_options_t;

/* ─── Callback types ─────────────────────────────────────── */

typedef void (*wsocket_message_cb)(const char *data, wsocket_meta_t *meta, void *userdata);
typedef void (*wsocket_history_cb)(wsocket_history_result_t *result, void *userdata);
typedef void (*wsocket_presence_cb)(wsocket_presence_member_t *member, void *userdata);
typedef void (*wsocket_members_cb)(wsocket_presence_member_t *members, int count, void *userdata);
typedef void (*wsocket_connect_cb)(void *userdata);
typedef void (*wsocket_disconnect_cb)(int code, void *userdata);
typedef void (*wsocket_error_cb)(const char *error, void *userdata);

/* ─── Client API ─────────────────────────────────────────── */

/**
 * Create a new wSocket client.
 * @param url WebSocket server URL (ws:// or wss://)
 * @param api_key API key for authentication
 * @param options Connection options (NULL for defaults)
 * @return Client handle, or NULL on failure
 */
wsocket_client_t *wsocket_create(const char *url, const char *api_key,
                                  const wsocket_options_t *options);

/** Destroy client and free resources. */
void wsocket_destroy(wsocket_client_t *client);

/** Connect to the server. Returns 0 on success. */
int wsocket_connect(wsocket_client_t *client);

/** Disconnect from the server. */
void wsocket_disconnect(wsocket_client_t *client);

/** Run the event loop (blocking). */
void wsocket_run(wsocket_client_t *client);

/** Check if connected. */
int wsocket_is_connected(wsocket_client_t *client);

/** Set connect callback. */
void wsocket_on_connect(wsocket_client_t *client, wsocket_connect_cb cb, void *userdata);

/** Set disconnect callback. */
void wsocket_on_disconnect(wsocket_client_t *client, wsocket_disconnect_cb cb, void *userdata);

/** Set error callback. */
void wsocket_on_error(wsocket_client_t *client, wsocket_error_cb cb, void *userdata);

/* ─── Channel API ────────────────────────────────────────── */

/**
 * Get or create a channel.
 * @param client Client handle
 * @param name Channel name
 * @return Channel handle
 */
wsocket_channel_t *wsocket_channel(wsocket_client_t *client, const char *name);

/** Subscribe to channel messages. */
void wsocket_subscribe(wsocket_channel_t *ch, wsocket_message_cb cb, void *userdata);

/** Unsubscribe from channel. */
void wsocket_unsubscribe(wsocket_channel_t *ch);

/**
 * Publish data to channel.
 * @param ch Channel handle
 * @param data JSON string to publish
 * @param persist Whether to persist (1) or not (0)
 */
void wsocket_publish(wsocket_channel_t *ch, const char *data, int persist);

/** Request channel history. */
void wsocket_history(wsocket_channel_t *ch, int limit, wsocket_history_cb cb, void *userdata);

/* ─── Presence API ───────────────────────────────────────── */

/** Enter presence on channel. data is optional JSON (can be NULL). */
void wsocket_presence_enter(wsocket_channel_t *ch, const char *data);

/** Leave presence on channel. */
void wsocket_presence_leave(wsocket_channel_t *ch);

/** Update presence data. */
void wsocket_presence_update(wsocket_channel_t *ch, const char *data);

/** Request current presence members. */
void wsocket_presence_get(wsocket_channel_t *ch);

/** Set presence enter callback. */
void wsocket_presence_on_enter(wsocket_channel_t *ch, wsocket_presence_cb cb, void *userdata);

/** Set presence leave callback. */
void wsocket_presence_on_leave(wsocket_channel_t *ch, wsocket_presence_cb cb, void *userdata);

/** Set presence update callback. */
void wsocket_presence_on_update(wsocket_channel_t *ch, wsocket_presence_cb cb, void *userdata);

/** Set presence members callback. */
void wsocket_presence_on_members(wsocket_channel_t *ch, wsocket_members_cb cb, void *userdata);

/* ─── Push API ───────────────────────────────────────────── */

/** Create a push client for REST-based push notifications. */
wsocket_push_t *wsocket_push_create(const char *base_url, const char *token, const char *app_id);

/** Destroy push client. */
void wsocket_push_destroy(wsocket_push_t *push);

/** Register FCM device token. */
int wsocket_push_register_fcm(wsocket_push_t *push, const char *device_token, const char *member_id);

/** Register APNs device token. */
int wsocket_push_register_apns(wsocket_push_t *push, const char *device_token, const char *member_id);

/** Send push to a specific member. payload is JSON string. */
int wsocket_push_send(wsocket_push_t *push, const char *member_id, const char *payload);

/** Broadcast push to all. payload is JSON string. */
int wsocket_push_broadcast(wsocket_push_t *push, const char *payload);

/** Unregister a member's device. platform can be NULL. */
int wsocket_push_unregister(wsocket_push_t *push, const char *member_id, const char *platform);

/** Delete a specific push subscription by its ID. */
int wsocket_push_delete_subscription(wsocket_push_t *push, const char *subscription_id);

/** Add a channel to a push subscription. */
int wsocket_push_add_channel(wsocket_push_t *push, const char *subscription_id, const char *channel);

/** Remove a channel from a push subscription. */
int wsocket_push_remove_channel(wsocket_push_t *push, const char *subscription_id, const char *channel);

/** Get the VAPID public key. Returns 0 on success, writes response to out_buf. */
int wsocket_push_get_vapid_key(wsocket_push_t *push, char *out_buf, size_t buf_size);

/** List push subscriptions for a member. Returns 0 on success, writes JSON to out_buf. */
int wsocket_push_list_subscriptions(wsocket_push_t *push, const char *member_id,
                                     char *out_buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* WSOCKET_IO_H */
