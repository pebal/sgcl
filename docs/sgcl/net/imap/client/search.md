[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::search, async_search

```cpp
expected<vector<uint32_t>, io::error> search(const criteria& c = criteria()) const;                         // (1)
expected<vector<uint32_t>, io::error> search(const string& keys) const;                                     // (2)
async::task<expected<vector<uint32_t>, io::error>> async_search(criteria c = criteria()) const noexcept;    // (3)
async::task<expected<vector<uint32_t>, io::error>> async_search(string keys) const noexcept;                // (4)
```

Sends UID SEARCH: the UIDs of the messages of the selected mailbox the criteria take. Strings with 8-bit text go with
CHARSET UTF-8 to a server without UTF-8 of its own; a server of IMAP4rev2 answers ESEARCH, read the same.

- (1, 3) [criteria](../criteria/README.md): `net::imap::criteria::unseen() && net::imap::criteria::from("bob")`.
- (2, 4) IMAP's search keys as written, `"UNSEEN FROM bob"`: what the class has no function for.
- (1, 2) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of
  the program, never a worker.
- (3, 4) Return a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the criteria; every message by default |
| `keys` | the search keys, RFC 9051 §6.4.4 |

## Return value

The UIDs, ascending; or the error, `errc::bad` for keys the server does not take.

## Complexity

One round trip.

## Exceptions

- (1, 2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3, 4) None.

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
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nx\r\n");
    mail.append("alice", "INBOX", "From: Carol <carol@example.com>\r\nSubject: Report\r\n\r\ny\r\n");
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
    auto from_bob = session.search(net::imap::criteria::from("bob"));
    println("{}", *from_bob);
    auto text = session.search("OR SUBJECT lunch SUBJECT report");
    println("{}", *text);
    srv.close();
}
```

Output:

```text
[1]
[1, 2]
```

## See also

- [criteria](../criteria/README.md)
- [count](count.md), [sort](sort.md), [fetch](fetch.md)
- [sgcl::net::imap::client](README.md)
