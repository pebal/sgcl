# How a page of the documentation is written

The rules every page under `docs/` and the root `README.md` follow. They are complete: a change to the
documentation is checked against every point here, not against the one that prompted it. An agent that
touches a page reads this file first and leaves the page satisfying all of it.

## 1. The pages and their order

### Files and directories

- A class has a directory, `docs/sgcl/<module>/<class>/`: the page of the class is the `README.md` in it
  (`vector/README.md`), and beside it a page per public method: all overloads of a method on one page named
  after it (`vector/insert.md`), the constructors on the page named after the class (`vector/vector.md`). A class
  without pages of its members (a struct of fields) has a page without a directory, `<class>.md`. A function
  that is not a member but belongs to the class (`erase`, `erase_if` of a container) has a page in the same
  directory and a row in the class page's `## Non-member functions`. A function belongs to a class when it takes
  an object of the class as an argument; one that does not (`concurrent::intern_string`, which takes characters
  and reaches the default pool by itself) is a free function, however close to the class, and the class page
  names it only in its description and `## See also`.
- A free function (`io::open`, `encoding::base64::encode`, `make_tracked`) has a page of its own,
  `docs/sgcl/<module>/<function>.md`, in the form of a method's page and without a directory; all its overloads
  are on that page, and the README of the module lists it in its Functions table.
- A function that waits and its form for a task, `x` and `async_x` (`file::read`, `file::async_read`), share one
  page named after `x`, titled `# sgcl::io::file::read, async_read`: they do the same and differ in how they
  wait.
