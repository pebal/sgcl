# sgcl::slog

Structured logging: what Go has in `log/slog`. A record is a level, a message and attributes, pairs of a key and a value that the compiler checks; it is written as a line of text or of JSON, byte for byte as slog writes it, or handed to a handler of the program. `#include "sgcl/slog/slog.h"` brings the module in. It depends on:

- [`core`](../core/README.md);
- [`async`](../async/README.md) (the workers a buffered logger batches per);
- [`io`](../io/README.md) (the writer a line goes to, `io::stderr` by default);
- [`txt`](../txt/README.md) (what Unicode says is printable, for the quoting);
- [`time`](../time/README.md) (the time of a record, in the local zone);
- [`encoding`](../encoding/README.md) (a type described by its fields).

The index of the whole interface is [`docs/sgcl/`](../README.md).

## One line

```cpp
#include "sgcl/io/io.h"
#include "sgcl/slog/slog.h"

using namespace sgcl;

int main() {
    slog::info("server started", "port", 8080, "tls", true);
    auto config = io::read_file("/etc/app.conf");
    if (!config) {
        slog::error("read failed", "path", "/etc/app.conf", "error", config.error());
    }
}
```

Output:

```text
time=2026-09-28T21:21:25.303+02:00 level=INFO msg="server started" port=8080 tls=true
time=2026-09-28T21:21:25.304+02:00 level=ERROR msg="read failed" path=/etc/app.conf error="open /etc/app.conf: No such file or directory"
```

On `io::stderr`, one line each, the time in the local zone (the time of the run: the one above is an example).

