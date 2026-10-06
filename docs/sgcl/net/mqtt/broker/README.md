[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md)

# sgcl::net::mqtt::broker

```cpp
#include "sgcl/net/mqtt/broker.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    class broker;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::mqtt::broker` is an MQTT broker of both versions: clients connect over TCP ([serve](serve.md)), TLS
([serve_tls](serve_tls.md)) or a WebSocket route of an [http::server](../../http/server/README.md)
([accept](accept.md)), subscribe and publish; each message goes to the sessions whose filters match its topic, at
the lower of its QoS and the subscription's. Mosquitto's role, in a program of its own.

## Rules

- A handle of one word: copies share the sessions. The fields are read when `serve` is called.
- A session lives in memory for its expiry after its connection (at most `max_session_expiry`), with its
  subscriptions and the messages queued for it (at most `max_queued`; past it a QoS 0 message makes room, else the
  new one is dropped). A client of the same id takes a session over: the old connection is told 0x8E and closed.
- Retained messages are kept in memory, one per topic; an empty retained message deletes it. A will goes out when a
  connection ends without DISCONNECT, after its delay unless the session comes back first.
- A shared subscription (`$share/<group>/<filter>`) gets each message once per group, its members in turn, an
  online one first.
- A client past a limit is told DISCONNECT with the reason (0x81 malformed, 0x82 protocol error, 0x93 receive
  maximum exceeded, 0x95 packet too large, 0x9B QoS not supported) and closed; one silent past one and a half
  times its keep alive is told 0x8D.
- MQTT 5's enhanced authentication (AUTH) is refused with 0x8C, bad authentication method.

## Member objects

| Member | Description |
|---|---|
| `authenticate` | a `function<bool(const string& client_id, const string& user, const string& password)>`: CONNECT checked; none by default: everyone |
| `authorize` | a `function<bool(const string& client_id, const string& user, const string& topic, bool subscribe)>`: each publication and subscription checked, 0x87 for a refusal; none by default: everything |
| `maximum_qos` | the most QoS taken and granted; `exactly_once` by default |
| `retain_available` | retained messages kept; `true` by default |
| `maximum_packet_size` | the largest packet taken; 1 MB by default |
| `receive_maximum` | QoS 2 publications a client may have in flight to the broker; 1024 by default |
| `topic_alias_maximum` | aliases a client may use; 64 by default |
| `max_session_expiry` | the longest a session is kept after its connection; 24 hours by default |
| `max_keep_alive` | the longest keep alive taken, told to a client that asks more; zero by default: the client's |
| `max_queued` | messages queued per session while it is offline or slow; 1000 by default |
| `max_connections` | the connections served at once; zero by default: no limit |
| `on_error` | a `function<void(const string&)>`: an accept's failure; a line on stderr by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](broker.md) | a broker, or a copy that shares one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another broker |
| [serve, async_serve](serve.md) | listens on an address, or serves a listener |
| [serve_tls, async_serve_tls](serve_tls.md) | listens with TLS from the first byte |
| [accept, async_accept](accept.md) | a WebSocket connection of an http::server's request |
| [publish](publish.md) | a message to the subscribers, as from a client |
| [shutdown, async_shutdown](shutdown.md) | gracefully: every connection told and closed |
| [close](close.md) | at once: every listener and connection closed |
| [connections](connections.md) | the connections being served |

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
    b.max_queued = 100;
    net::mqtt::client a = net::mqtt::client::connect(url).value();
    net::mqtt::client c = net::mqtt::client::connect(url).value();
    c.subscribe("$share/workers/jobs/+", net::mqtt::qos::at_least_once);
    a.publish("jobs/1", "resize photo", net::mqtt::qos::at_least_once);
    println("{} {}", c.receive()->text(), b.connections());
    a.disconnect();
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
resize photo 2
```

## See also

- [client](../client/README.md)
- [mqtt](../README.md)
