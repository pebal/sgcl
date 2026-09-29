# sgcl::slog::handler, memory, record, attr, value

```cpp
#include "sgcl/slog/handler.h"   // or "sgcl/slog/slog.h", "sgcl/sgcl.h"

namespace sgcl::slog {
    namespace req {
        template<class T> concept handler;   // handle(const record&); enabled(level) if it has one
    }
    class handler;    // any handler, as a value
    class memory;     // a handler that keeps the records, for tests
    class record;     // a record as a handler reads it
    class attr;       // an attribute: its key and value
    class value;      // a value, and value::kind
}
```

## One line

```cpp
slog::memory kept;
auto log = slog::logger(kept);
```

## Rules

- **A handler is any type with `handle(const record&)`**, and with `enabled(slog::level)` if it would rather not be given some levels (none: every level). Nothing to derive from, nothing virtual: `slog::handler` holds any of them as a value — a `tracked_ptr` of what keeps it, a pointer to it and a table of its two methods made once per type, as [`io::writer`](../io/stream.md) holds a writer. A handler given by `tracked_ptr` is held by it; one of your own given by reference is referenced (its managed object kept if it lies in one; one on a stack or a global is yours to keep alive); a temporary, or a handle of the library (`memory`), is copied into a managed object of its own.
- **From every thread.** `handle` is called on the thread that logs, from every thread that logs at once. A handler of your own guards what it keeps; `memory` does.
- **A record is a view.** The record, its attributes and their values point at the stack of the call and at the objects it named, valid while `handle` runs. `clone()` is a copy that owns all it holds — its texts, its attributes, a described type made a group of copies, a container of the program its two texts — to keep for later.
- **The tree.** A handler gets the logger's attributes too, as a tree: `with`'s before a `group` at the top, a logger's `group("req")` an attribute of kind group named `req` holding what came after it, and the call's own attributes inside the innermost group. Go's `Handler` gets them apart (`WithAttrs`, `WithGroup`); here a handler has one method and the record says it all.

## Members

### handler

```cpp
class handler {
public:
    handler() noexcept;                               // empty
    template<class H> handler(H&& h);                 // any H with handle(const record&), or a tracked_ptr to one
    void handle(const record& r) const;
    bool enabled(slog::level l) const;                // H's enabled, or true
    explicit operator bool() const noexcept;
};
```

### memory

```cpp
class memory {
public:
    memory();                                         // made empty
    vector<record> records() const;                   // the clones kept, in the order they came
    size_t size() const;
    void clear() const;
    void handle(const record& r) const;               // keeps r.clone()
    bool enabled(slog::level) const noexcept;         // true
};
```

A handle of one word: the copies share the records. Any thread may log through it.

### record

```cpp
class record {
public:
    time::datetime time() const;                      // in the logger's zone: local, or UTC after utc()
    slog::level level() const noexcept;
    slice<const char> message() const noexcept;
    const std::source_location& source() const noexcept;   // where the call is
    bool has_source() const noexcept;                 // the logger's source() asked for it
    iterator begin() const noexcept;  iterator end() const noexcept;   // the attributes at the top of the tree, as attr
    size_t size() const noexcept;  bool empty() const noexcept;
    record clone() const;                             // a copy that owns all it holds
};
```

### attr, value

```cpp
class attr {
public:
    slice<const char> key() const noexcept;
    slog::value value() const noexcept;
};

class value {
public:
    enum class kind : uint8_t { null, boolean, int64, uint64, float64, string, duration, time, group, any };
    kind type() const noexcept;
    bool as_bool() const;          int64_t as_int() const;         uint64_t as_uint() const;
    double as_double() const;      string as_string() const;       duration as_duration() const;
    time::datetime as_time() const;                  // in a zone of its offset
    range of attr as_group() const;                  // a group's attributes; a described type's, copied first
    string text() const;                             // as the text handler writes it, not quoted: 5, 1.5s, [a b]; a group [k=v k2=v2]
    string json() const;                             // as the JSON handler writes it
};
```

slog's `Value` and its kinds. An `as_…` of another kind is a `logic_error` (Go panics). A string is `as_string()` whatever made it: a literal, a `string`, a type's `write_text` or `to_text`. A type described by its fields is a group of them. `any` is what Go calls `KindAny`: a container, a map, a variant or a `json` of the program, read through `text()` (Go's `%+v`) and `json()` (json.Marshal's).

## Example

```cpp
#include "sgcl/io/io.h"
#include "sgcl/slog/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).warn("disk almost full", "free", 0.05);
    for (const auto& r : kept.records()) {
        println("{} {} free={}", r.level() == slog::level::warn, r.message(), (*r.begin()).value().as_double());
    }
}
```

Output:

```text
true disk almost full free=0.05
```

```cpp
#include "sgcl/io/io.h"
#include "sgcl/slog/slog.h"

using namespace sgcl;

struct errors_only {
    void handle(const slog::record& r) const {
        println("{}: {}", r.message(), r.size());
        for (auto a : r) {
            println("  {} = {}", a.key(), a.value().text());
        }
    }

    bool enabled(slog::level l) const {
        return l >= slog::level::error;
    }
};

int main() {
    auto log = slog::logger(errors_only()).with("service", "api").group("req");
    log.info("not given to the handler");
    log.error("failed", "id", 7, "ok", false);

    slog::memory kept;
    auto test = slog::logger(kept, slog::level::debug);
    test.debug("first", "n", 1);
    test.warn("second", "ratio", 0.5);
    for (const auto& r : kept.records()) {
        auto a = *r.begin();
        println("{} {} {}", r.message(), a.key(), a.value().json());
    }
}
```

Output:

```text
failed: 2
  service = api
  req = [id=7 ok=false]
first n 1
second ratio 0.5
```

## See also

- [README](README.md), [logger](logger.md)
- `tests/slog/logger.cpp`: the handlers, `memory`, the record's views and `clone()`, checked.