- The page of an operator is named as cppreference names it: `operator_assign.md` (`=`), `operator_at.md`
  (`[]`), `operator_cmp.md` (`==` and `<=>` together), `operator_call.md` (`()`), `operator_deref.md` (`*` and
  `->`), `operator_bool.md` (`explicit operator bool`), `operator_arith.md` (the arithmetic operators and their
  compound assignments), `operator_inc.md` (`++`, `--`), `operator_conv.md` (a conversion operator with no
  named function it mirrors; one that mirrors one is on that function's page, titled `load, operator snapshot`).
  An operator that is a hidden friend is titled as a free function with the class in parentheses:
  `# sgcl::immutable::operator==, operator!= (sgcl::immutable::vector)`. No other character than a letter, a digit, `_` and
  `-` in the name of a file.
- An enumeration has a page without a directory: the names block, the description, a table
  `Value | Description` with every enumerator, an example and `## See also`.
- A constant or a variable (`npos`, `this_thread`) has no page: it is a row of a table on the page of the class or
  the README of the module that declares it, with what it means. `config` keeps its one page with the table of
  its constants.
- A nested type has a page of its own named with a hyphen: `json-reader.md` for `encoding::json::reader`; when
  it has methods, a directory of that name with the page as its `README.md` (`map-builder/README.md`). Its
  breadcrumb runs through the enclosing class (`sgcl › immutable › map › builder`) and its names block shows the
  nesting (`class map { public: class builder; };`).
- `begin` and `cbegin` share the page `begin.md`, titled `…::begin, cbegin`; the same for `end`/`cend`,
  `rbegin`/`crbegin` and `rend`/`crend`.
- A specialization with the interface of the primary template is a section of the class page
  (`## Specializations`: `vector<unique_ptr<T>>`). A specialization with an interface of its own has a page of
  its own named with a hyphen, a directory with the page as its `README.md` and the pages of its methods beside it
  (`atomic-tracked_ptr/README.md`, `atomic-handle/README.md`).
- An alias over a template is documented under the alias's name (`string`); the template and the other aliases
  stand in the names block of that page.
- The methods of a mixin are documented once, on the mixin's pages (`mixin/enumerable/contains.md`); the class
  pages link to them.
- The requirements have a directory, `req/`, with a page per requirement; `req/README.md` is the overview.
- A page about a concept rather than a class (ECDSA over p256 and p384, the hash ids) describes the declarations
  it covers under its rules and has no member tables; the members are on the pages of the classes.

### Titles, navigation, links

- Above the title, the breadcrumb, names only, without template arguments: a method's page
  `[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)`, a class's page
  (`vector/README.md`) `[sgcl](../../README.md) › [core](../README.md)`, a page without a directory
  (`make_tracked.md`) `[sgcl](../README.md) › [core](README.md)`. The page of a module has
  `[sgcl](../README.md) › core`.
- The title is the fully qualified name, never without `sgcl::`: `# sgcl::net::http::client`, a header without a
  class of its own the same way (`# sgcl::txt::bidi`). A class template carries the parameters of its primary
  template, without their defaults (`# sgcl::vector<T>`, `# sgcl::map<Key, T, Hash, KeyEqual>`), a function
  template the ones the caller writes (`# sgcl::make_tracked<T>`); a method page is `# sgcl::vector<T>::insert`,
  a mixin `# sgcl::mixin::enumerable<Derived>`, a requirement `# sgcl::req::equatable` (a concept without its
  argument). The same names stand in the tables of a README. The parameters are named as `std` names them —
  `T` for the element or the mapped type, `Key`, `Hash`, `KeyEqual`, `Compare`, `Container` — in the headers and so
  on the pages.
- In the source, `<` and `>` of a title or of a link's text are escaped, `sgcl::vector\<T\>`: a bare `<T>` is an
  HTML tag that GitHub removes.
- No HTML in a page (`<small>` and the like): what a Markdown viewer does not render shows as text.
- The text of a link has no backticks: `[as_slice](as_slice.md)`, not ``[`as_slice`](as_slice.md)``.
- A link to a class with a directory names its `README.md`: `[vector](../vector/README.md)`, and
  `[vector](README.md)` from a page of its methods.
- Every table has a header row with its columns named, each with a capital letter (`Function | Description`,
  `Parameter | Description`, `Type | Definition`, `Class | Header | Description`); never an empty `| | |`.

### The page of a class

The `README.md` of the class's directory (`vector/README.md`), or `<class>.md` for a class without pages of its
members.

1. The breadcrumb and the title.
2. The names block: a `cpp` block with the include line and the declarations, the class header first and the
   module header as the remark — `#include "sgcl/net/http/client.h"   // or "sgcl/net/http.h"`. No program
   and no fragment of one between the title, this block and the description.
3. The description: what the type is, how it differs from `std` and Go. Prose, no code.
4. `## Rules`: where an object may live, what it may hold, what waits and what does not.
5. `## Template parameters`: a table of the parameters of the class. A container's `Hash`, `KeyEqual` and
   `Compare` say their call must be noexcept: one that is not is rejected at compile time, but for the function
   objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are.
6. `## Member types`: a table, `Type | Definition`.
7. `## Member objects`: the public fields, an options struct's fields, the settings, each with what it means
   and its default.
8. `## Member functions`: tables without code, grouped as cppreference groups them — the constructor, the
   destructor and the assignment first without a heading, then `#### Element access`, `#### Iterators`,
   `#### Capacity`, `#### Modifiers` (or the groups the class has: cppreference's names first — `Lookup`,
   `Bucket interface`, `Hash policy`, `Observers` — and a name of its own only where none fits, `New versions`
   for an immutable container, `Statistics`), then `#### From mixin::<name>` for the members of each mixin.
   One row per function, its name a link to its page.
9. `## Non-member functions`, `## Deduction guides`, `## Specializations`.
10. `## Complexity` and, for a container, `## Iterator invalidation`.
11. `## Example`, then `## See also`.

A section the class has nothing for is left out. The class page has no code beside its members: their programs
are on their own pages.

### The page of a method

1. The breadcrumb and the title.
2. The signatures in a `cpp` block, without the include line. Overloads are numbered by a comment on the right,
   `// (1)`, in one column per block: the longest declaration line of the block (template, name and
   continuation lines, without a trailing comment of their own) plus 4 spaces. The number stands on the line
   that holds the function's name, never on a `template<…>` line or a continuation line; a remark of the line
   follows it, `// (2), implicitly declared`. An unnumbered declaration has no comment, and a block of one
   declaration has no number. A signature longer than 100 columns is broken after its `template<…>`, the
   declaration below it at the same column; one without `template<…>` is broken after a parameter's comma, the
   continuation aligned under the first parameter:

   ```cpp
   namespace sgcl::compress::sevenzip {
       expected<void, error> extract(const string& archive_path, const string& directory,         // (1)
                                     const options& o = {});
       async::task<expected<void, error>> async_extract(string archive_path, string directory,    // (2)
                                                        options o = {}) noexcept;
   }
   ```

   A `requires` clause naming a `detail` concept is left out of the signature and said in the description in
   words ("takes part only when `Compare` declares `is_transparent`").
3. The description. Overloads with a sentence each are a numbered list (`1.`, `2.`); a sentence shared by
   several is a bullet with their numbers, `- (1–2) …`. The text refers to an overload as `(7)`.
4. `## Parameters`: a table of the function's parameters, `Parameter | Description`. The template parameters of
   the method (`InputIt`, `Pred`) are not described.
5. `## Return value`, `## Complexity`, `## Exceptions`. These and the parameters are always there; one that has
   nothing to say says `None.`, except that a constructor has no `## Return value`. Where the overloads differ,
   a bullet per overload or range: `- (1–2) Constant, …`, `- (3) Linear in count, …`, with no alignment.
6. `## Exceptions` lists what is thrown as a bullet list (`length_error` when …, what the copy of `T` throws),
   and under it, as prose, the state the object is left in; a single exception is one sentence. Listed are the
   element's construction, copy and move (what the constructor, the copy or the move of `T` throws) and the
   library's own exceptions (`length_error`, `out_of_range`, `invalid_argument`, …). Not listed: running out of
   memory, managed or not — managed memory ends the program with a diagnostic
   ([collector](sgcl/core/collector/README.md#the-memory-limit)), and for
   memory outside the managed heap the program's own `operator new` and new-handler decide, which the library
   neither catches nor documents, while the library's own `malloc`/`realloc` and system objects end the program
   with a diagnostic (DESIGN 391, 409); nor the hash, the equality or the comparison of a container, whose `Hash`,
   `KeyEqual` and `Compare` must be noexcept. A function that cannot throw is declared `noexcept` (conditionally
   when it depends on `T`), its signature shows it, and its Exceptions say so: `None.`, or "What the copy
   constructor of `T` throws; none when it is noexcept."
7. `## Notes`, only when there is something to note: what is particular to the library (the buffer left to the
   collector, a slice that keeps the old elements).
8. `## Example`: one program.
9. `## See also`, ending with the link to the class page, `[vector](README.md)`.

### The page of a requirement and of a mixin

- A requirement: the breadcrumb, the title, the names block (the include line, the declaration with what it asks
  in a comment), the description, `## Satisfied by` (the types that satisfy it, then "Not by …"), `## Notes` when
  there is something to note, `## Example`, `## See also`.
- A mixin and the requirement of the same name link to each other in `## See also`; a requirement of a category
  without a page of its mixin (`bidirectional`, `random_access`, `contiguous`) links to the README of the mixins.

### The README of a module

The `README.md` of the module's directory (`core/README.md`, `net/http/README.md`) and of a group of its pages
(`core/mixin/README.md`); its names block names the namespace, which tells it from the page of a class.

1. The breadcrumb and the title, `# sgcl::<module>`.
2. A block with the module's header and its namespace: `#include "sgcl/core.h"   // namespace sgcl`.
3. The description, two or three paragraphs: what the module is for and the idea it rests on.
4. `## The rules`, with sections of their own where a group of classes has rules of its own (`### Containers`).
5. The tables, alphabetical within each group: Pointers, Functions, Classes, the containers, Mixins (the names
   alone, without `mixin::`), Requirements — the groups the module has — with the columns
   `<Kind> | Header | Description`: the first column is named after what the table lists (`Pointer`, `Function`,
   `Class`, `Container`, `Mixin`, `Requirement`), never `Page`.
6. `## See also`.

A README has no programs and no comparisons; the comparisons of performance are on the module's
`benchmarks.md`.

### The README of `docs/sgcl/`

The list of the modules, alphabetical, `Module | Description`; a module's header and namespace are on its own
README. The conventions of the programs on the pages are in the root `README.md` ("Reading the code on the
pages").

## 2. A program

- Every example is a complete program with `int main`, short, about one thing; a page has several short
  programs rather than one long one. A GET and a POST may share one when it stays under a screen. There is no
  fragment anywhere, under a signature or in a table: what is worth showing is a program.
- A program prints what it shows rather than asserting it: a fact about a type is printed as `true` or
  `false` (`println("{}", req::equatable<point>)`), and the page's output block says it.
- It compiles and runs as pasted, from the root of the tree, and prints what the page says (section 3).
- Includes: one header per module the program uses, the module's umbrella header — `sgcl/core.h`,
  `sgcl/io.h`, `sgcl/net.h`, `sgcl/net/http.h`, `sgcl/net/tls.h`, `sgcl/concurrent.h`,
  `sgcl/async.h`, `sgcl/time.h`, `sgcl/encoding.h`, `sgcl/hash.h`,
  `sgcl/crypto.h`, `sgcl/compress.h`, `sgcl/codec.h`, `sgcl/slog.h`,
  `sgcl/math.h`, `sgcl/txt.h`, `sgcl/immutable.h`. Never `sgcl/sgcl.h`, never a class
  header (`sgcl/io/print.h`, `sgcl/encoding/base64.h`), never a header of a module the program does not
  name. Core is a module like the others: `sgcl/core.h` when the program uses `range`, `thread`,
  `map` or another name of core that the other headers do not bring. A module header brings only what
  the module itself needs, so every used module is named.
- The include lines of sgcl stand in alphabetical order of their paths (`sgcl/async.h`, `sgcl/core.h`,
  `sgcl/io.h`, `sgcl/net/http.h`, `sgcl/net.h`…); a `<…>` header of the standard library after them.
- `using namespace sgcl;` after the includes, and no directive of a module of sgcl: every other module's name is
  written with its module (`net::http::client`, `encoding::base64`, `io::read_file`).
  `using namespace std::chrono_literals;` is allowed where a program writes `5s` or `100ms`.
- A program names nothing from a `detail` namespace: what an example needs and the public API does not give is a
  gap in the API, reported and filled, never a call into `detail`.
- A program keeps the rule of the handles ([the rules of core](sgcl/core/README.md#the-rules), rule 1): a
  `string`, a `vector`, a `slice`, an `io::file` or any other type that holds a tracked pointer lives on the
  stack or inside a managed object, never in a container of the standard library, a lambda copied into a
  `std::thread`, `std::async` or `std::function`, a global, a `static`, `new` memory or an exception object;
  a `root_ptr` or a `rooted` holds one there.
- A counted loop is `for (int i : range(n))`, never `for (int i = 0; i < n; ++i)`.
- Nothing the compiler converts by itself is written: no `*` on an `expected` passed on, no `slice<>(x)`,
  no `as_slice()`; `sgcl::tracked_ptr p = make_tracked<T>()`, not the type twice.
- A variable is not named after its type; a value is named by its type where `auto` would need a `*`.
- `force_collect()` appears only marked as optional, for demonstration.
- Comments: an explanatory comment stays short, two spaces after the code, or on its own line above the
  statement when the line would pass 100 columns. No comment repeats what the line prints when the page
  has an output block under the program (section 3). No comment is aligned to a far column. No line of a
  program is longer than 100 columns.
- A program that writes files does so in the checker's temporary directory (`--tmp` for the page in
  `tools/run_blocks.pages`), never in the tree.

## 3. The output of a program

Directly under a program, one of three things, by what the program prints:

- **A determined result** — the label `Output:` and a `text` block with the exact print. The checker compares
  it literally.
- **A result that varies but is not the user's data** (the time, the times of a cycle, ids, the order threads
  interleave in, the order of a hash map, random bytes, the memory ceiling, the number of cores) — the
  label `Sample output:` and a `text` block with one run's print. The checker compiles and runs the program
  and does not compare. No sentence above the block says it varies: the label says it.
- **A result that may show the user's data** (a listing of a directory, home paths, the hostname, the
  environment) — no output block at all. The checker compiles and runs the program.

The label is exactly `Output:` or `Sample output:` on its own line, followed by a blank line and a
```` ```text ```` block. No other label, no note inside the label. A program's answer to a request
(a server) is the label `A request to it:` with a `text` block of the `curl` lines and their answers, and
`Its output:` for what the server itself prints.

## 4. What the checker needs

`tools/run_blocks.py <page> --strict` compiles every program of the page with `-Wall
-Wno-unused-variable`, runs it, and compares its print with the `Output:` block. A page needs no
arguments: what its programs need — a fresh directory (`--tmp`), a local file server in place of a URL
(`--serve URL=FILE`), an echo server (`--echo URL`), a local TLS server (`--tls HOST:PORT`), a program not
to run (`--skip N`) — is listed for the page in `tools/run_blocks.pages`, one line per page, never in a
comment on the page. Under `--strict`, a page passes with `bad 0` and `unchecked 0`; a program left unread
by the checker (a label it does not know) is `unchecked`, which fails.

## 5. Checking a change

Before a page is handed in:

1. Every point of sections 1–3 holds for the whole page, not only for the lines changed.
2. `tools/run_blocks.py <page> --strict --changed`: `bad 0`, `unchecked 0`, and nothing left in the tree (`git
   status` clean apart from the pages). `--changed` compiles only the programs whose text or answer differs from
   the page in git HEAD: a change of prose, a title, a label or a names block costs no compilation, and a program
   that did not change is never compiled again.
3. `tools/lint_docs.py <page>` (or `--changed`): 0 findings. It checks the form of section 1 that a reader of
   the page would see broken — the breadcrumb, the title and its escaped `<`, HTML, the text and the target of
   every link, the header row of every table, a fragment of code, the numbering of overloads, the sections and
   their order, a README's programs and the order of its tables. A finding on a page in the old form is fixed by
   moving the page to this form, not by changing the lint.
4. `tools/lint_handles.py <page>`: 0 findings. It reads the programs of the page for the rule of the handles
   (section 2): a handle in a container of the standard library, in a lambda copied into a thread or a
   `std::function`, in a global, a `static`, `new` memory or an exception object. A Release run of the program
   never shows these; the lint and a Debug build do.
5. `grep` for what the change removed (an old name, `sgcl/sgcl.h` in a program, a class header, a `The
   output:` label, a note in a label) gives 0 over `docs/` and `README.md`; `detail::` inside the programs of
   the page gives 0.
6. What a page says about types — which ones satisfy a requirement, what a parameter takes — is checked by
   `static_assert`s in a scratch file before the page is handed in.
7. The change touches only what it says: a relabelling changes labels, a comment change changes comments,
   a code change is announced as one. A program's code is not rewritten to make its output stable; the
   output rule of section 3 covers that.
