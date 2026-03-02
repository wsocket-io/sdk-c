# wSocket SDK for C

Official C SDK for wSocket — realtime pub/sub, presence, history, and push notifications.

## Installation

### CMake

```cmake
add_subdirectory(wsocket-io-c)
target_link_libraries(your_app wsocket_io)
```

### Manual

```bash
cd sdks/c
mkdir build && cd build
cmake ..
make
sudo make install
```

## Dependencies

- [libwebsockets](https://libwebsockets.org/) — WebSocket client
- [cJSON](https://github.com/DaveGamble/cJSON) — JSON parser
- [libcurl](https://curl.se/libcurl/) — HTTP client (for Push)

On Ubuntu/Debian:

```bash
sudo apt install libwebsockets-dev libcurl4-openssl-dev
```

## Quick Start

```c
#include "wsocket.h"

void on_message(const char *data, wsocket_meta_t *meta, void *userdata) {
    printf("Received on %s: %s\n", meta->channel, data);
}

int main() {
    wsocket_client_t *client = wsocket_create("wss://your-server.com", "your-api-key", NULL);

    wsocket_connect(client);

    wsocket_channel_t *ch = wsocket_channel(client, "chat");
    wsocket_subscribe(ch, on_message, NULL);
    wsocket_publish(ch, "{\"text\":\"Hello from C!\"}", 0);

    wsocket_run(client); // blocks, processes events

    wsocket_destroy(client);
    return 0;
}
```

## Presence

```c
void on_enter(wsocket_presence_member_t *member, void *userdata) {
    printf("%s entered\n", member->client_id);
}

wsocket_presence_on_enter(ch, on_enter, NULL);
wsocket_presence_enter(ch, "{\"name\":\"Alice\"}");
wsocket_presence_get(ch);
```

## History

```c
void on_history(wsocket_history_result_t *result, void *userdata) {
    for (int i = 0; i < result->count; i++) {
        printf("%s: %s\n", result->messages[i].publisher_id, result->messages[i].data);
    }
}

wsocket_history(ch, 50, on_history, NULL);
```

## Push Notifications

```c
wsocket_push_t *push = wsocket_push_create("https://your-server.com", "secret", "app1");

wsocket_push_register_fcm(push, "device-token", "user-123");
wsocket_push_send(push, "user-123", "{\"title\":\"Hello\",\"body\":\"World\"}");
wsocket_push_broadcast(push, "{\"title\":\"News\",\"body\":\"Update\"}");

wsocket_push_destroy(push);
```

## Requirements

- C11 or later
- libwebsockets 4.0+
- libcurl 7.0+

## License

MIT
