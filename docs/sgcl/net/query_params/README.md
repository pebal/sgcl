[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::query_params

```cpp
#include "sgcl/net/url.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class query_params;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::query_params` is `application/x-www-form-urlencoded` as the WHATWG URL Standard (§5) reads and writes it:
a list of name and value pairs in their order, a name as often as it comes, as the query of a URL and the body of an
HTML form carry them. It is Go's `url.Values` and JavaScript's `URLSearchParams`, with the order of the latter: Go's
is a map, and its `Encode` sorts.

It is a list, not a map: a name may come many times, the order is kept, and a lookup by name walks the list, as in the
standard. A query is a handful of pairs, and there is no hash for the sender to steer. The pairs are kept unescaped;
[parse](parse.md) unescapes them and [to_string](to_string.md) escapes them again.

## Rules

- **A query_params holds a [vector](../../core/vector/README.md)** of pairs of [strings](../../core/string/README.md).
  One thread at a time changes it. A copy is a list of its own; a list moved from is empty, as the vector moved from is.
- **parse reads every text as pairs**, as the standard does: a `+` is a space, an escape that is not `%XX` stays as it
  is written, bytes that are not UTF-8 after the unescaping become U+FFFD; a leading `?` is taken off first. Its one
  refusal is the limit below.
- **to_string writes a space as `+`** and escapes everything but the letters, the digits and `* - . _`.
- **add and set may refuse**, for the limit alone: they return an `expected<void, io::error>`, and the pairs stay as
  they were when it holds an error. erase returns the object.
- **The limit.** The 512 MiB a [url](../url/README.md#rules) keeps is kept here: [parse](parse.md) and
  [first](first.md) refuse a text past it, and the length of [to_string](to_string.md) is
  tracked through every change, so that [parse](parse.md), [add](add.md) and
  [set](set.md) refuse pairs that would be written past it (a byte the form escapes is three), each with
  the error `net::errc::invalid_url`. to_string is written once into a string of that length, and nothing here throws.
  The one way past the limit is [url::query_params](../url/query_params.md): a URL's query is within it, and its pairs
  are taken whole, though written by the form's rules they may take up to three times it (a `/` the query keeps and the
  form escapes); add and set refuse to grow such pairs, and [url::with_query](../url/with_query.md) refuses them.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](query_params.md) | constructs an empty list, or the list a literal spells |

#### Lookup

| Function | Description |
|---|---|
| [get](get.md) | the first value of a name |
| [get_all](get_all.md) | every value of a name, in their order |
| [contains](contains.md) | checks whether a name is there |
| [first](first.md) | the first value of a name, found in a text without parsing the rest (static) |

#### Modifiers

| Function | Description |
|---|---|
| [add](add.md) | adds a pair at the end |
| [set](set.md) | sets the value of a name in the place of its first pair |
| [erase](erase.md) | removes every pair of a name |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the number of pairs |
| [empty](empty.md) | checks whether there are no pairs |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first pair |
| [end](end.md) | an iterator past the last pair |

#### Text

| Function | Description |
|---|---|
| [parse](parse.md) | reads the pairs of a text (static) |
| [to_string](to_string.md) | the pairs written, `a=1&b=x+y` |

#### Comparison

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares two lists pair by pair |

## Complexity

[get](get.md), [get_all](get_all.md), [contains](contains.md),
[set](set.md) and [erase](erase.md) are linear in the number of pairs, set and erase also in
the length of the pairs of the name, whose written length they measure; [add](add.md) is linear in the
length of the pair; [parse](parse.md), [first](first.md) and
[to_string](to_string.md) are linear in the length of the text.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params params("?q=katze&tag=a+b&tag=c%2B%2B");
    println("{} {} {}", params.get("q"), params.get_all("tag").size(), params.contains("page"));

    params.set("q", "hund");
    params.add("page", "2");
    params.erase("tag");
    println(params.to_string());

    net::url u("https://example.com/search");
    println(u.with_query(params)->to_string());
}
```

Output:

```text
katze 2 false
q=hund&page=2
https://example.com/search?q=hund&page=2
```

## See also

- [url::query_params](../url/query_params.md): the pairs of a URL's query; [url::with_query](../url/with_query.md): a URL
  with pairs as its query
- [http::request](../http/request/README.md): a request's query and a form's body
