[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::headers

```cpp
#include "sgcl/net/http/headers.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class headers;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::headers` is the fields of a head, Go's `http.Header`: a list of names and values in the order of the
wire, a name as often as it comes (`Set-Cookie`). Names are compared without regard to ASCII case and kept as they were
written: there is no canonical form (Go turns `content-type` into `Content-Type`, which costs a string a field;
HTTP/2 writes names in lower case anyway). A lookup walks the list: a head is a handful of fields, bounded by the
server's limit, and there is no hash to be steered by whoever sends it.

A `headers` is a value, not a handle: a copy is a list of its own, and a moved-from one is empty. A [request](../request/README.md) and a
[response](../response/README.md) hold theirs and give a reference to it; a [response_writer](../response_writer/README.md) the same.
Dates (`Date`, `Last-Modified`, `If-Modified-Since`, `Expires`) are [time::datetime](../../../time/README.md) values,
read and written in the format of HTTP, the `time` module's `time::http`, the only one in the library (Go's
`http.TimeFormat` and `http.ParseTime`).

## Rules

- The fields of a received head are slices of the one string the head was copied into: nothing is allocated a field.
  [get](get.md) makes the string it returns (`""` when there is none; [contains](contains.md) tells
  the two apart), and so does the iteration, a `pair<string, string>` for each field.
- A name and a value are kept as the program gives them and checked where they are written: a name that is not a
  token of RFC 9110, or a value with CR, LF, NUL or another control, makes the client's send
  `std::errc::invalid_argument` before a byte is sent ([client](../client/README.md#rules)) and a handler's response a 500
  ([response_writer](../response_writer/README.md)). Values often come from users, and neither a split message nor an exception
  in the path of a request will do.
## Member types

| Type | Definition |
|---|---|
| `iterator` | an input iterator over the fields in their order; `*it` makes a `pair<string, string>` of the name and the value |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](headers.md) | constructs an empty list |
| `(destructor)` | drops the list |
| `operator=` | copies or moves another list |

#### Lookup

| Function | Description |
|---|---|
| [get](get.md) | the first value of a name |
| [get_all](get_all.md) | every value of a name |
| [contains](contains.md) | checks whether a name is there |
| [date](date.md) | the first value of a name read as a date |

#### Modifiers

| Function | Description |
|---|---|
| [set](set.md) | sets the value of a name, in the place of its first field |
| [add](add.md) | appends a field |
| [erase](erase.md) | drops every field of a name |
| [set_date](set_date.md) | sets the value of a name to a date |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the number of fields |
| [empty](empty.md) | checks whether there is no field |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first field |
| [end](end.md) | the iterator past the last field |

## Complexity

A lookup, [set](set.md) and [erase](erase.md): linear in the number of fields. [add](add.md):
constant, amortized.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Set-Cookie", "a=1").add("set-cookie", "b=2").set("Content-Type", "text/plain");
    h.set_date("Last-Modified", time::datetime::from_unix(784111777));
    println("{} {} {}", h.get("SET-COOKIE"), h.get_all("Set-Cookie").size(), h.size());
    for (auto [name, value] : h) {
        println("{}: {}", name, value);
    }
}
```

Output:

```text
a=1 2 4
Set-Cookie: a=1
set-cookie: b=2
Content-Type: text/plain
Last-Modified: Sun, 06 Nov 1994 08:49:37 GMT
```

## See also

- [request::headers](../request/headers.md), [response::headers](../response/headers.md): the fields of a message
- [cookie](../cookie/README.md): a `Set-Cookie` value written and read
- [response_writer](../response_writer/README.md): the fields a handler sends
