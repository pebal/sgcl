[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md) › [client](client/README.md) › options

# sgcl::net::mqtt::client::options

```cpp
#include "sgcl/net/mqtt/client.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    class client {
    public:
        struct options {
            string client_id;
            string user;
            string password;
            mqtt::version version = mqtt::version::v5;
            bool clean_start = true;
            duration session_expiry = {};
            duration keep_alive = 60 * second;
            optional<message> will;
            duration will_delay = {};
            uint16_t receive_maximum = 65535;
            uint32_t maximum_packet_size = 0;
            uint16_t topic_alias_maximum = 16;
            bool topic_aliases = true;
            size_t max_received = 10000;
            net::tls::config tls;
            duration timeout = 30 * second;
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::mqtt::client::options` is how a [client](client/README.md) connects and what it asks of the broker, the
argument of [connect](client/connect.md). The defaults are a session of its own that ends with the connection,
MQTT 5, a keep alive of a minute.

## Member objects

| Member | Description |
|---|---|
| `client_id` | the client id; empty by default: the broker assigns one (MQTT 5), one is made here (3.1.1) |
| `user`, `password` | the credentials; empty: the URL's, or none |
| `version` | the protocol's [version](version.md); `v5` by default |
| `clean_start` | a new session, the one the broker kept of the client id dropped; `true` by default |
| `session_expiry` | how long the broker keeps the session after the connection ends (MQTT 5); zero by default: it ends with it |
| `keep_alive` | the longest silence between packets, a PINGREQ sent past three quarters of it; 60 s by default; zero: none |
| `will` | a [message](message/README.md) the broker publishes when the connection ends without [disconnect](client/disconnect.md); none by default |
| `will_delay` | how long the broker waits before the will (MQTT 5); a session back within it sends no will; zero by default |
| `receive_maximum` | the QoS 1 and 2 messages the broker may have in flight to the client; 65535 by default |
| `maximum_packet_size` | the largest packet the client takes, larger ones not sent to it; 0 by default: no limit |
| `topic_alias_maximum` | the aliases the broker may use toward the client; 16 by default |
| `topic_aliases` | aliases used toward the broker, as many as it allows; `true` by default |
| `max_received` | messages kept until [receive](client/receive.md), past it the reader waits; 10000 by default |
| `tls` | the [tls::config](../tls/config.md) of `mqtts://` and `wss://`; the server's name the URL's host when none is set |
| `timeout` | the connection and CONNACK, then each acknowledgement's wait; 30 s by default; zero: none |
| `stop` | ends the dial with `ECANCELED` |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::broker b;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(b.async_serve(l));
    string url = string::concat("mqtt://", l.local_endpoint().to_string());
    net::mqtt::client::options o;
    o.client_id = "thermostat";
    o.keep_alive = std::chrono::seconds(20);
    o.will = net::mqtt::message("devices/thermostat", "lost", net::mqtt::qos::at_least_once, true);
    net::mqtt::client c = net::mqtt::client::connect(url, o).value();
    println("{}", c.client_id());
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
thermostat
```

## See also

- [connect](client/connect.md)
- [client](client/README.md)
