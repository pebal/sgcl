[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::value

```cpp
#include "sgcl/slog/record.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class value {
    public:
        enum class kind : uint8_t;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::value` is the value of an attribute as a handler reads it, slog's `Value` and its kinds:
[type](type.md) says the [kind](../value-kind.md), and an `as_` accessor gives the value of that kind.
[text](text.md) and [json](json.md) write it as the two handlers of the module do, whatever its kind.

What a call gave becomes a kind as the record makes it: a signed integer is `int64`, an unsigned one `uint64`, a
`float` or a `double` `float64`; a string is `string` whatever made it — a literal, a `string`, an `io::error`'s or an
`error_code`'s message, an exception's text, a type's `write_text`, `format_value`, `to_text` or `to_string`; an
`optional` the kind of its value, or `null`; a type described by its fields is a group of them. `any` is what Go
calls `KindAny`: a container, a map, a variant or a `json` of the program, a field of a described type, read through
`text()` (Go's `%+v`) and `json()` (json.Marshal's).

## Rules

- A view of the record's, valid while the record is: in the record a handler is given, while `handle` runs; in a
  [clone](../record/clone.md), while the clone is.
- An `as_` accessor of another kind throws `logic_error`, where Go panics.

## Member types

| Type | Definition |
|---|---|
| [kind](../value-kind.md) | the kind of a value: `null`, `boolean`, `int64`, `uint64`, `float64`, `string`, `duration`, `time`, `group`, `any` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](value.md) | constructs a value of kind `null` |

#### Observers

| Function | Description |
|---|---|
| [type](type.md) | the kind of the value |
| [as_bool](as_bool.md) | the value of a `boolean` |
| [as_int](as_int.md) | the value of an `int64` |
| [as_uint](as_uint.md) | the value of a `uint64` |
| [as_double](as_double.md) | the value of a `float64` |
| [as_string](as_string.md) | the text of a `string` |
| [as_duration](as_duration.md) | the value of a `duration` |
| [as_time](as_time.md) | the value of a `time`, in a zone of its offset |
| [as_group](as_group.md) | the attributes of a `group` |

#### Text

| Function | Description |
|---|---|
| [text](text.md) | the value as the text handler writes it, not quoted |
| [json](json.md) | the value as the JSON handler writes it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct summary {
    void handle(const slog::record& r) const {
        for (auto a : r) {
            auto v = a.value();
            switch (v.type()) {
                case slog::value::kind::int64:
                    println("{}: int {}", a.key(), v.as_int());
                    break;
                case slog::value::kind::string:
                    println("{}: text {}", a.key(), v.as_string());
                    break;
                default:
                    println("{}: {}", a.key(), v.json());
                    break;
            }
        }
    }
};

int main() {
    slog::logger(summary()).info("m", "port", 8080, "host", "localhost", "tls", true,
                                 "error", io::read_file("/no/such/file").error());
}
```

Output:

```text
port: int 8080
host: text localhost
tls: true
error: text open /no/such/file: No such file or directory
```

## See also

- [attr](../attr/README.md), [record](../record/README.md)
- [The kinds of values](../README.md#the-kinds-of-values): what a call may give
- [sgcl::slog](../README.md)
