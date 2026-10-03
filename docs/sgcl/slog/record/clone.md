[sgcl](../../README.md) › [slog](../README.md) › [record](README.md)

# sgcl::slog::record::clone

```cpp
record clone() const;
```

Returns a copy of the record that owns all it holds, to keep after `handle` returns: the message and the keys
copied, the attributes copied with the logger's and the call's joined into one tree, a type described by its fields
made a group of copies of them, a container, a variant or a `json` of the program its two texts (as
[value::text](../value/text.md) and [value::json](../value/json.md) write it); a text form longer than 4 GiB − 1 is
cut to fit, at the start of a code point. [memory](../memory/README.md) keeps clones. A clone of a clone is a clone again.

## Parameters

None.

## Return value

The copy: the same time, level, message, source and attributes, in managed memory of its own.

## Complexity

Linear in the size of the record.

## Exceptions

What a value of the program read through its operations throws while it is copied.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct request {
    int id = 0;
    vector<string> tags;

    void describe(encoding::field_list& f) {
        f.add("id", id);
        f.add("tags", tags);
    }
};

struct keeper {
    vector<slog::record>* kept;

    void handle(const slog::record& r) const {
        kept->push_back(r.clone());
    }
};

int main() {
    vector<slog::record> kept;
    {
        request req{5, {"a", "b"}};
        slog::logger(keeper{&kept}).info("served", "req", req);
    }
    auto group = (*kept[0].begin()).value();
    println("{} {}", group.text(), group.json());
}
```

Output:

```text
[id=5 tags=[a b]] {"id":5,"tags":["a","b"]}
```

## See also

- [memory](../memory/README.md): a handler that keeps clones
- [sgcl::slog::record](README.md)
