[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::parse, async_parse

```cpp
static expected<yaml, error> parse(const string& text) noexcept;                             // (1)
static expected<yaml, error> parse(const string& text, const options& o) noexcept;           // (2)
static expected<yaml, error> parse(const io::reader& in);                                    // (3)
static expected<yaml, error> parse(const io::reader& in, const options& o);                  // (4)
static async::task<expected<yaml, error>> async_parse(io::reader in) noexcept;               // (5)
static async::task<expected<yaml, error>> async_parse(io::reader in, options o) noexcept;    // (6)
```

Exactly one document: a text of none is null, a text of two is an error ([parse_all](parse_all.md) reads every
document of a stream).

1. Of the text, with the default [options](../yaml-options.md).
2. The same with the options given.
3. Of the text of the stream, read to its end.
4. The same with the options given.
5. (3) for a task.
6. (4) for a task.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 (a byte order mark at its start passed over) |
| `in` | the stream |
| `o` | what is accepted |

## Return value

The node, or the [error](../error/README.md) with its line and column: `syntax` for what the grammar does not
take (a mapping value where none may be, an indentation that is no block's, a tab in it, a simple key without its
`:`, an alias of no anchor, a second document); `unexpected_end` for a quoted scalar or a flow collection not
closed; `invalid_escape`; `invalid_utf8`; `invalid_character` for a character YAML does not allow; `type_mismatch`
for a core tag on a node it does not fit (`!!int x`); `duplicate_key`; `depth_limit`; `limit_exceeded` for the nodes
reached through aliases past `max_alias_nodes`; `io` for a stream that failed.

## Complexity

Linear in the length of the text, and in the members of each mapping for the check of its keys.

## Exceptions

- (1–2), (5–6) None.
- (3–4) What the read of the stream throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto doc = encoding::yaml::parse("name: app\nports:\n  - 80\n  - 443\n");
    println("{} {}", doc->operator[]("name").as_string("?"), doc->operator[]("ports").size());
    for (const char* bad : {"a: b: c", "a: 1\na: 2", "list: [1, 2", "a: &x 1\nb: *y"}) {
        println(encoding::yaml::parse(bad).error().message());
    }
}
```

Output:

```text
app 2
1:5: a mapping value where none may be
2:1: a key given twice in a mapping
1:12: no ',' or ']' after an entry of a flow sequence
2:4: an alias of no anchor before it
```

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the billion laughs: stopped at the first alias past the bound
    string bomb = "a: &a [lol, lol, lol, lol, lol, lol, lol, lol, lol]\n";
    for (char c : string("bcdefghi")) {
        bomb = bomb + string(1, c) + ": &" + string(1, c) + " [";
        for (int k : range(9)) {
            bomb = bomb + (k ? ", *" : "*") + string(1, char(c - 1));
        }
        bomb = bomb + "]\n";
    }
    println(encoding::yaml::parse(bomb).error().message());
}
```

Output:

```text
7:8: more nodes reached through aliases than max_alias_nodes
```

## See also

- [parse_all](parse_all.md)
- [yaml::options](../yaml-options.md)
- [sgcl::encoding::yaml](README.md)