`debug`, `info`, `warn` and `error` write through the [default logger](logger.md#default_logger-set_default): text on `io::stderr`, from `info` up. The attributes are pairs, a key and a value: the key a literal (or a `const char*`), the value one of the kinds below. An odd count, a key that is not a string, a value of a kind the module does not take: each an error of the build, where Go writes `!BADKEY` or `%v` at run time.

## A logger of one's own

```cpp
slog::logger log(slog::options{.out = file, .level = slog::level::debug, .json = true});
auto api = log.with("service", "api");
api.warn("slow request", "path", path, "took", took);
```

```text
{"time":"2026-09-28T14:05:01.123456789+02:00","level":"WARN","msg":"slow request","service":"api","path":"/users","took":1500000000}
```

A [`logger`](logger.md) is a handle of one word, made by its constructor from [`slog::options`](logger.md#options) (the output, text or JSON, the level, `source`, `utc`, `buffered`, sampling), or from a writer and a level for text lines, or from a handler of the program; the verbs (`debug`, `info`, `warn`, `error`, `log`) write. `with(...)` gives a logger that writes its attributes in every record, rendered once when it is made; `group(name)` puts what comes after it in a group. Copies, and the loggers made from one by `with` and `group`, share its output.

## The kinds of values

| the value | text | JSON |
|---|---|---|
| `bool`, an integer, `float`, `double` | `true`, `-5`, `1.5`, `1e+06` (Go's `%v`) | `true`, `-5`, `1.5`, `1000000` (json.Marshal's); NaN and ±Inf as slog's error string |
| a literal, `const char*`, `string`, `std::string`, `std::string_view`, a text slice | as it is, or in quotes when it has to be | a string |
| `duration`, a `std::chrono` duration | `1.5s` | nanoseconds, `1500000000` |
| `time::datetime` | `2026-09-28T14:05:01.123+02:00` (milliseconds) | RFC 3339 with the nanoseconds |
| `io::error`, `std::error_code` | its message | a string |
| `std::exception_ptr` | `std::runtime_error: what it said` (null: `<nil>`) | a string |
| `optional<T>`, `nullptr` | the value, or `<nil>` | the value, or `null` |
| a type with `write_text(char*)` and `MaxText` (`net::ip_address`, `net::endpoint`, `net::ip_network`) or a `format_value` | its text, written into the line | a string |
| a type with `to_text()` or `to_string()` | its text (the string it makes is its cost) | a string |
| a type with [`describe`](../encoding/fields.md) | a group of its fields: `req.id=5 req.path=/a` | an object: `"req":{"id":5,"path":"/a"}` |
| `slog::group("req", "id", id, ...)` | `req.id=5` | `"req":{"id":5}` |

A field of a described type that is a container, a map, a variant or a `json` is written as Go writes a value of `KindAny`: in text as `%+v` does (`[a b]`, `map[k:v]`, `{id:5 path:/a}`), in JSON as `json.Marshal` does. A group of no attributes is left out, as slog leaves it.

## The rules

- **One record, one write.** The line is made whole in plain memory of the thread and written by one `write` to the writer, from the thread that logs, with no lock of the module's: the writer takes writes from many threads at once, as a file opened for appending, `io::stderr` and a connection do. A `buffered_writer` does not: set `options::buffered` instead.
- **Nothing managed per record.** A record allocates nothing on the managed heap: the line is in the thread's plain memory, grown once to the largest record; the attributes of `with` were rendered when the logger was made; a described type is read through its fields where it lies. A value whose type makes its text as a `string` (`to_text`, `to_string`, an `io::error`'s message) costs that string. `tests/slog/heap.cpp` counts it.
- **Below the level, nothing.** A record below the logger's level is one comparison: no line, no text of any value. The arguments themselves are computed by the caller, as for any call: an argument that costs to compute goes under `if (log.enabled(slog::level::debug))`.
- **A write that fails** is not returned to the caller, as `print`'s is not: `dropped()` counts the records lost, and the first failure of an output says so in one line on `io::stderr`.
- **Buffered.** `options::buffered` gathers the lines of each worker of the scheduler in a batch of 32 KB of its own, written by one write when it is full, at a record of `warn` and up, when the worker goes to sleep, at `flush()` and at exit (`atexit`, `at_quick_exit`). The order across workers holds only within each worker; every line has its time. A thread that is no worker shares one batch with the others, written when it is full, at a `warn`, at `flush()` and at exit.
- **Handlers of the program.** `slog::logger(h)` (or `options::handler`) gives the records to a handler of the program ([`handler`](handler.md)): any type with `handle(const record&)`, called from every thread that logs. The record is a view of the call's stack; `clone()` is a copy to keep. [`slog::memory`](handler.md#memory) keeps them, for tests.
- **Rotation and sending logs elsewhere** are not in the module; a handler of the program does either.

## Example

```cpp
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"
#include "sgcl/net/net.h"
#include "sgcl/slog/slog.h"

using namespace sgcl;

struct request {
    int id = 0;
    string path;

    void describe(encoding::field_list& f) {
        f.add("id", id);
        f.add("path", path);
    }
};

int main() {
    slog::info("server started", "port", 8080, "tls", true);

    slog::logger log(slog::options{.out = io::stdout, .level = slog::level::debug, .utc = true});
    auto api = log.with("service", "api").group("http");
    api.debug("request", "req", request{7, "/users"}, "took", 1500 * microsecond);
    auto peer = net::endpoint(net::ip_address::v4(10, 0, 0, 2), 443);
    api.warn("slow", slog::group("peer", "addr", peer, "tls", true));
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="server started" port=8080 tls=true
time=2026-09-28T12:05:01.123Z level=DEBUG msg=request service=api http.req.id=7 http.req.path=/users http.took=1.5ms
time=2026-09-28T12:05:01.123Z level=WARN msg=slow service=api http.peer.addr=10.0.0.2:443 http.peer.tls=true
```

The first line goes to `io::stderr`, the others to `io::stdout`; the times are those of the run.

## The collector's log

The collector's own lines (`SGCL_LOG_PRINT_LEVEL`, [config](../core/config.md#sgcl_log_print_level)) as records, instead of lines on `std::cout`:

```cpp
// the collector's lines of level 1: its start and stop, every force_collect
#define SGCL_LOG_PRINT_LEVEL 1
#include "sgcl/slog/slog.h"

using namespace sgcl;

int main() {
    slog::collector_log();  // through the default logger: text on io::stderr
    collector::force_collect(true);  // optional, for the demonstration only
    slog::info("done");  // a record takes the collector's waiting lines first
}
```

Sample output:

```text
time=2026-09-29T11:02:15.207+02:00 level=INFO msg=collector verbosity=1 line="force collect and wait from id: 0x16b0f3000"
time=2026-09-29T11:02:15.209+02:00 level=INFO msg=done
```

- **Only when asked.** Until `collector_log(log)` (or `collector_log()`, the default logger) is called the lines go to `std::cout` byte for byte as before, in a program that includes slog too; a line said before the call (the collector's start) is there.
- **Every record at info, `msg=collector`,** the macro's level as `verbosity` (1 to 3). The line of a cycle (level 2) comes as its numbers: `mem_allocs`, `mem_removed`, `total_mem`, `objects_created`, `objects_removed`, `live_objects`, `cycle` (`full` or `young`), `helpers`, `helpers_used`, `time_ms`, `total_time_ms`; any other line as `line`, its text without `[sgcl] `.
- **When they are written.** A line comes from the collector's thread or from a thread's registration, where nothing managed may be touched: it is copied into a queue of plain memory there, and written by a thread that may log: a worker on its way to sleep, the next record of any logger, `flush()`, the exit. At most 4096 lines wait; past that they are counted, and a `warn` record says how many were dropped.

## The pages

| | |
|---|---|
| [logger](logger.md) | `logger`, its `options` and verbs, `level`, `level_var`, `group`, `message`, the free functions and the default logger |
| [handler](handler.md) | `handler`, `memory`, and what a handler reads: `record`, `attr`, `value` |

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `slog.Info("msg", "k", v)` | `slog::info("msg", "k", v)` | pairs checked by the compiler: no `!BADKEY` |
| `slog.New(slog.NewJSONHandler(w, &slog.HandlerOptions{Level: slog.LevelDebug, AddSource: true}))` | `slog::logger(slog::options{.out = w, .level = slog::level::debug, .json = true, .source = true})` | the options a struct |
| `logger.With("k", v)`, `WithGroup("g")` | `log.with("k", v)`, `log.group("g")` | |
| `slog.Group("g", ...)` | `slog::group("g", ...)` | |
| `slog.LevelVar` | `slog::level_var` | |
| `slog.SetDefault`, `slog.Default` | `slog::set_default`, `slog::default_logger` | atomic |
| a struct as `slog.Any` | a type with `describe`: a group | `%+v` and `json.Marshal` for its containers |
| `HandlerOptions.ReplaceAttr` | — | a handler of the program |
| `slog.Handler` (an interface) | `slog::handler` (a value over any type with `handle`) | `with`'s attributes arrive inside the record, as a tree |
| — | `options::buffered`, `options::sample_first`/`sample_then`/`sample_per`, `dropped()` | a batch per worker; zap's sampling |
