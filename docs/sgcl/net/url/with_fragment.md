[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::with_fragment

```cpp
expected<url, io::error> with_fragment(const string& fragment) const noexcept;
```

The URL with the fragment given, by the standard's hash setter: escaped with the standard's fragment set, a leading
`#` dropped; the empty string removes the fragment. The standard takes any fragment; the one refusal is [the
limit](README.md#rules) of 512 MiB the other setters keep.

## Parameters

| Parameter | Description |
|---|---|
| `fragment` | the new fragment, with its `#` or without it; empty for none |

## Return value

The new URL, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL fragment` and the value asked for, when the value is past 512 MiB or the URL would pass it (a byte
escaped is three).

## Complexity

Linear in the length of the URL and of the fragment.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("https://x/doc");
    println(u.with_fragment("#part 2")->to_string());
    println(u.with_fragment("top")->with_fragment("")->to_string());
}
```

Output:

```text
https://x/doc#part%202
https://x/doc
```

## See also

- [without_fragment](without_fragment.md): no fragment
- [fragment](fragment.md): the fragment
- [sgcl::net::url](README.md)
