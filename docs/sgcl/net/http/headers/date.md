[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::date

```cpp
optional<time::datetime> date(const string& name) const noexcept;
```

Reads the first field named `name` as a date of HTTP (RFC 9110 §5.6.7), Go's `http.ParseTime`: the IMF-fixdate
(`Sun, 06 Nov 1994 08:49:37 GMT`) and the two obsolete forms a recipient must take, RFC 850
(`Sunday, 06-Nov-94 08:49:37 GMT`) and asctime (`Sun Nov  6 08:49:37 1994`), as a
[time::datetime](../../../time/README.md) in UTC. The fields that carry one are `Date`, `Last-Modified`,
`If-Modified-Since` and `Expires`. The format is the `time` module's `time::http`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field, in any case |

## Return value

The instant, in UTC; `nullopt` when there is no field of the name or its first value is not a date.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.set("Date", "Sun, 06 Nov 1994 08:49:37 GMT");
    h.set("Last-Modified", "Sunday, 06-Nov-94 08:49:37 GMT");
    h.set("Expires", "Sun Nov  6 08:49:37 1994");
    h.set("If-Modified-Since", "yesterday");
    for (const char* name : {"Date", "Last-Modified", "Expires"}) {
        println("{}", h.date(name)->format(time::rfc3339));
    }
    println("{} {}", h.date("If-Modified-Since").has_value(), h.date("Age").has_value());
}
```

Output:

```text
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
1994-11-06T08:49:37Z
false false
```

## See also

- [set_date](set_date.md): a date written
- [sgcl::net::http::headers](../headers.md)
