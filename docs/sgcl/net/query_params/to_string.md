[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::to_string

```cpp
string to_string() const noexcept;
```

The pairs written as `application/x-www-form-urlencoded`, in their order, Go's `Values.Encode` without its sort:
`name=value` joined by `&`, a space written as `+`, everything but the letters, the digits and `* - . _` escaped as
`%XX`. A pair whose value is empty is written with its `=`. The length of the text is tracked by the changes, so it is
written once into a string of that length; it is at most 512 MiB ([the limit](../query_params.md#rules)), but for
the pairs of a long query of a URL, which [url::query_params](../url/query_params.md) takes whole.

## Parameters

None.

## Return value

The text; the empty string for no pairs.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params params;
    params.add("q", "c++ & go");
    params.add("path", "/a~b");
    params.add("empty", "");
    println(params.to_string());
}
```

Output:

```text
q=c%2B%2B+%26+go&path=%2Fa%7Eb&empty=
```

## See also

- [parse](parse.md): the text read
- [url::with_query](../url/with_query.md): a URL with these pairs as its query
- [sgcl::net::query_params](../query_params.md)
