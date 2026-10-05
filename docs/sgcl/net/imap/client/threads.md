[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::threads, async_threads

```cpp
expected<vector<thread>, io::error> threads(const criteria& c = criteria(),                                                    // (1)
                                            threading algorithm = threading::references) const;
async::task<expected<vector<thread>, io::error>> async_threads(criteria c = criteria(),                                        // (2)
                                                               threading algorithm = threading::references) const noexcept;
```

Sends UID THREAD (RFC 5256): the messages the criteria take as threads, by the algorithm.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the criteria |
| `algorithm` | [threading](../threading.md): `references` by default |

## Return value

The [threads](../thread.md), oldest first; or the error, `errc::not_supported` for a server without the algorithm.

## Complexity

One round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Message-ID: <a@x>\r\nSubject: plan\r\n\r\nx\r\n");
    mail.append("alice", "INBOX", "Message-ID: <b@x>\r\nIn-Reply-To: <a@x>\r\nSubject: Re: plan\r\n\r\ny\r\n");
    net::imap::server srv;
    srv.backend = mail;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;   // the loopback, no TLS
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    session.select("INBOX");
    auto threads = session.threads();
    println("{} thread: {} <- {}", threads->size(), (*threads)[0].uid, (*threads)[0].children[0].uid);
    srv.close();
}
```

Output:

```text
1 thread: 1 <- 2
```

## See also

- [thread](../thread.md), [threading](../threading.md)
- [sgcl::net::imap::client](README.md)
