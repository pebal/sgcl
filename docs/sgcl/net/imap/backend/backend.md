[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [backend](README.md)

# sgcl::net::imap::backend::backend

```cpp
backend();                                  // (1)
template<class B>                           // (2)
backend(B b);
backend(const backend& other) = default;    // (3)
```

1. A [memory_backend](../memory_backend/README.md) of its own.
2. The store given, taken by value: a memory_backend, a maildir_backend, or a type of the program's with the methods
   of [backend](README.md)'s rules (those marked optional where it has them). Takes part only when `B` is not
   `backend`.
3. The same store as `other`. A move is the copy.

## Parameters

| Parameter | Description |
|---|---|
| `b` | the store |
| `other` | another backend |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

A type of the program's: every method passed to a memory_backend, the appends counted.

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"
#include "sgcl/time.h"
#include <atomic>

using namespace sgcl;

int main() {
    struct counting {
        net::imap::memory_backend inner;
        tracked_ptr<std::atomic<int>> appends = make_tracked<std::atomic<int>>(0);

        bool authenticate(const string& u, const string& p) { return inner.authenticate(u, p); }
        auto mailboxes(const string& u) { return inner.mailboxes(u); }
        auto create(const string& u, const string& n, const string& s) { return inner.create(u, n, s); }
        auto remove(const string& u, const string& n) { return inner.remove(u, n); }
        auto rename(const string& u, const string& f, const string& t) { return inner.rename(u, f, t); }
        auto subscribe(const string& u, const string& n, bool on) { return inner.subscribe(u, n, on); }
        auto open(const string& u, const string& n) { return inner.open(u, n); }
        auto read(const string& u, const string& n, uint32_t uid) { return inner.read(u, n, uid); }
        auto store(const string& u, const string& n, const vector<net::imap::flag_update>& c) {
            return inner.store(u, n, c);
        }
        auto expunge(const string& u, const string& n, const vector<uint32_t>& uids) {
            return inner.expunge(u, n, uids);
        }
        auto append(const string& u, const string& n, const string& m, const vector<string>& f,
                    const time::datetime& d) {
            ++*appends;
            return inner.append(u, n, m, f, d);
        }
    };

    counting store;
    store.inner.add_user("alice", "secret");
    net::imap::server srv;
    srv.backend = store;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    session.append("INBOX", "Subject: x\r\n\r\ny\r\n");
    println("{} append", store.appends->load());
    srv.close();
}
```

Output:

```text
1 append
```

## See also

- [backend](README.md)
- [memory_backend](../memory_backend/README.md)
