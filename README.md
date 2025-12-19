# VenomMemory

مكتبة تجريدية عالية الأداء للتواصل بين العمليات (IPC) باستخدام الذاكرة المشتركة.

**High-performance shared memory IPC library for Linux.**

## ✨ Features

- **Ultra-low latency**: ~5µs round-trip time
- **High throughput**: 150,000+ requests/second
- **Flexible buffers**: Configure sizes per daemon
- **Cross-process**: Uses futex for efficient signaling
- **Simple API**: Single header include, easy to use

## 📊 Performance

| Message Size | Avg Latency | Throughput |
|-------------|-------------|------------|
| 64 bytes    | 5.17 µs     | 193K req/s |
| 1 KB        | 5.18 µs     | 193K req/s |
| 4 KB        | 6.34 µs     | 158K req/s |
| 16 KB       | 7.01 µs     | 143K req/s |

## 🚀 Quick Start

### Building

```bash
mkdir build && cd build
cmake ..
make
```

### Daemon (Server)

```c
#include <venom_memory.h>

int handle_cmd(VenomChannel *ch, const void *cmd, size_t size, void *ud) {
    // Process command...
    venom_send_response(ch, "OK", 2);
    return 0;
}

int main() {
    VenomChannelConfig cfg = {
        .command_buffer_size = 4096,
        .response_buffer_size = 65536,
        .default_timeout_ms = 0
    };
    
    VenomChannel *ch = venom_channel_create("my_daemon", &cfg);
    venom_channel_listen(ch, handle_cmd, NULL);
    venom_channel_destroy(ch);
}
```

### Shell (Client)

```c
#include <venom_memory.h>

int main() {
    VenomChannel *ch = venom_channel_connect("my_daemon");
    
    char response[256];
    size_t resp_size = sizeof(response);
    
    venom_request(ch, "ping", 4, response, &resp_size, 5000);
    
    venom_channel_disconnect(ch);
}
```

## 📁 Project Structure

```
VenomMemory/
├── include/           # Public headers
│   ├── venom_memory.h # Single include
│   ├── venom_channel.h
│   ├── venom_shm.h
│   └── venom_sync.h
├── src/               # Implementation
├── examples/          # Example programs
└── tests/             # Unit tests & benchmarks
```

## 🔧 API Reference

### Daemon API

| Function | Description |
|----------|-------------|
| `venom_channel_create(namespace, config)` | Create channel |
| `venom_channel_listen(ch, handler, userdata)` | Listen for commands |
| `venom_send_response(ch, data, size)` | Send response |
| `venom_channel_destroy(ch)` | Cleanup |

### Shell API

| Function | Description |
|----------|-------------|
| `venom_channel_connect(namespace)` | Connect to daemon |
| `venom_send_command(ch, cmd, size)` | Send command |
| `venom_wait_response(ch, buf, size, recv, timeout)` | Wait for response |
| `venom_request(ch, cmd, size, buf, resp_size, timeout)` | Send & wait |
| `venom_channel_disconnect(ch)` | Disconnect |

## 📄 License

MIT License
# VenomMemory
