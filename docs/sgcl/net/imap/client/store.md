[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::store, async_store

```cpp
expected<vector<uint32_t>, io::error> store(const sequence_set& uids, store_mode mode, const vector<string>& flags,         // (1)
                                            uint64_t unchanged_since = 0) const;
async::task<expected<vector<uint32_t>, io::error>> async_store(sequence_set uids, store_mode mode, vector<string> flags,    // (2)
                                                               uint64_t unchanged_since = 0) const noexcept;
```

Sends UID STORE (`.SILENT`): the flags of the messages replaced, added to or removed from. With `unchanged_since`
(CONDSTORE, RFC 7162) only the messages whose mod-sequence is not past it change, the rest named back.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `uids` | the messages |
| `mode` | [store_mode](../store_mode.md) |
| `flags` | the flags: system flags and keywords (atoms) |
| `unchanged_since` | a mod-sequence; 0, the default, none |

## Return value

The UIDs left alone for having changed past `unchanged_since` (MODIFIED); or the error, `std::errc::invalid_argument`
for a flag that is not an atom, before anything is sent.

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
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
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
    net::imap::selected box = session.select("INBOX");
    session.store(1, net::imap::store_mode::add, {"$Work"});
    auto modified = session.store(1, net::imap::store_mode::add, {net::imap::flag::seen}, box.highest_modseq);
    println("{}", *modified);   // changed since: left alone
    srv.close();
}
```

Output:

```text
[1]
```

## See also

- [add_flags](add_flags.md), [remove_flags](remove_flags.md), [set_flags](set_flags.md)
- [sgcl::net::imap::client](README.md)
