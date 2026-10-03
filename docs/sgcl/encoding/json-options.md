[sgcl](../README.md) › [encoding](README.md) › [json](json/README.md)

# sgcl::encoding::json::options

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        struct options {
            uint32_t max_depth = 512;
            bool allow_duplicate_keys = false;
            bool allow_invalid_utf8 = false;
            bool keep_number_text = false;
            bool reject_unknown_fields = false;
            size_t max_token_size = size_t(64) << 20;
        };
    };
}
```

`sgcl::encoding::json::options` is what a parse accepts: [parse](json/parse.md), [as](json/as.md), the
[reader](json-reader/README.md). The defaults are those of Go's v2: invalid UTF-8, a lone surrogate and a key given twice
in one object are errors, and the unknown fields of a typed read are skipped. A plain struct: set the fields that
differ and pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.
- A task's [async_parse](json/parse.md) takes its options by value, so the caller's may be gone when it runs.

## Member objects

| Object | Description |
|---|---|
| `max_depth` | how deep arrays and objects may nest, inside one another: `depth_limit` past it; 512 |
| `allow_duplicate_keys` | a key given twice in one object is taken, the last one winning in its place, rather than `duplicate_key` (Go's v2 `AllowDuplicateNames`); `false` |
| `allow_invalid_utf8` | invalid UTF-8 in a string, a lone surrogate `\uD800` too, is taken as U+FFFD rather than `invalid_utf8` or `invalid_escape` (Go's v2 `AllowInvalidUTF8`); `false` |
| `keep_number_text` | a number that is not an integer literal is kept as its literal rather than rounded to a double, for amounts that cannot pass through one: [number_text](json/number_text.md) gives it, and one out of a double's range (`1e400`) is no error (Go's `UseNumber`); `false` |
| `reject_unknown_fields` | a typed read ([parse](json/parse.md)`<T>`, [as](json/as.md)`<T>`, the reader's `read<T>`): a key no field has is `unknown_field`, with that key's path (`/home/zip`), rather than skipped (Go's `DisallowUnknownFields`); `false` |
| `max_token_size` | what a [reader](json-reader/README.md) of a stream holds at once, a token or a value read whole: a token of this many bytes is read, and one the reader holds more of and still asks more for is `out_of_range`; a token whole in the block a read filled is not measured; 64 MiB |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = R"({"price": 21.5, "price": 19.990000000000000001})";
    println(encoding::json::parse(text).error().message());

    encoding::json::options o;
    o.allow_duplicate_keys = true;
    o.keep_number_text = true;
    encoding::json doc = encoding::json::parse(text, o);
    println(doc.to_string());

    o.max_depth = 2;
    println(encoding::json::parse("[[[1]]]", o).error().message());
}
```

Output:

```text
1:17: duplicate key "price"
{"price":19.990000000000000001}
1:3: nesting deeper than 2
```

## See also

- [parse](json/parse.md): a text or a stream read
- [style](json-style.md): how a value is written
- [sgcl::encoding::json](json/README.md)
