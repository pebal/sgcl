# sgcl::net::http::headers

```cpp
#include "sgcl/net/http/headers.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class headers;   // the fields of a head: (name, value) in their order
}
```

The fields of a head: a list of names and values in the order of the wire, a name as often as it comes (`Set-Cookie`). Names are compared without regard to ASCII case and kept as they were written: there is no canonical form (Go turns `content-type` into `Content-Type`, which costs a string a field; HTTP/2 writes names in lower case anyway). A lookup walks the list: a head is a handful of fields, bounded by the server's limit, and there is no hash to be steered by whoever sends it.

## Rules

- The fields of a received head are slices of the one string the head was copied into: nothing is allocated a field. `get` makes the string it returns (`""` when there is none; `contains` tells the two apart), and so does the iteration, a `pair<string, string>` for each field.
- `set` puts the value in the place of the first field of the name and drops the others; `add` appends.
- **Dates** (`Date`, `Last-Modified`, `If-Modified-Since`, `Expires`) are [`time::datetime`](../../time/README.md) values: `date(name)` reads the three forms of RFC 9110 §5.6.7 (IMF-fixdate, and the obsolete RFC 850 and asctime a recipient must take) into UTC, `nullopt` when there is none or it is not a date; `set_date(name, t)` writes IMF-fixdate, always GMT, whatever `t`'s zone. The format is the `time` module's (`time::http`), the only one in the library.
- A name the program gives must be a token of RFC 9110 (`invalid_argument` otherwise); a value has CR, LF and NUL made spaces, since values often come from users and an exception in the path of a request would be worse than the change (Go does the same).

## Members

```cpp
headers();
string get(const string& name) const;
vector<string> get_all(const string& name) const;
bool contains(const string& name) const;
headers& set(const string& name, const string& value);
headers& add(const string& name, const string& value);
headers& erase(const string& name);
optional<time::datetime> date(const string& name) const;           // RFC 9110 §5.6.7, in UTC
headers& set_date(const string& name, const time::datetime& t);    // "Sun, 06 Nov 1994 08:49:37 GMT"
size_t size() const noexcept;
bool empty() const noexcept;
iterator begin() const noexcept;   // pair<string, string>, in order
iterator end() const noexcept;
```

## Example

```cpp
#include "sgcl/net/http/headers.h"
#include <iostream>

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Set-Cookie", "a=1").add("set-cookie", "b=2").set("Content-Type", "text/plain");
    std::cout << h.get("SET-COOKIE") << ' ' << h.get_all("Set-Cookie").size() << '\n';
    h.set("X-Name", "line one\r\nInjected: yes");
    h.set_date("Last-Modified", time::datetime::from_unix(784111777));
    std::cout << (h.date("last-modified") == time::datetime::from_unix(784111777)) << '\n';
    for (auto [name, value] : h) {
        std::cout << name << ": " << value << '\n';
    }
}
```

Output:

```text
a=1 2
1
Set-Cookie: a=1
set-cookie: b=2
Content-Type: text/plain
X-Name: line one  Injected: yes
Last-Modified: Sun, 06 Nov 1994 08:49:37 GMT
```

## See also

- [request](request.md), [response](response.md), [response_writer](response_writer.md): where headers are held
