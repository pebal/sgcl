# sgcl::slog::logger, options, level, level_var, group, message

```cpp
#include "sgcl/slog/logger.h"   // or "sgcl/slog/slog.h", "sgcl/sgcl.h"

namespace sgcl::slog {
    enum class level : int8_t { debug = -4, info = 0, warn = 4, error = 8 };
    class level_var;                  // a level changed while the program runs
    class message;                    // the text of a record and where the call is
    template<class... A> class group; // attributes in a group of their own
    struct options;                   // what a logger is made with: the output, the format, the level
    class logger;                     // what a record is written as, from which level, with which attributes

    template<class... A> void debug(message m, const A&... kv);   // and info, warn, error: through the default logger
    logger default_logger();
    void set_default(const logger& l);
}
```

## One line

```cpp
slog::info("server started", "port", 8080);
slog::logger log(io::stdout, slog::level::debug);    // text lines on stdout from debug
slog::logger json(slog::options{.out = file, .json = true, .source = true, .utc = true});
```

## Rules

- **Made once, by its constructor.** What a logger writes, where and from which level is given when it is made: `slog::logger(options)`, or `slog::logger(out, level)` for text lines, or `slog::logger(handler, level)` for a handler of the program. Nothing of it changes later but a [`level_var`](#level_var)'s level.
- **Children.** `with` and `group` give a new logger and leave the one they were called on as it was: `base.with("k", 1)` is a child, `base` still writes without `k`. The verbs write.
- **The output is shared.** The copies of a logger and the loggers made from it by `with` and `group` share its output: the writer, its batches when buffered, the count of the records lost. Each constructor makes an output of its own.
- **Where a logger lives.** A handle, one tracked word: on a stack, in a task, in a managed object; in a global or a `std` container a [`rooted`](../core/rooted.md) of it (`rooted<slog::logger> log(slog::logger(slog::options{.out = file, .json = true}));`). The default logger is the module's own, kept that way.
- **The levels are Go's numbers**, so the levels between them compare and read as Go's do: `slog::level(2)` is written `INFO+2`, `slog::level(-5)` `DEBUG-1`.

## Members

### level

```cpp
enum class level : int8_t { debug = -4, info = 0, warn = 4, error = 8 };
```

### level_var

```cpp
class level_var {
public:
    explicit level_var(level l = level::info);   // made at its level
    void set(level l) const noexcept;            // seen by the next record of every logger made with it
    level get() const noexcept;
};
```

One level for many loggers, changed while the program runs (slog's `LevelVar`): `slog::options{.level_var = v}`. A handle: the copies share the level. A relaxed atomic: a record in flight on another thread may still be judged by the level before.

### message

```cpp
class message {
public:
    message(const char* text, std::source_location where = std::source_location::current()) noexcept;   // a literal, a const char*
    message(const string& text, std::source_location where = ...) noexcept;
    message(const std::string& text, std::source_location where = ...) noexcept;
    message(const slice<const char>& text, std::source_location where = ...) noexcept;
    slice<const char> text() const noexcept;
    const std::source_location& where() const noexcept;
};
```

The first parameter of every verb: the text and the place of the call, which the constructor's default argument takes where the call is written (after the pack of attributes a verb has no room for a default argument of its own). It refers to the text, as the call does.

### group

```cpp
template<class... A> class group {
public:
    group(const char* name, const A&... kv) noexcept;    // slog::group("req", "id", id, "path", path): the arguments deduced
};
```

Attributes in a group of their own (slog's `Group`), given where a key would be, the name inside: `log.info("m", slog::group("req", "id", 5))` writes `req.id=5` as text and `"req":{"id":5}` as JSON. A group of no attributes, or of empty groups only, is left out; a group without a name puts its attributes where it stands. A group keeps its arguments by reference, as the call does: it is made in the call that logs it.

### options

```cpp
struct options {
    io::writer out = io::writer(io::stderr);   // where the text or JSON lines go
    slog::handler handler;                     // the records to a handler of the program instead, when set
    slog::level level = slog::level::info;     // the least level written
    optional<slog::level_var> level_var;       // the least level read from it at every record, in place of level
    bool json = false;                         // JSON lines, text lines otherwise
    bool source = false;                       // file:line of the call, as `source`
    bool utc = false;                          // the time in UTC, not the local zone
    bool buffered = false;                     // a batch per worker
    uint32_t sample_first = 0;                 // sampling: see below; sample_per zero is none
    uint32_t sample_then = 0;
    duration sample_per = {};
};
```

Go's `HandlerOptions` with the choice of handler, a plain struct filled by designated initializers in the order of its fields: `slog::options{.out = file, .level = slog::level::debug, .json = true}`.

### logger

```cpp
class logger {
public:
    logger();                                                              // text on io::stderr, level info, the local time
    explicit logger(const options& o);
    explicit logger(const io::writer& out, slog::level l = level::info);   // text lines on out
    explicit logger(const handler& h, slog::level l = level::info);        // the records to a handler of the program

    template<class... A> logger with(const A&... kv) const;     // these attributes in every record
    logger group(const char* name) const;                       // what comes after it in a group

    template<class... A> void debug(message m, const A&... kv) const;   // info, warn, error the same
    template<class... A> void log(slog::level l, message m, const A&... kv) const;

    bool enabled(slog::level l) const;          // whether a record of l is written
    void flush() const;                         // the batches written now
    uint64_t dropped() const noexcept;          // the records whose write failed
};
```

**The formats.** Text is slog's `TextHandler` byte for byte: `time=… level=INFO msg="server started" port=8080`, a text in quotes when it is empty or holds a space, `=`, `"`, a control, invalid UTF-8 or a code point Unicode does not call printable, escaped inside as Go's `strconv.Quote` escapes (`\n`, `\x1b`, ` `, `\xff` for a byte of invalid UTF-8); a group's name before its keys with a dot. JSON is slog's `JSONHandler` byte for byte: the keys `time`, `level`, `source`, `msg`, a group an object. The time is the local zone's (`+02:00`), or UTC with `options::utc` (`Z`): three places of the second in text, the nanoseconds in JSON. `tools/slog_oracle.go` writes the lines Go writes for the same records, and `tests/slog/oracle.cpp` compares them.

**`options::source`** adds where the call is: `source=src/main.cpp:42` in text, `"source":{"function":"int main()","file":"src/main.cpp","line":42}` in JSON — the function as the compiler names it, the file as it was given to the compiler.

**with(...)** takes the pairs and groups a verb takes. They are copied and rendered into both formats once, now; a record adds them as bytes. A type described by its fields, a `to_text` or a `format_value` is made text at `with`, so a logger may outlive the objects it was given.

**group(name)** puts every later attribute, of `with` and of the records, in a group of that name; an empty name is no group. A group that a record would leave empty is not written.

**`options::buffered`** — see the [rules](README.md#the-rules). The batches are per worker of the scheduler (`async::detail::Scheduler::worker_index()`), 32 KB each, one lock each that only a `flush()` or the exit ever meets from another thread; a worker writes its own on its way to sleep. A record of `warn` and up writes its batch at once, itself included. A logger that gives its records to a handler of the program (`options::handler`) is not batched: the handler is given each record as it comes.

**`options::sample_first`, `sample_then`, `sample_per`** keep, of the records of one level and one message in each span of `sample_per`, the first `sample_first` and then every `sample_then`-th (`sample_then` 0: none more), as zap's sampler does; counted per worker, without a lock between workers (a hash of the message into 256 counters each). The records left out are said once, in a line of their own before the first record after their span: `level=WARN msg="records sampled out" count=6`.

**enabled(l)** is what a record asks first: one comparison, and the handler's `enabled` for a handler of the program. A record below the level makes nothing.

### default_logger, set_default

```cpp
logger default_logger();              // a copy: it shares the default's output
void set_default(const logger& l);    // atomic, for every thread from now on
template<class... A> void debug(message m, const A&... kv);    // default_logger().debug(m, kv...); info, warn, error
```

The default logger is kept in a `rooted<atomic<logger>>` of the module's (the atomic of a one-word handle, [atomic](../core/atomic.md)): `set_default` is one atomic store, `default_logger()` one atomic load. It starts as `logger()`: text on `io::stderr` from `info` up.

## Example

```cpp
#include "sgcl/io/io.h"
#include "sgcl/slog/slog.h"

using namespace sgcl;

int main() {
    slog::logger log(slog::options{.out = io::stdout, .level = slog::level::debug, .json = true});
    log.with("service", "api").debug("started", "port", 8080);
}
```

Output:

```text
{"time":"2026-09-28T14:05:01.123456789+02:00","level":"DEBUG","msg":"started","service":"api","port":8080}
```

The time is that of the run.

```cpp
#include "sgcl/io/io.h"
#include "sgcl/slog/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;
    slog::level_var least(slog::level::warn);
    slog::logger log(slog::options{.out = out, .level_var = least, .utc = true});

    log.info("not written");
    least.set(slog::level::debug);
    log.info("written", "attempt", 2);

    auto child = log.with("user", "ala").group("req");
    child.warn("slow", "path", "/users", "took", 250 * millisecond);
    log.info("the parent as it was");

    print(out.text());
    println("{} records lost, debug {}", log.dropped(), log.enabled(slog::level::debug));
}
```

Output:

```text
time=2026-09-28T12:05:01.123Z level=INFO msg=written attempt=2
time=2026-09-28T12:05:01.123Z level=WARN msg=slow user=ala req.path=/users req.took=250ms
time=2026-09-28T12:05:01.123Z level=INFO msg="the parent as it was"
0 records lost, debug true
```

The times are those of the run.

## See also

- [README](README.md): the kinds of values, the rules; [handler](handler.md): `handler`, `memory`, `record`
- `tests/slog/logger.cpp`, `tests/slog/oracle.cpp`: every behaviour above, checked.
