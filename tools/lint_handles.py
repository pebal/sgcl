#!/usr/bin/env python3
# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
"""Tracked handles in unmanaged memory, found in the source without compiling it.

A tracked word (a tracked_ptr, and every type that holds one: string, vector,
slice, weak_ptr, io::file, net::connection, async::channel, codec::image…)
may live only on a thread's stack or inside a managed object; unmanaged
memory reaches one through root_ptr or rooted. A Debug build asserts it at
run time ("a tracked_ptr must live on the stack or inside a managed object");
a Release build never notices. This finds the usual ways in, one line per
finding, `path:line: error|check: what`:

  container    a handle as an element of a container of the standard library
               that allocates (std::vector, deque, list, forward_list, map,
               set, unordered_*, queue, stack, priority_queue), made by
               make_unique or make_shared, or a shared state's (promise,
               future), at any depth of the arguments
               (std::vector<std::pair<int, string>>), through aliases too;
               a std::shared_ptr or std::unique_ptr type alone is not one
               (to_shared aliases a managed object): what it was made by is
  layout       a handle inside std::variant or std::expected: their storage
               shares a word between alternatives (sgcl::variant and
               sgcl::expected keep the word at a fixed offset)
  thread       a handle copied into the state of std::thread, std::jthread or
               std::async: a lambda that captures one by value (by name, by
               `[=]` and use, by an init-capture), an argument passed by value,
               a lambda stored in a std::function the same way; a helper of
               the tree that forwards its callable to a thread by value is
               followed to its callers (check)
  new          `new T` of a handle or of a class that holds one
  throw        an exception object that is a handle or holds one (io::error
               holds a string): the runtime allocates it
  static       a handle (or a class that holds one) with static or thread
               storage: a variable at namespace scope, a static local, a
               static data member
  fixture      a handle as a member of a gtest or benchmark fixture: the
               framework allocates fixtures with new

The handle types are derived from sgcl/: every class whose layout holds a
tracked word (a member, a base, an alias of one, a template argument of a
class that keeps its argument in place) is one, and so is every class of a
scanned file built the same way (a test's `struct Item { string name; }`).
What the scan cannot see is listed below (EXPLICIT_*), with the reason.
`--list-types` prints what it knows. A type is resolved as the compiler
would find it: through the file's namespaces, `using` directives and
declarations, aliases, and the tree's own headers it includes
(tests/types.h brings `using namespace sgcl`); the return type of a
function of the tree names what `auto x = f(...)` holds.

`error` is a finding about a type the scan knows. `check` is one to read:
a type inferred (`auto x = f(...)`, a structured binding, a helper followed
to its callers), or a handle only through a slice, a function or an any,
which hold a word only by what they hold at run time (a slice over
unmanaged bytes has none). A line that is right as it is carries
`// lint-handles: ok <reason>`, on the line or on a comment line above it.

  tools/lint_handles.py                  sgcl/, tests/, benchmarks/, docs/ and README.md
  tools/lint_handles.py PATH ...         those files, or the files under PATH
  tools/lint_handles.py --root DIR ...   the tree at DIR (PATHs relative to it)
  tools/lint_handles.py --list-types     the handle types it derived
  tools/lint_handles.py --no-docs        without the ```cpp blocks of the docs

The exit status is 1 when anything was found.
"""

import bisect
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SUPPRESS = 'lint-handles: ok'

# --- What the scan cannot decide -----------------------------------------

EXPLICIT_MAYBE_FUNCTION = 'a closure that holds a pointer kept in the word of detail::ValueStorage, as bytes'

# Hold a tracked word the scan does not see as a member of a known type
EXPLICIT_HANDLE = {
    'sgcl::tracked_ptr': 'the tracked word itself (its member is the raw pointer)',
    'sgcl::function': EXPLICIT_MAYBE_FUNCTION,
    'sgcl::move_only_function': EXPLICIT_MAYBE_FUNCTION,
    'sgcl::any': EXPLICIT_MAYBE_FUNCTION,
    'sgcl::detail::Pointer': 'the word of the collector (root cells, the atomics); only the engine keeps one',
}

# Keep their template arguments in place: a handle when an argument is one
EXPLICIT_TRANSPARENT = {
    'sgcl::array': 'the elements in place (detail::ArrayElements)',
    'sgcl::variant': 'the alternatives in raw storage of its own (variant.h)',
    'sgcl::expected': 'the value or the error in a union',
    'sgcl::atomic': 'the word of the handle it is specialized for',
}

# A tracked word only by what they hold at run time. A slice over unmanaged
# bytes (a pointer and a length, bytes_of, a std::vector's data) holds a null
# word that never registers nor asserts (slice.h: the unregistered null), so
# it may lie anywhere; a slice of a string or an sgcl::vector holds its owner.
# A function or an any keeps a closure of plain data in its buffer and makes
# a managed node, its word in the buffer, only for one that holds a pointer.
# A finding that holds a handle only through one of these is a `check`.
EXPLICIT_MAYBE = {
    'sgcl::slice': 'a word only with an owner (a slice of managed memory)',
    'sgcl::function': 'a word only for a closure that holds a pointer (detail::ValueStorage keeps it as bytes)',
    'sgcl::move_only_function': 'as sgcl::function',
    'sgcl::any': 'as sgcl::function',
}

# MODE[0]: 0 counts the views as handles, 1 does not (what is certain)
MODE = [0]

# Never a handle in unmanaged memory: the holders the rule allows
EXPLICIT_ALLOWED = {
    'sgcl::root_ptr': 'the allowed holder: a root cell of a managed CellBlock, a raw pointer here',
    'sgcl::rooted': 'the allowed holder: a root_ptr to a managed copy',
    'sgcl::atomic_ref': 'a reference to a word that lives elsewhere',
    'sgcl::thread': 'its closure lives in a managed node, held by a root_ptr (thread.h)',
}

# The standard library, by what it does with a template argument
STD_HEAP = {'vector', 'deque', 'list', 'forward_list', 'map', 'multimap', 'set', 'multiset',
            'unordered_map', 'unordered_multimap', 'unordered_set', 'unordered_multiset',
            'queue', 'stack', 'priority_queue',
            'make_unique', 'make_shared', 'allocate_shared', 'make_unique_for_overwrite',
            'promise', 'future', 'shared_future', 'flat_map', 'flat_set'}
STD_LAYOUT = {'variant', 'expected'}
STD_TRANSPARENT = {'pair', 'tuple', 'optional', 'array', 'atomic'}
STD_FUNCTION = {'function', 'move_only_function', 'copyable_function', 'packaged_task'}
STD_THREAD = {'thread', 'jthread', 'async'}
STD_REF = {'ref', 'cref'}

KEYWORDS = set('''alignas alignof and asm auto bool break case catch char char8_t char16_t char32_t class
co_await co_return co_yield concept const consteval constexpr constinit const_cast continue decltype default
delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long
mutable namespace new noexcept not nullptr operator or private protected public register reinterpret_cast
requires return short signed sizeof static static_assert static_cast struct switch template this
thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while'''.split())
BUILTIN = {'bool', 'char', 'char8_t', 'char16_t', 'char32_t', 'double', 'float', 'int', 'long', 'short', 'signed',
           'unsigned', 'void', 'wchar_t'}
SPECIFIERS = {'static', 'inline', 'constexpr', 'constinit', 'consteval', 'mutable', 'thread_local', 'extern',
              'const', 'volatile', 'typename', 'virtual', 'explicit', 'friend', 'register'}

# --- Text ------------------------------------------------------------------

_LEX = re.compile(r'//[^\n]*|/\*.*?\*/|R"([^(\s]{0,16})\(.*?\)\1"|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def _blank(m):
    t = m.group(0)
    if t.startswith('/'):
        return re.sub(r'[^\n]', ' ', t)
    q = '"' if t[-1] == '"' else "'"
    return q + re.sub(r'[^\n]', ' ', t[1:-1]) + q


def strip_source(text):
    """Comments and literals blanked (lengths and lines kept), the preprocessor's lines blanked, and of
    each #if only its first branch kept (both branches would unbalance the braces)."""
    s = _LEX.sub(_blank, text)
    out, stack, cont = [], [], False   # stack: per #if, whether the current branch is dropped
    for line in s.split('\n'):
        stripped = line.lstrip()
        dropped = any(stack)
        if cont or stripped.startswith('#'):
            directive = re.match(r'#\s*(\w+)', stripped) if not cont else None
            if directive:
                d = directive.group(1)
                if d in ('if', 'ifdef', 'ifndef'):
                    zero = d == 'if' and re.match(r'#\s*if\s+0\b', stripped) is not None
                    stack.append(zero)
                elif d in ('elif', 'else', 'elifdef', 'elifndef') and stack:
                    stack[-1] = True
                elif d == 'endif' and stack:
                    stack.pop()
            cont = line.rstrip().endswith('\\')
            out.append(' ' * len(line))
            continue
        out.append(' ' * len(line) if dropped else line)
    return '\n'.join(out)


def matching(s, i, open_c, close_c, stop=''):
    """The index past the bracket that closes s[i] (open_c), or -1."""
    depth = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == open_c:
            depth += 1
        elif c == close_c:
            depth -= 1
            if depth == 0:
                return i + 1
        elif c in stop:
            return -1
        i += 1
    return -1


def angle_end(s, i, limit=4000):
    """s[i] is '<': the index past the '>' that closes it, or -1 when it is not a template's list."""
    depth, paren, n = 0, 0, min(len(s), i + limit)
    j = i
    while j < n:
        c = s[j]
        if c == '(' or c == '[':
            paren += 1
        elif c == ')' or c == ']':
            paren -= 1
            if paren < 0:
                return -1
        elif paren == 0:
            if c == '<':
                depth += 1
            elif c == '>':
                if j > 0 and s[j - 1] == '-':   # ->
                    pass
                else:
                    depth -= 1
                    if depth == 0:
                        return j + 1
            elif c in ';{}':
                return -1
            elif c == '&' and j + 1 < n and s[j + 1] == '&' and depth == 1 and not re.match(r'\s*[>,]', s[j + 2:j + 6]):
                return -1   # a && b: a comparison, not a template
            elif c == '|' and j + 1 < n and s[j + 1] == '|':
                return -1
        j += 1
    return -1


def split_top(s, sep=','):
    """s split at sep outside (), [], {}, <>."""
    parts, depth, start = [], 0, 0
    for i, c in enumerate(s):
        if c in '([{<':
            depth += 1
        elif c in ')]}>':
            if not (c == '>' and i > 0 and s[i - 1] == '-'):
                depth -= 1
        elif c == sep and depth == 0:
            parts.append(s[start:i])
            start = i + 1
    parts.append(s[start:])
    return parts


def strip_template_prefix(h):
    """'template<…> requires …' removed from the front of a head; the parameter text returned too."""
    params = None
    while True:
        m = re.match(r'\s*template\s*<', h)
        if not m:
            break
        e = angle_end(h, m.end() - 1)
        if e < 0:
            break
        if params is None:
            params = h[m.end():e - 1]
        h = h[e:]
    return h, params


def template_params(text):
    """[(name, is_pack)] of a template's parameter list."""
    out = []
    if not text:
        return out
    for p in split_top(text):
        p = p.strip()
        if not p:
            continue
        head = split_top(p, '=')[0]
        names = re.findall(r'[A-Za-z_]\w*', re.sub(r'<.*>', '', head))
        if not names:
            continue
        name = names[-1]
        if name in ('class', 'typename', 'auto') or name in KEYWORDS:
            continue
        out.append((name, '...' in head))
    return out


# --- Types -------------------------------------------------------------------

_TOK = re.compile(r'\s*(::|\.\.\.|->|&&|[A-Za-z_]\w*|\d[\w.\']*|.)', re.S)


def tokenize(s):
    return [t for t in _TOK.findall(s) if t.strip()]


class Node:
    __slots__ = ('comps', 'glob', 'ptr', 'func', 'text')

    def __init__(self):
        self.comps = []     # [(name, args or None)]
        self.glob = False
        self.ptr = False    # a pointer or a reference: holds no word of its own
        self.func = False
        self.text = ''

    def name(self):
        return ('::' if self.glob else '') + '::'.join(c[0] for c in self.comps)


def parse_type(toks, i=0):
    """A type from toks[i:]: (Node or None, the index after it)."""
    n = len(toks)
    node = Node()
    start = i
    while i < n and toks[i] in ('const', 'volatile', 'typename', 'struct', 'class', 'union', 'enum',
                                'mutable', 'static', 'inline', 'constexpr', 'thread_local', 'extern', 'constinit'):
        i += 1
    if i < n and toks[i] == 'decltype':
        i += 1
        if i < n and toks[i] == '(':
            depth = 0
            while i < n:
                if toks[i] == '(':
                    depth += 1
                elif toks[i] == ')':
                    depth -= 1
                    if depth == 0:
                        i += 1
                        break
                i += 1
        return None, i
    if i < n and toks[i] == '::':
        node.glob = True
        i += 1
    while i < n and re.match(r'[A-Za-z_]\w*$', toks[i]):
        name = toks[i]
        i += 1
        args = None
        if i < n and toks[i] == '<' and name not in KEYWORDS:
            args, i = parse_args(toks, i)
        node.comps.append((name, args))
        if i + 1 < n and toks[i] == '::' and (re.match(r'[A-Za-z_]\w*$', toks[i + 1]) or toks[i + 1] == 'template'):
            i += 1
            if toks[i] == 'template':
                i += 1
            continue
        break
    if not node.comps:
        return None, i
    # long long, unsigned int: one name
    while i < n and toks[i] in ('int', 'long', 'short', 'char', 'double'):
        i += 1
    while i < n:
        t = toks[i]
        if t in ('const', 'volatile', '...'):
            i += 1
        elif t in ('*', '&', '&&'):
            node.ptr = True
            i += 1
        elif t == '(':
            # a function type, or a pointer to one
            node.func = True
            depth = 0
            while i < n:
                if toks[i] == '(':
                    depth += 1
                elif toks[i] == ')':
                    depth -= 1
                    if depth == 0:
                        i += 1
                        break
                i += 1
        elif t == '[':
            while i < n and toks[i] != ']':
                i += 1
            i += 1
        else:
            break
    node.text = pretty(toks[start:i])
    return node, i


def pretty(toks):
    out = ''
    for t in toks:
        if out and re.match(r'\w', t) and re.match(r'.*\w$', out):
            out += ' '
        elif out and t not in ('<', '>', ',', '::', '*', '&', '&&', ')', '(', '[', ']', '...') and out[-1] == ',':
            out += ' '
        out += t
    return out


def parse_args(toks, i):
    """toks[i] is '<': ([Node or None per argument], the index after '>')."""
    args = []
    i += 1
    n = len(toks)
    while i < n:
        if toks[i] == '>':
            return args, i + 1
        node, j = parse_type(toks, i)
        k = j
        depth = 0
        while k < n:
            t = toks[k]
            if t in ('(', '[', '<'):
                depth += 1
            elif t in (')', ']'):
                depth -= 1
            elif t == '>':
                if depth == 0:
                    break
                depth -= 1
            elif t == ',' and depth == 0:
                break
            k += 1
        if node is not None and k > j and toks[j] != '...':
            node = None        # an expression (N + 1, sizeof(T)), not a type
        args.append(node)
        if k < n and toks[k] == ',':
            k += 1
        i = k
    return args, n


def type_of_text(text):
    toks = tokenize(text)
    if not toks:
        return None
    node, i = parse_type(toks, 0)
    return node


# --- Entities and scopes -------------------------------------------------

class Scope:
    def __init__(self, kind, parent=None, name='', fallback=None):
        self.kind = kind            # root, ns, class, func, alias
        self.parent = parent
        self.name = name
        self.names = {}
        self.usings = []            # (written name, scope)
        self.fallback = fallback    # the shared namespace a file-local one reopens
        self.funcs = {}             # name -> [(return type text, scope)]

    def qual(self):
        parts, s = [], self
        while s is not None:
            if s.name:
                parts.append(s.name)
            s = s.parent
        return '::'.join(reversed(parts))


class Namespace(Scope):
    def __init__(self, parent, name, fallback=None):
        Scope.__init__(self, 'ns', parent, name, fallback)


class Class(Scope):
    def __init__(self, parent, name, params, path, line):
        Scope.__init__(self, 'class', parent, name)
        self.params = []
        for pname, pack in params:
            p = Param(pname, pack, self)
            self.params.append(p)
            self.names[pname] = p
        self.members = []           # (type text, line)
        self.bases = []             # type texts
        self._always = [False, False]     # per MODE: views counted, views not counted
        self._dep = [set(), set()]          # Params
        self._why = ['', '']
        self.path = path
        self.line = line
        self.specs = []
        self.fixture = False

    always = property(lambda self: self._always[MODE[0]], lambda self, v: self._always.__setitem__(MODE[0], v))
    dep = property(lambda self: self._dep[MODE[0]], lambda self, v: self._dep.__setitem__(MODE[0], v))
    why = property(lambda self: self._why[MODE[0]], lambda self, v: self._why.__setitem__(MODE[0], v))


class Alias:
    def __init__(self, target, scope, params=None):
        self.target = target
        self.params = []
        self.scope = scope
        if params:
            self.scope = Scope('alias', scope)
            for pname, pack in params:
                p = Param(pname, pack, self.scope)
                self.params.append(p)
                self.scope.names[pname] = p
        self._node = None

    def node(self):
        if self._node is None:
            self._node = type_of_text(self.target) or False
        return self._node


class UsingDecl:
    def __init__(self, target, scope):
        self.target = target
        self.scope = scope


class NsAlias:
    def __init__(self, target, scope):
        self.target = target
        self.scope = scope


class Param:
    def __init__(self, name, pack, owner):
        self.name = name
        self.pack = pack
        self.owner = owner


class Std:
    def __init__(self, name):
        self.name = name
        self.kind = ('heap' if name in STD_HEAP else 'layout' if name in STD_LAYOUT else
                     'transparent' if name in STD_TRANSPARENT else 'function' if name in STD_FUNCTION else
                     'thread' if name in STD_THREAD else 'ref' if name in STD_REF else 'other')


class Universe:
    def __init__(self):
        self.root = Scope('root')
        std = Namespace(self.root, 'std')
        self.root.names['std'] = std
        for n in STD_HEAP | STD_LAYOUT | STD_TRANSPARENT | STD_FUNCTION | STD_THREAD | STD_REF:
            std.names[n] = Std(n)
        self.classes = []
        self.headers = {}       # path -> File: the tree's own headers a test includes


# --- Lookup --------------------------------------------------------------

def deref(ent, depth=0):
    """A using-declaration, a namespace alias or an alias without arguments followed to what it names."""
    while depth < 12:
        depth += 1
        if isinstance(ent, UsingDecl):
            node = type_of_text(ent.target)
            ent = resolve(node, ent.scope, depth) if node else None
        elif isinstance(ent, NsAlias):
            node = type_of_text(ent.target)
            ent = resolve(node, ent.scope, depth) if node else None
        else:
            return ent
    return None


_MEMBER = {}
_LOOKUP = {}


def using_targets(scope):
    """The namespaces a scope's using-directives name, resolved once."""
    ut = getattr(scope, '_ut', None)
    if ut is not None:
        return ut
    scope._ut = []
    out = []
    for u, us in scope.usings:
        if isinstance(u, Scope):
            out.append(u)       # an included header's file scope
            continue
        nsn = type_of_text(u)
        ns = deref(resolve(nsn, us)) if nsn else None
        if isinstance(ns, Scope) and ns is not scope:
            out.append(ns)
    scope._ut = out
    return out


def member(ent, name, depth=0):
    key = (id(ent), name)
    if key in _MEMBER:
        return _MEMBER[key]
    _MEMBER[key] = None
    r = _member(ent, name, depth)
    _MEMBER[key] = r
    return r


def _member(ent, name, depth):
    ent = deref(ent, depth)
    if isinstance(ent, Alias) and not ent.params:
        node = ent.node()
        ent = deref(resolve(node, ent.scope, depth + 1), depth + 1) if node else None
    if isinstance(ent, Scope):
        if name in ent.names:
            return ent.names[name]
        if ent.fallback is not None and name in ent.fallback.names:
            return ent.fallback.names[name]
        if isinstance(ent, Class) and depth < 6:
            for b in ent.bases:
                node = type_of_text(b)
                if node:
                    be = deref(resolve(node, ent.parent, depth + 1), depth + 1)
                    if isinstance(be, Class) and be is not ent:
                        r = member(be, name, depth + 1)
                        if r is not None:
                            return r
        if ent.kind in ('ns', 'file'):
            for ns in using_targets(ent):
                r = member(ns, name, depth + 1)
                if r is not None:
                    return r
    return None


def lookup(name, scope, depth=0):
    key = (id(scope), name)
    if key in _LOOKUP:
        return _LOOKUP[key]
    _LOOKUP[key] = None
    r = _lookup(name, scope, depth)
    _LOOKUP[key] = r
    return r


def _lookup(name, scope, depth):
    s = scope
    while s is not None:
        if name in s.names:
            return s.names[name]
        if s.fallback is not None and name in s.fallback.names:
            return s.fallback.names[name]
        if isinstance(s, Class):
            r = member(s, name, depth + 1)
            if r is not None:
                return r
        for ns in using_targets(s):
            r = member(ns, name, depth + 1)
            if r is not None:
                return r
        if s.parent is not None:
            # the enclosing scope's answer, when it is known already
            known = _LOOKUP.get((id(s.parent), name))
            if known is not None:
                return known
        s = s.parent
    return None


def lookup_funcs(node, scope):
    """The overloads a function's name in a call names: [(return type text, scope)]."""
    if node is None or not node.comps:
        return []
    name = node.comps[-1][0]
    if len(node.comps) > 1:
        owner = Node()
        owner.comps = node.comps[:-1]
        owner.glob = node.glob
        ent = deref(resolve(owner, scope))
        if isinstance(ent, Alias) and not ent.params and ent.node():
            ent = deref(resolve(ent.node(), ent.scope))
        out = []
        seen = set()
        todo = [ent]
        while todo:
            e = todo.pop()
            if not isinstance(e, Scope) or id(e) in seen:
                continue
            seen.add(id(e))
            out += e.funcs.get(name, [])
            if e.fallback is not None:
                todo.append(e.fallback)
            todo += using_targets(e)
        return out
    s = scope
    seen = set()
    while s is not None:
        todo = [s]
        out = []
        while todo:
            e = todo.pop()
            if id(e) in seen:
                continue
            seen.add(id(e))
            out += e.funcs.get(name, [])
            if e.fallback is not None:
                todo.append(e.fallback)
            if e.kind in ('ns', 'file', 'root'):
                todo += using_targets(e)
        if out:
            return out
        s = s.parent
    return []


def returns_handle(node, scope):
    """True when every overload of the function a call names returns a handle."""
    rets = lookup_funcs(node, scope)
    if not rets:
        return False
    for ret, fs in rets:
        if evaluate(type_of_text(ret), fs) is not True:
            return False
    return True


def root_of(scope):
    while scope.parent is not None:
        scope = scope.parent
    return scope


def resolve(node, scope, depth=0):
    if node is None or not node.comps or depth > 10:
        return None
    first = node.comps[0][0]
    if node.glob:
        ent = member(root_of(scope), first, depth)
    else:
        ent = lookup(first, scope, depth)
    for name, _ in node.comps[1:]:
        if ent is None:
            return None
        ent = member(ent, name, depth)
    return ent


def qual_of(ent):
    if isinstance(ent, Scope):
        return ent.qual()
    if isinstance(ent, Std):
        return 'std::' + ent.name
    return ''


# --- Is it a handle -------------------------------------------------------

def orv(a, b):
    if a is True or b is True:
        return True
    if a and b:
        return a | b
    return a or b


def evaluate(node, scope, env=None, depth=0):
    """True when the type holds a tracked word in its own layout, a set of Params when that depends on
    them, else False."""
    if node is None or node is False or node.ptr or node.func or depth > 14:
        return False
    ent = deref(resolve(node, scope, depth), depth)
    args = node.comps[-1][1] or []
    return evaluate_entity(ent, args, scope, env, depth)


def evaluate_entity(ent, args, scope, env, depth):
    if ent is None:
        return False
    if isinstance(ent, Param):
        if env and ent in env:
            bound = env[ent]
            if isinstance(bound, list):
                r = False
                for b in bound:
                    r = orv(r, evaluate(b[0], b[1], b[2], depth + 1))
                return r
            return evaluate(bound[0], bound[1], bound[2], depth + 1)
        return {ent}
    if isinstance(ent, Std):
        if ent.kind in ('transparent', 'layout'):
            r = False
            for a in args:
                r = orv(r, evaluate(a, scope, env, depth + 1))
            return r
        return False
    if isinstance(ent, Alias):
        node = ent.node()
        if not node:
            return False
        new_env = bind(ent.params, args, scope, env)
        return evaluate(node, ent.scope, new_env, depth + 1)
    if isinstance(ent, Class):
        q = ent.qual()
        if q in EXPLICIT_ALLOWED or MODE[0] and q in EXPLICIT_MAYBE:
            return False
        if q in EXPLICIT_HANDLE or ent.always:
            return True
        if q in EXPLICIT_TRANSPARENT:
            r = False
            for a in args:
                r = orv(r, evaluate(a, scope, env, depth + 1))
            return r
        r = False
        for k, p in enumerate(ent.params):
            if p in ent.dep:
                chosen = args[k:] if p.pack else args[k:k + 1]
                for a in chosen:
                    r = orv(r, evaluate(a, scope, env, depth + 1))
        return r
    return False


def bind(params, args, scope, env):
    out = dict(env) if env else {}
    for k, p in enumerate(params):
        if p.pack:
            out[p] = [(a, scope, env) for a in args[k:]]
        elif k < len(args):
            out[p] = (args[k], scope, env)
    return out


def is_handle(node, scope):
    return evaluate(node, scope) is True


def certainty(node, scope, env=None):
    """'error' when the type holds a tracked word for certain, 'check' when only through a view (a slice),
    None when it holds none."""
    if evaluate(node, scope, env) is not True:
        return None
    MODE[0] = 1
    try:
        strict = evaluate(node, scope, env) is True
    finally:
        MODE[0] = 0
    return 'error' if strict else 'check'


VIEW_NOTE = ' (check: only a slice with an owner, or a function or any with a closure that holds a pointer, holds a word)'


# --- A file --------------------------------------------------------------

class Block:
    __slots__ = ('open', 'close', 'parent', 'head_start', 'kind', 'name', 'scope', 'children', 'tparams', 'head', 'prefix')

    def __init__(self, open_, parent, head_start, head):
        self.open = open_
        self.close = None
        self.parent = parent
        self.head_start = head_start
        self.head = head
        self.kind = None
        self.name = ''
        self.scope = None
        self.children = []
        self.tparams = None
        self.prefix = []


_LAMBDA_TAIL = re.compile(r'\]\s*(?:<[^<>]*>\s*)?(?:\([^()]*(?:\([^()]*\)[^()]*)*\)\s*)?(?:(?:mutable|constexpr|consteval|static|noexcept(?:\s*\([^()]*\))?)\s*)*(?:->\s*[\w:<>,\s*&]+)?\s*$')
_CONTROL = re.compile(r'\s*(?:if|for|while|switch|catch|else|do|try)\b')


def clean_head(h):
    h = re.sub(r'\[\[.*?\]\]', ' ', h, flags=re.S)
    h = re.sub(r'^\s*(?:(?:public|private|protected)\s*:(?!:)\s*)+', '', h)
    return h


def classify(block, parent_kind):
    h = clean_head(block.head)
    h2, tp = strip_template_prefix(h)
    block.tparams = tp
    st = h2.strip()
    if re.match(r'(?:inline\s+)?namespace\b', st):
        m = re.match(r'(?:inline\s+)?namespace\s+([\w:]+)', st)
        block.kind = 'ns'
        block.name = m.group(1) if m else ''
        return
    if re.match(r'extern\s*"\s*"\s*$', st):
        block.kind = 'transparent'
        return
    if re.match(r'(?:typedef\s+)?enum\b', st):
        block.kind = 'enum'
        return
    if _is_lambda(st):
        block.kind = 'lambda'
        return
    ch = parse_class_head(st)
    if ch is not None:
        prefix, name, spec, bases = ch
        block.kind = 'spec' if spec else 'class'
        block.name = name
        block.prefix = prefix
        block.head = bases
        return
    if parent_kind in ('root', 'ns', 'class', 'spec', 'transparent'):
        if _paren_outside_angles(st) or re.search(r'\boperator\b', st):
            block.kind = 'func'
        else:
            block.kind = 'init'
        return
    if st == '' or _CONTROL.match(st):
        block.kind = 'control'
    elif _is_lambda(st):
        block.kind = 'lambda'
    elif re.search(r'\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->[^{]*)?$', st) and not re.search(r'[=,(]\s*\w+\s*\(', st):
        block.kind = 'func'   # a local class's member function, a requires-expression
    else:
        block.kind = 'init'


def parse_class_head(st):
    """(the qualifying names, the name, a specialization, the bases) of a class's head, or None."""
    m = re.match(r'(?:typedef\s+)?(?:requires\b.*?\s)?\b(class|struct|union)\b', st, re.S)
    if not m or re.search(r'\benum\b', st[:m.start(1)]):
        return None
    rest = st[m.end():]
    rest = re.sub(r'^\s*(?:\[\[.*?\]\]\s*)*', '', rest, flags=re.S)
    rest = re.sub(r'^\s*alignas\s*\([^()]*(?:\([^()]*\)[^()]*)*\)', '', rest)
    comps = []
    spec = False
    while True:
        mm = re.match(r'\s*([A-Za-z_]\w*)', rest)
        if not mm or mm.group(1) == 'final':
            break
        name = mm.group(1)
        rest = rest[mm.end():]
        args = False
        mm = re.match(r'\s*<', rest)
        if mm:
            e = angle_end(rest, mm.end() - 1)
            if e < 0:
                return None
            rest = rest[e:]
            args = True
        comps.append((name, args))
        mm = re.match(r'\s*::(?!\s*$)', rest)
        if mm:
            rest = rest[mm.end():]
            continue
        break
    rest = re.sub(r'^\s*final\b', '', rest).strip()
    if rest and not (rest.startswith(':') and not rest.startswith('::')):
        return None
    if not comps:
        return [], '', False, rest
    return [c[0] for c in comps[:-1]], comps[-1][0], comps[-1][1], rest


def _is_lambda(st):
    m = _LAMBDA_TAIL.search(st)
    if not m:
        return False
    # the '[' that opens the capture list: not an index (a[i]) nor an attribute
    end = m.start()
    depth = 0
    i = end
    while i >= 0:
        c = st[i]
        if c == ']':
            depth += 1
        elif c == '[':
            depth -= 1
            if depth == 0:
                before = st[:i].rstrip()
                return before == '' or before[-1] in '=(,{;:?&|!+-*/%<>' or before.endswith('return') or before.endswith('co_return')
        i -= 1
    return False


def _paren_outside_angles(st):
    depth = 0
    for i, c in enumerate(st):
        if c == '<':
            depth += 1
        elif c == '>' and depth > 0:
            depth -= 1
        elif c == '(' and depth == 0:
            return True
        elif c == '=' and depth == 0 and not (i + 1 < len(st) and st[i + 1] == '='):
            return False
    return False


class File:
    def __init__(self, path, text, universe, local, line_offset=0, display=None):
        self.path = path
        self.display = display or shown(path)
        self.text = text
        self.lines = text.split('\n')
        self.s = strip_source(text)
        self.nl = [m.start() for m in re.finditer('\n', self.s)]
        self.universe = universe
        self.local = local              # a test or a page: its namespaces are its own
        self.line_offset = line_offset
        self.classes = []
        self.statics = []               # (type text, offset, scope, what)
        self.findings = {}
        self.local_ns = {}
        self.root = Scope('file', universe.root) if local else universe.root
        self.blocks = []
        self.build()
        if local and display is None:
            self.include_headers()

    def include_headers(self):
        """The tree's headers this file includes, outside sgcl/: their names seen from here (tests/types.h
        brings `using namespace sgcl`)."""
        for m in re.finditer(r'^\s*#\s*include\s*"([^"]+)"', self.text, re.M):
            inc = m.group(1)
            for cand in (os.path.join(os.path.dirname(self.path), inc), os.path.join(ROOT, inc)):
                cand = os.path.normpath(cand)
                if os.path.isfile(cand):
                    break
            else:
                continue
            if cand.startswith(os.path.join(ROOT, 'sgcl') + os.sep):
                continue
            h = header_file(cand, self.universe)
            if h is not None and h is not self:
                self.root.usings.append((h.root, self.root))

    def line(self, off):
        return bisect.bisect_right(self.nl, off - 1) + 1 if off > 0 else 1

    def report(self, off, sev, rule, what):
        ln = self.line(off)
        src = self.lines[ln - 1] if ln - 1 < len(self.lines) else ''
        if SUPPRESS in src:
            return
        if ln >= 2 and SUPPRESS in self.lines[ln - 2] and self.lines[ln - 2].strip().startswith('//'):
            return
        key = ln + self.line_offset
        if key not in self.findings:
            self.findings[key] = (sev, rule, what)

    # the blocks and their scopes
    def build(self):
        s = self.s
        frames = [[0, {0: -1}]]
        stack = []
        top = []
        for m in re.finditer(r'[;{}()]', s):
            c = m.group()
            p = m.start()
            f = frames[-1]
            if c == '(':
                f[0] += 1
                f[1][f[0]] = p
            elif c == ')':
                if f[0] > 0:
                    f[0] -= 1
            elif c == ';':
                f[1][f[0]] = p
            elif c == '{':
                hs = f[1].get(f[0], -1) + 1
                b = Block(p, stack[-1] if stack else None, hs, s[hs:p])
                (stack[-1].children if stack else top).append(b)
                self.blocks.append(b)
                stack.append(b)
                frames.append([0, {0: p}])
            else:
                if stack:
                    b = stack.pop()
                    b.close = p
                if len(frames) > 1:
                    frames.pop()
                f = frames[-1]
                f[1][f[0]] = p
        for b in stack:
            b.close = len(s)
        self.top = top
        self.opens = [b.open for b in self.blocks]
        self._scopes(top, self.root, 'root')
        self._statements(None, top, self.root)

    def _ns_scope(self, parent_scope, name):
        cur = parent_scope
        for part in name.split('::'):
            part = part.strip()
            if not part:
                continue
            if self.local:
                key = (id(cur), part)
                if key not in self.local_ns:
                    shared = member(cur.fallback if cur.fallback is not None else (cur if cur is not self.root else self.universe.root), part)
                    shared = shared if isinstance(shared, Scope) else None
                    ns = Namespace(cur, part, shared)
                    self.local_ns[key] = ns
                    cur.names.setdefault(part, ns)
                cur = self.local_ns[key]
            else:
                ex = cur.names.get(part)
                if not isinstance(ex, Namespace):
                    ex = Namespace(cur, part)
                    cur.names[part] = ex
                cur = ex
        return cur

    def _scopes(self, blocks, scope, kind):
        for b in blocks:
            classify(b, kind)
            if b.kind == 'ns':
                b.scope = self._ns_scope(scope, b.name) if b.name else scope
            elif b.kind == 'transparent':
                b.scope = scope
            elif b.kind in ('class', 'spec'):
                owner = scope
                for part in b.prefix:
                    nxt = owner.names.get(part) if isinstance(owner, Scope) else None
                    if nxt is None and isinstance(owner, Scope) and owner.fallback is not None:
                        nxt = owner.fallback.names.get(part)
                    if not isinstance(nxt, Scope):
                        owner = scope
                        break
                    owner = nxt
                c = Class(owner, b.name, template_params(b.tparams), self.display, self.line(b.open) + self.line_offset)
                c.bases = [re.sub(r'^\s*(?:public|private|protected|virtual)\s+(?:virtual\s+)?', '', x).strip()
                           for x in split_top(b.head.lstrip(':'))] if b.head.strip() else []
                c.bases = [x for x in c.bases if x]
                b.scope = c
                if b.kind == 'spec':
                    prim = owner.names.get(b.name) if b.name else None
                    if isinstance(prim, Class):
                        prim.specs.append(c)
                    elif b.name and not isinstance(prim, Std) and prim is None:
                        owner.names[b.name] = c
                elif b.name:
                    ex = owner.names.get(b.name)
                    if not isinstance(ex, Std):
                        owner.names[b.name] = c
                        if isinstance(ex, Class):
                            c.specs.extend(ex.specs)
                self.classes.append(c)
                if not self.local:
                    self.universe.classes.append(c)
            elif b.kind == 'enum':
                b.scope = scope
            else:
                if b.kind == 'func' and kind in ('root', 'ns', 'class', 'spec', 'transparent'):
                    self.function(b.head, scope)
                fs = Scope('func', scope)
                for pname, pack in template_params(b.tparams):
                    fs.names[pname] = Param(pname, pack, fs)
                b.scope = fs
            self._scopes(b.children, b.scope, b.kind if b.kind != 'transparent' else kind)

    def segments(self, start, end, children):
        """The statements directly in s[start:end], children blocks collapsed: [(text, offset)]."""
        s = self.s
        out = []
        cur, cur_off = [], None
        pos = start
        anon = []

        def flush():
            nonlocal cur, cur_off
            t = ''.join(cur)
            if t.strip():
                out.append((t, cur_off))
            cur, cur_off = [], None

        def add(a, b):
            nonlocal cur_off
            piece = s[a:b]
            depth = 0
            last = 0
            for i, c in enumerate(piece):
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth = max(0, depth - 1)
                elif c == ';' and depth == 0:
                    chunk = piece[last:i]
                    if chunk.strip() and cur_off is None:
                        cur_off = a + last + (len(chunk) - len(chunk.lstrip()))
                    cur.append(chunk)
                    flush()
                    last = i + 1
            chunk = piece[last:]
            if chunk.strip() and cur_off is None:
                cur_off = a + last + (len(chunk) - len(chunk.lstrip()))
            cur.append(chunk)

        for ch in children:
            add(pos, ch.open)
            if ch.kind in ('init', 'lambda'):
                cur.append('{}')
            elif ch.kind == 'class' and not ch.name:
                anon.append(ch)
                cur, cur_off = [], None
            else:
                cur, cur_off = [], None
            pos = ch.close + 1
        add(pos, end)
        flush()
        return out, anon

    def _statements(self, block, children, scope):
        start = block.open + 1 if block else 0
        end = block.close if block else len(self.s)
        kind = block.kind if block else 'root'
        segs, anon = self.segments(start, end, children)
        for text, off in segs:
            self.statement(text, off, scope, kind, block)
        if kind in ('class', 'spec'):
            for a in anon:
                asegs, _ = self.segments(a.open + 1, a.close, a.children)
                for text, off in asegs:
                    self.statement(text, off, scope, kind, block)
        for ch in children:
            self._statements(ch, ch.children, ch.scope)

    def statement(self, text, off, scope, kind, block):
        t = clean_head(text).strip()
        if not t:
            return
        t, tp = strip_template_prefix(t)
        t = t.strip()
        m = re.match(r'using\s+namespace\s+([\w:\s]+)$', t)
        if m:
            scope.usings.append((m.group(1).strip(), scope))
            return
        m = re.match(r'namespace\s+(\w+)\s*=\s*([\w:\s]+)$', t)
        if m:
            scope.names[m.group(1)] = NsAlias(m.group(2).strip(), scope)
            return
        m = re.match(r'using\s+(\w+)\s*(?:\[\[.*?\]\]\s*)?=\s*(.+)$', t, re.S)
        if m:
            scope.names[m.group(1)] = Alias(m.group(2), scope, template_params(tp))
            return
        m = re.match(r'using\s+((?:typename\s+)?[\w:\s]+::\s*(\w+))$', t)
        if m:
            if m.group(2) not in scope.names:
                scope.names[m.group(2)] = UsingDecl(m.group(1).replace('typename', ''), scope)
            return
        m = re.match(r'typedef\s+(.+?)\s*\b(\w+)\s*$', t, re.S)
        if m and '(' not in m.group(1):
            scope.names[m.group(2)] = Alias(m.group(1), scope)
            return
        if re.match(r'(?:friend|static_assert|return|using|typedef|enum|template|class|struct|union|concept|'
                    r'extern\s*"|goto|break|continue|throw|co_return|co_yield|delete|case|default)\b', t):
            if not re.match(r'(?:class|struct|union)\s+\w+(?:\s*::\s*\w+)*\s+(?!final\b)\w+', t):   # struct X x; is a declaration
                return
        if kind in ('func', 'lambda', 'control', 'init'):
            if not re.match(r'(?:static|thread_local)\b', t):
                return
        decl = self.declaration(t)
        if decl is None:
            if kind in ('root', 'ns', 'transparent', 'class', 'spec') and '(' in t:
                self.function(t, scope)
            return
        typ, name, is_static = decl
        if kind in ('class', 'spec'):
            if is_static:
                self.statics.append((typ, off, scope, 'the static member %s' % name, t))
            else:
                scope.members.append((typ, off))
        elif kind in ('root', 'ns', 'transparent'):
            if re.match(r'(?:constexpr\s+)?(?:inline\s+)?(?:static\s+)?(?:constexpr\s+)?[\w:<>,\s*&]*\boperator\b', t):
                return
            self.statics.append((typ, off, scope, 'the variable %s at namespace scope' % name, t))
        else:
            self.statics.append((typ, off, scope, 'the static local %s' % name, t))

    def function(self, t, scope):
        """A function's declaration or definition head: its return type kept under its name."""
        t = clean_head(t)
        t, tp = strip_template_prefix(t)
        t = re.sub(r'\b(?:inline|static|constexpr|consteval|friend|virtual|explicit|extern)\b', ' ', t)
        depth = 0
        cut = -1
        for i, c in enumerate(t):
            if c == '<':
                depth += 1
            elif c == '>' and depth > 0:
                depth -= 1
            elif c == '(' and depth == 0:
                cut = i
                break
        if cut < 0:
            return
        before = t[:cut].rstrip()
        mname = re.search(r'~?[A-Za-z_]\w*$', before)
        if not mname or mname.group(0) in KEYWORDS or mname.group(0).startswith('~'):
            return
        name = mname.group(0)
        ret = before[:mname.start()].strip()
        if not ret or ret.endswith('::'):
            return      # a constructor, or a member defined out of its class
        if ret in ('auto', 'decltype(auto)'):
            e = matching(t, cut, '(', ')')
            mm = re.match(r'[^-{]*->\s*(.+?)\s*(?:requires\b.*)?$', t[e:], re.S) if e > 0 else None
            if not mm:
                return
            ret = mm.group(1)
        fs = scope
        params = template_params(tp)
        if params:
            fs = Scope('alias', scope)
            for pname, pack in params:
                fs.names[pname] = Param(pname, pack, fs)
        scope.funcs.setdefault(name, []).append((ret, fs))

    def declaration(self, t):
        """(type text, name, static) of a variable's declaration, or None."""
        is_static = bool(re.match(r'(?:\w+\s+)*?(?:static|thread_local)\b', t)) and bool(re.search(r'\b(?:static|thread_local)\b', t.split('<')[0]))
        # the initializer off
        depth = 0
        cut = len(t)
        for i, c in enumerate(t):
            if c in '<([':
                depth += 1
            elif c in '>)]':
                depth -= 1
            elif depth == 0 and c == '=' and (i + 1 >= len(t) or t[i + 1] != '=') and (i == 0 or t[i - 1] not in '=!<>'):
                cut = i
                break
            elif depth == 0 and c == '{':
                cut = i
                break
        d = t[:cut]
        if _paren_outside_angles(d) and not re.search(r'\(\s*\*', d):
            return None
        if re.search(r'\(\s*\*', d):
            return None
        d = re.sub(r'\balignas\s*\([^()]*(?:\([^()]*\)[^()]*)*\)', ' ', d)
        d = split_top(d)[0]
        d = re.sub(r':\s*\d+\s*$', '', d)                  # a bit-field
        d = re.sub(r'(\[[^\]]*\]\s*)+$', '', d.rstrip())   # an array's bounds
        m = re.match(r'(.*?)(\w+)\s*$', d, re.S)
        if not m:
            return None
        typ, name = m.group(1).strip(), m.group(2)
        if not typ or name in KEYWORDS:
            return None
        words = [w for w in re.findall(r'[A-Za-z_]\w*', typ) if w not in SPECIFIERS]
        if not words:
            return None
        return typ, name, is_static

    # positions
    def block_at(self, off):
        k = bisect.bisect_right(self.opens, off) - 1
        while k >= 0:
            b = self.blocks[k]
            if b.open < off <= b.close:
                return b
            b = b.parent
            while b is not None:
                if b.open < off <= b.close:
                    return b
                b = b.parent
            return None
        return None

    def scope_at(self, off):
        b = self.block_at(off)
        return b.scope if b is not None else self.root

    def function_of(self, off):
        """The outermost function-like block around off: its head start and the block."""
        b = self.block_at(off)
        found = None
        while b is not None:
            if b.kind in ('ns', 'class', 'spec', 'transparent', 'enum'):
                break
            found = b
            b = b.parent
        return found


def shown(path):
    rel = os.path.relpath(path, ROOT)
    return path if rel.startswith('..') else rel


def header_file(path, universe):
    if path in universe.headers:
        return universe.headers[path]
    universe.headers[path] = None      # an include cycle stops here
    with open(path, encoding='utf-8', errors='replace') as fh:
        f = File(path, fh.read(), universe, local=True)
    settle(f.classes)
    universe.headers[path] = f
    return f


# --- The class statuses -------------------------------------------------

def settle(classes):
    """Each class's status from its members and bases, to a fixpoint: with the views counted, then
    without them."""
    for mode in (1, 0):
        MODE[0] = mode
        try:
            _settle(classes)
        finally:
            MODE[0] = 0


def _settle(classes):
    for _ in range(30):
        changed = False
        for c in classes:
            if c.always:
                continue
            q = c.qual()
            if q in EXPLICIT_ALLOWED or q in EXPLICIT_TRANSPARENT or MODE[0] and q in EXPLICIT_MAYBE:
                continue
            if q in EXPLICIT_HANDLE:
                c.always = True
                c.why = EXPLICIT_HANDLE[q]
                changed = True
                continue
            dep = set(c.dep)
            always = False
            why = ''
            for typ, off in c.members:
                r = evaluate(type_of_text(typ), c)
                if r is True:
                    always, why = True, 'member ' + typ.strip()
                    break
                if r:
                    dep |= {p for p in r if p in c.params}
            if not always:
                for b in c.bases:
                    r = evaluate(type_of_text(b), c.parent)
                    if r is True:
                        always, why = True, 'base ' + b
                        break
                    if r:
                        # a base that depends on a parameter of this class
                        dep |= {p for p in r if p in c.params}
            if not always:
                for sp in c.specs:
                    if sp.always:
                        always, why = True, 'specialization at %s:%d' % (sp.path, sp.line)
                        break
            if always:
                c.always, c.why = True, why
                changed = True
            elif dep != c.dep:
                c.dep = dep
                changed = True
        if not changed:
            break


# --- The checks ---------------------------------------------------------

_TEMPLATE_ID = re.compile(r'(?<![\w.>])((?:::\s*)?[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*)\s*<(?!<)')
_NAME = re.compile(r'(?<![\w.])((?:::\s*)?[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*)')


def written(node):
    return node.text if node is not None else '?'


def heap_finding(node, scope, env=None, deep=False, depth=0):
    """What is wrong with a template-id, or None."""
    if node is None or depth > 8:
        return None
    ent = deref(resolve(node, scope, depth), depth)
    args = node.comps[-1][1] or []
    if isinstance(ent, Std) and ent.kind in ('heap', 'layout'):
        for a in args:
            sev = certainty(a, scope, env) if a is not None else None
            if sev:
                note = VIEW_NOTE if sev == 'check' else ''
                if ent.kind == 'heap':
                    return 'container', 'std::%s of %s: the elements live in unmanaged memory (an sgcl container, or root_ptr)%s' % (ent.name, written(a), note), sev
                return 'layout', 'std::%s holding %s: no fixed word for the pointer (sgcl::%s)%s' % (ent.name, written(a), ent.name, note), sev
    if isinstance(ent, Alias) and ent.params and args:
        tn = ent.node()
        if tn:
            new_env = bind(ent.params, args, scope, env)
            r = heap_finding(tn, ent.scope, new_env, True, depth + 1)
            if r:
                return r[0], '%s: %s' % (written(node), r[1]), r[2]
    if deep:
        for a in args:
            if a is not None:
                r = heap_finding(a, scope, env, True, depth + 1)
                if r:
                    return r
    return None


class Checker:
    def __init__(self, universe):
        self.universe = universe
        self.helpers = {}      # function name -> {param index}: forwards that argument to a thread by value
        self.files = []

    def check(self, f):
        s = f.s
        decls = {}      # name -> [(offset, 'thread' | 'functions' | 'function')]
        # 1. template-ids
        for m in _TEMPLATE_ID.finditer(s):
            name = m.group(1)
            last = re.split(r'\s*::\s*', name)[-1]
            if last in KEYWORDS:
                continue
            lt = m.end() - 1
            e = angle_end(s, lt)
            if e < 0:
                continue
            node = type_of_text(s[m.start(1):e])
            if node is None:
                continue
            scope = f.scope_at(m.start())
            r = heap_finding(node, scope)
            if r:
                f.report(m.start(), r[2], r[0], r[1])
            # the variables that hold threads or std::functions
            ent = deref(resolve(node, scope))
            tail = re.match(r'\s*&?\s*(\w+)\s*(=|\(|\{|;)', s[e:e + 200])
            if isinstance(ent, Std):
                args = node.comps[-1][1] or []
                if ent.kind == 'function' and tail:
                    decls.setdefault(tail.group(1), []).append((e, 'function'))
                    self.std_function_init(f, tail, e)
                elif (ent.kind == 'heap' or ent.name == 'array') and args and args[0] is not None and tail:
                    ae = deref(resolve(args[0], scope))
                    if isinstance(ae, Std) and ae.kind == 'thread':
                        decls.setdefault(tail.group(1), []).append((e, 'thread'))
                    elif isinstance(ae, Std) and ae.kind == 'function' and ent.kind == 'heap':
                        decls.setdefault(tail.group(1), []).append((e, 'functions'))
        # 2. std::thread, std::jthread, std::async
        for m in re.finditer(r'(?<![\w.])((?:::\s*)?(?:\w+\s*::\s*)*(?:thread|jthread|async))\b', s):
            node = type_of_text(m.group(1))
            scope = f.scope_at(m.start())
            ent = deref(resolve(node, scope))
            if not (isinstance(ent, Std) and ent.kind == 'thread'):
                continue
            rest = s[m.end():m.end() + 80]
            mm = re.match(r'\s*(?:\w+\s*)?([({])', rest)
            if not mm or re.match(r'\s*::', rest):
                continue
            open_at = m.end() + mm.start(1)
            close = matching(s, open_at, s[open_at], ')' if s[open_at] == '(' else '}')
            if close < 0:
                continue
            self.launch(f, s[open_at + 1:close - 1], open_at + 1, 'std::' + ent.name, skip_first=(ent.name == 'async'))
        # 3. emplace_back / push_back on containers of threads or functions
        for var in decls:
            for m in re.finditer(r'(?<![\w.])' + re.escape(var) + r'\s*(?:\.|->)\s*(emplace_back|emplace|push_back|emplace_front|push_front|insert)\s*\(', s):
                kind = self.declared(f, decls[var], m.start())
                if kind not in ('thread', 'functions'):
                    continue
                open_at = m.end() - 1
                close = matching(s, open_at, '(', ')')
                if close < 0:
                    continue
                what = 'std::thread' if kind == 'thread' else 'std::function'
                self.launch(f, s[open_at + 1:close - 1], open_at + 1, what, callable_only=(kind == 'functions'))
        # 4. assignments of lambdas to std::function variables
        for var in decls:
            for m in re.finditer(r'(?<![\w.])' + re.escape(var) + r'\s*=\s*(?=\[)', s):
                if self.declared(f, decls[var], m.start()) == 'function':
                    self.lambda_at(f, m.end(), 'std::function')
        # 5. new
        for m in re.finditer(r'(?<![\w:])new\s+(?!\()', s):
            seg = s[m.end():m.end() + 300]
            node = type_of_text(re.split(r'[({;\[]', seg, maxsplit=1)[0])
            if node is None:
                continue
            scope = f.scope_at(m.start())
            sev = certainty(node, scope)
            if sev:
                f.report(m.start(), sev, 'new', 'new %s: a tracked word in unmanaged memory (make_tracked)%s' % (written(node), VIEW_NOTE if sev == 'check' else ''))
        # 5b. exception objects: the runtime allocates them
        for m in re.finditer(r'(?<![\w:])(throw\s+|make_exception_ptr\s*\(\s*)(?=[\w:])', s):
            seg = s[m.end():m.end() + 400]
            seg = re.split(r';', seg, maxsplit=1)[0]
            scope = f.scope_at(m.start())
            mm = re.match(r'((?:::)?[A-Za-z_][\w:]*(?:\s*<[^;]*?>)?)\s*[({]', seg, re.S)
            node = type_of_text(mm.group(1)) if mm else None
            ent = deref(resolve(node, scope)) if node is not None else None
            sev = certainty(node, scope) if isinstance(ent, (Class, Alias)) else None
            if sev:
                f.report(m.start(), sev, 'throw', 'an exception object of %s: the runtime allocates it, unmanaged memory (a std::string message, or a rooted member)' % written(node))
                continue
            mm = re.match(r'(?:std::move\s*\(\s*)?(\w+)\s*\)?\s*\)?\s*$', seg)
            if mm:
                r = self.local_type(f, mm.group(1), m.start())
                if r:
                    f.report(m.start(), r[1], 'throw', 'an exception object copied from %s (%s): the runtime allocates it, unmanaged memory' % (mm.group(1), written(r[0])))
        # 6. static storage
        for typ, off, scope, what, stmt in f.statics:
            node = type_of_text(typ)
            sev = certainty(node, scope) if node is not None else None
            note = VIEW_NOTE if sev == 'check' else ''
            if node is not None and node.name() == 'auto':
                mm = re.search(r'(?<![=!<>])=(?!=)\s*(.*)$', stmt, re.S)
                r = self.infer(f, mm.group(1), off, scope, 0, off) if mm else None
                if r:
                    node, sev, note = r[0], r[1], ''
            if sev:
                f.report(off, sev, 'static', '%s (%s): static storage is neither the stack nor a managed object (root_ptr)%s' % (what, written(node), note))
        # 7. fixtures
        for c in f.classes:
            if self.is_fixture(c):
                for typ, off in c.members:
                    node = type_of_text(typ)
                    sev = certainty(node, c) if node is not None else None
                    if sev:
                        f.report(off, sev, 'fixture', 'the fixture %s holds %s: the framework makes it with new' % (c.name, written(node)))
        # 8. the helpers this file defines, and their calls
        self.find_helpers(f)

    def declared(self, f, decls, pos):
        """Which of a name's declarations a use at pos means: the nearest one before it in the same
        function, else one outside every function (a member, a global)."""
        fn = f.function_of(pos)
        best = None
        outside = None
        for off, kind in decls:
            fd = f.function_of(off)
            if fd is None:
                outside = kind
            elif fd is fn and off < pos and (best is None or off > best[0]):
                best = (off, kind)
        return best[1] if best else outside

    def is_fixture(self, c, depth=0):
        if c.fixture:
            return True
        for b in c.bases:
            if re.search(r'(?:^|::)\s*(?:Test|TestWithParam|Fixture)\b', b) and re.search(r'testing|benchmark', b):
                c.fixture = True
                return True
            if depth < 4:
                node = type_of_text(b)
                be = deref(resolve(node, c.parent)) if node else None
                if isinstance(be, Class) and be is not c and self.is_fixture(be, depth + 1):
                    c.fixture = True
                    return True
        return False

    def std_function_init(self, f, tail, e):
        s = f.s
        start = e + tail.end()
        rest = s[start:start + 20]
        if tail.group(2) == '=':
            if re.match(r'\s*\[', rest):
                self.lambda_at(f, start + len(rest) - len(rest.lstrip()), 'std::function')
        elif tail.group(2) in '({':
            mm = re.match(r'\s*\[', rest)
            if mm:
                self.lambda_at(f, start + mm.end() - 1, 'std::function')

    def launch(self, f, args_text, args_off, what, skip_first=False, callable_only=False):
        args = split_top(args_text)
        offs = []
        pos = 0
        for a in args:
            offs.append(args_off + pos)
            pos += len(a) + 1
        first = True
        for k, (a, off) in enumerate(zip(args, offs)):
            st = a.strip()
            lead = len(a) - len(a.lstrip())
            if not st:
                continue
            if skip_first and k == 0 and re.match(r'(?:std::)?launch\b', st):
                continue
            if st.startswith('['):
                self.lambda_at(f, off + lead, what)
            elif first:
                # the callable named: a lambda kept in a variable
                mm = re.match(r'(?:std::move\s*\(\s*)?(\w+)\s*\)?$', st)
                if mm:
                    lam = self.lambda_of_variable(f, mm.group(1), off)
                    if lam is not None:
                        self.lambda_at(f, lam, what, report_at=off + lead)
                    else:
                        self.by_value(f, mm.group(1), off + lead, what, callable=True)
            elif not callable_only:
                self.argument(f, st, off + lead, what)
            first = False if not (skip_first and k == 0) else first

    def lambda_of_variable(self, f, name, off):
        fb = f.function_of(off)
        start = fb.head_start if fb is not None else 0
        region = f.s[start:off]
        found = None
        for m in re.finditer(r'\b(?:auto|const\s+auto)\s*&?\s*' + re.escape(name) + r'\s*=\s*(?=\[)', region):
            found = start + m.end()
        return found

    def argument(self, f, st, off, what):
        """An argument passed on to a thread's state by value."""
        if re.match(r'(?:std::)?c?ref\s*\(', st):
            return
        mm = re.match(r'(?:std::move\s*\(\s*)?(\w+)\s*\)?$', st)
        if mm:
            self.by_value(f, mm.group(1), off, what)
            return
        mm = re.match(r'((?:::)?[\w:]+(?:\s*<.*>)?)\s*[({]', st, re.S)
        if mm:
            node = type_of_text(mm.group(1))
            sev = certainty(node, f.scope_at(off)) if node is not None else None
            if sev:
                f.report(off, sev, 'thread', '%s copies %s into its state, unmanaged memory (pass a reference, or a root_ptr)' % (what, written(node)))

    def by_value(self, f, name, off, what, callable=False):
        verdict = self.local_type(f, name, off)
        if verdict is None:
            return
        node, sev, scope, decl_off = verdict
        if callable:
            return
        f.report(off, sev, 'thread', '%s copies %s (%s) into its state, unmanaged memory (pass a reference, or a root_ptr)' % (what, name, written(node)))

    def local_type(self, f, name, off, depth=0, exclude=None):
        """(type node, severity, scope, offset of the declaration) of a local variable or parameter that is a
        handle, by the nearest declaration before off in the enclosing function; None otherwise."""
        if depth > 4 or name in KEYWORDS:
            return None
        fb = f.function_of(off)
        if fb is None:
            return None
        start = fb.head_start
        s = f.s
        region = s[start:off]
        for m in reversed(list(re.finditer(r'(?<![\w.:>])' + re.escape(name) + r'\b', region))):
            at = start + m.start()
            bind_m = self.binding(s, start, at, name)
            if bind_m is not None:
                if bind_m == -1:
                    return None
                init = s[bind_m:bind_m + 300]
                init = re.split(r'[;]', init, maxsplit=1)[0].strip()
                r = self.infer(f, init, bind_m, f.scope_at(at), depth, at)
                return r
            if exclude and exclude[0] <= at < exclude[1]:
                continue
            after = s[start + m.end():start + m.end() + 3].lstrip()
            if not after or after[0] not in '=;{(:,)[':
                continue
            if after.startswith('::'):
                continue
            # the text before the name, back to a delimiter
            k = at - 1
            depth_a = 0
            while k >= start:
                c = s[k]
                if c == '>':
                    depth_a += 1
                elif c == '<':
                    depth_a -= 1
                elif depth_a <= 0 and c in ';{}(,':
                    break
                k -= 1
            before = s[k + 1:at].strip()
            if not before:
                continue
            words = re.sub(r'\b(?:const|static|constexpr|volatile|thread_local|inline|mutable)\b', ' ', before).strip()
            if not words or '*' in words:
                if '*' in words:
                    return None
                continue
            if re.search(r'[=+\-/%|^!?]|\breturn\b|\bcase\b|\bnew\b|\bdelete\b|\bthrow\b|\bco_\w+', words):
                continue
            ws = words.replace('&', ' ').strip()
            if not re.match(r'(?:::)?[A-Za-z_][\w:]*(?:\s*<.*>)?(?:\s*::\s*\w+)*$', ws, re.S):
                continue
            first_word = re.match(r'(?:::)?([A-Za-z_]\w*)', ws).group(1)
            if first_word in BUILTIN:
                return None         # declared as a number, a character, a bool
            if first_word in KEYWORDS and first_word not in ('auto',):
                continue
            scope = f.scope_at(at)
            if ws == 'auto' or ws == 'decltype ( auto )':
                if after[0] == ':':
                    return None        # a range-for's element: unknown
                if after[0] not in '={(':
                    return None
                init_start = start + m.end() + (s[start + m.end():].find(after[0])) + 1
                init = s[init_start:init_start + 300]
                init = re.split(r'[;]', init, maxsplit=1)[0].strip()
                return self.infer(f, init, init_start, scope, depth, at)
            node = type_of_text(ws)
            if node is None:
                return None
            sev = certainty(node, scope)
            if sev:
                if sev == 'check':
                    node.text = written(node) + ', a word only with a slice\'s owner or a managed closure'
                return node, sev, scope, at
            return None
        return None

    def binding(self, s, start, at, name):
        """When the name at `at` is one of a structured binding's names: where its initializer starts (or -1
        for a range-for's element); else None."""
        k = at - 1
        while k > start and s[k] not in '[;{}()':
            k -= 1
        if s[k] != '[':
            return None
        before = s[max(start, k - 40):k]
        if not re.search(r'\bauto\s*&{0,2}\s*$', before):
            return None
        e = matching(s, k, '[', ']')
        if e < 0:
            return None
        mm = re.match(r'\s*(=|:)', s[e:e + 20])
        if not mm:
            return None
        if mm.group(1) == ':':
            return -1
        return e + mm.end()

    def infer(self, f, init, off, scope, depth, decl_at):
        init = init.strip()
        mm = re.match(r'std\s*::\s*move\s*\(\s*(\w+)\s*\)', init)
        if mm:
            r = self.local_type(f, mm.group(1), decl_at, depth + 1)
            return (r[0], 'check', r[2], r[3]) if r else None
        mm = re.match(r'(?:sgcl::)?make_tracked\s*<', init)
        if mm:
            e = angle_end(init, mm.end() - 1)
            n = Node()
            n.text = 'tracked_ptr<%s>' % init[mm.end():e - 1].strip() if e > 0 else 'tracked_ptr'
            return n, 'check', scope, decl_at
        mm = re.match(r'((?:::)?[A-Za-z_][\w:]*(?:\s*<[^;]*?>)?)\s*[({]', init)
        if mm:
            node = type_of_text(mm.group(1))
            if node is not None:
                ent = deref(resolve(node, scope))
                if isinstance(ent, (Class, Alias, Std)) and certainty(node, scope):
                    node.text = mm.group(1)
                    return node, 'check', scope, decl_at
                if not isinstance(ent, (Class, Alias, Std)) and returns_handle(node, scope):
                    n = Node()
                    n.text = 'what %s returns' % pretty(tokenize(mm.group(1)))
                    return n, 'check', scope, decl_at
        mm = re.match(r'(\w+)$', init)
        if mm:
            r = self.local_type(f, mm.group(1), decl_at, depth + 1)
            return (r[0], 'check', r[2], r[3]) if r else None
        return None

    def lambda_at(self, f, at, what, report_at=None):
        """s[at] is the '[' of a lambda handed to what: its captures by value checked."""
        s = f.s
        if at >= len(s) or s[at] != '[':
            return
        ce = matching(s, at, '[', ']')
        if ce < 0:
            return
        caps = s[at + 1:ce - 1]
        # the body
        bo = s.find('{', ce)
        if bo < 0:
            return
        be = matching(s, bo, '{', '}')
        if be < 0:
            return
        body = s[ce:be]
        rep = report_at if report_at is not None else at
        default_copy = False
        names = []
        for c in split_top(caps):
            c = c.strip()
            if not c:
                continue
            if c == '=':
                default_copy = True
            elif c == '&' or c.startswith('&') or c in ('this', '*this'):
                continue
            else:
                mm = re.match(r'(\w+)\s*(?:\.\.\.)?\s*(=|\{|\()?\s*(.*)$', c, re.S)
                if not mm:
                    continue
                if mm.group(2):
                    init = mm.group(3).rstrip(')}').strip()
                    r = self.infer(f, init, at, f.scope_at(at), 0, at)
                    if r:
                        f.report(rep, r[1], 'thread', '%s gets a lambda that captures %s (%s) by value: a copy in unmanaged memory (capture by reference, or a root_ptr)' % (what, mm.group(1), written(r[0])))
                    continue
                names.append(mm.group(1))
        for n in names:
            r = self.local_type(f, n, at)
            if r:
                f.report(rep, r[1], 'thread', '%s gets a lambda that captures %s (%s) by value: a copy in unmanaged memory (capture by reference, or a root_ptr)' % (what, n, written(r[0])))
                return
        if default_copy:
            used = set(re.findall(r'(?<![\w.:>])([A-Za-z_]\w*)\b(?!\s*::)', body)) - KEYWORDS
            for n in sorted(used):
                # declared inside the lambda: its own
                if re.search(r'[\w>&*]\s+&?' + re.escape(n) + r'\s*[=;{(:,)\[]', body):
                    continue
                r = self.local_type(f, n, at, exclude=(ce, be))
                if r:
                    f.report(rep, r[1], 'thread', '%s gets a lambda that copies %s (%s) by [=]: a copy in unmanaged memory (capture by reference, or a root_ptr)' % (what, n, written(r[0])))
                    return

    def find_helpers(self, f):
        """Functions of the tree that hand a parameter of theirs to a thread by value."""
        s = f.s
        for b in f.blocks:
            if b.kind != 'func':
                continue
            head = clean_head(b.head)
            head, tp = strip_template_prefix(head)
            mm = re.search(r'(\w+)\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)[^()]*$', head)
            if not mm:
                continue
            fname = mm.group(1)
            params = split_top(mm.group(2))
            pnames = []
            for p in params:
                d = split_top(p, '=')[0].strip()
                w = re.findall(r'\w+', d)
                pnames.append(w[-1] if w else '')
            body = s[b.open:b.close]
            if not re.search(r'\bstd\s*::\s*(?:j?thread|async)\b', body):
                continue
            launch = (r'(?:std\s*::\s*j?thread\s*\w*\s*[({]|std\s*::\s*async\s*\(\s*(?:std::launch::\w+\s*,\s*)?'
                      r'|emplace_back\s*\()\s*')
            for k, pn in enumerate(pnames):
                if not pn or pn in KEYWORDS:
                    continue
                q = re.escape(pn)
                # the parameter itself handed to the thread (a copy of the closure), or captured by value
                # in the lambda handed to it
                if re.search(launch + r'(?:std::move\s*\(\s*|std::forward\s*<\s*\w+\s*>\s*\(\s*)?' + q + r'\b', body) or \
                   re.search(launch + r'\[[^\]]*(?<![&\w])' + q + r'\b[^\]]*\]', body):
                    self.helpers.setdefault(fname, set()).add(k)

    def helper_calls(self, f):
        if not self.helpers:
            return
        s = f.s
        pat = re.compile(r'(?<![\w])(?:\w+\s*::\s*)*(' + '|'.join(re.escape(h) for h in self.helpers) + r')\s*\(')
        for m in pat.finditer(s):
            b = f.block_at(m.start())
            if b is not None and b.kind == 'func' and b.open > m.start():
                continue
            open_at = m.end() - 1
            close = matching(s, open_at, '(', ')')
            if close < 0:
                continue
            # a definition, not a call
            after = s[close:close + 40]
            if re.match(r'\s*(?:const\s*)?(?:noexcept\s*)?\{', after):
                continue
            args = split_top(s[open_at + 1:close - 1])
            pos = open_at + 1
            for k, a in enumerate(args):
                if k in self.helpers[m.group(1)] and a.strip().startswith('['):
                    lead = len(a) - len(a.lstrip())
                    before = len(f.findings)
                    self.lambda_at(f, pos + lead, '%s (to a thread)' % m.group(1))
                    for key in list(f.findings)[before:]:
                        sev, rule, what = f.findings[key]
                        f.findings[key] = ('check', rule, what)
                pos += len(a) + 1


# --- Driving -------------------------------------------------------------

def sources(paths):
    out = []
    for p in paths:
        p = os.path.join(ROOT, p) if not os.path.isabs(p) else p
        if os.path.isfile(p):
            out.append(p)
            continue
        for d, dirs, files in os.walk(p):
            dirs[:] = sorted(x for x in dirs if not x.startswith('.') and not x.startswith('build') and x not in ('external', 'seeds', 'corpus'))
            for name in sorted(files):
                if name.endswith(('.h', '.hpp', '.cpp', '.cc', '.md')):
                    out.append(os.path.join(d, name))
    return out


def doc_blocks(path):
    """The ```cpp blocks of a page: (text, line of its first line - 1)."""
    out = []
    with open(path, encoding='utf-8') as fh:
        lines = fh.read().split('\n')
    cur, start = None, 0
    for i, line in enumerate(lines):
        if line.startswith('```'):
            if cur is None:
                if line[3:].strip() in ('cpp', 'c++'):
                    cur, start = [], i + 1
            else:
                out.append(('\n'.join(cur), start))
                cur = None
        elif cur is not None:
            cur.append(line)
    return out


def load_library(universe):
    files = []
    for path in sources(['sgcl']):
        if not path.endswith('.h'):
            continue
        with open(path, encoding='utf-8', errors='replace') as fh:
            files.append(File(path, fh.read(), universe, local=False))
    settle(universe.classes)
    return files


def list_types(universe):
    rows = []
    for c in universe.classes:
        q = c.qual()
        if '::detail' in q or not q.startswith('sgcl') or c.name.startswith('_') or c.name[:1].isupper():
            continue
        if q in EXPLICIT_ALLOWED:
            continue
        if c.always:
            rows.append((q, 'always', c.why or EXPLICIT_HANDLE.get(q, '')))
        if q in EXPLICIT_MAYBE and c.always:
            rows[-1] = (q, 'by what it holds', EXPLICIT_MAYBE[q])
        elif q in EXPLICIT_TRANSPARENT:
            rows.append((q, 'when an argument is one', EXPLICIT_TRANSPARENT[q]))
        elif c.dep:
            rows.append((q, 'when %s is one' % ', '.join(p.name for p in c.params if p in c.dep), ''))
    seen = set()
    for q, when, why in sorted(rows):
        if q in seen:
            continue
        seen.add(q)
        print('%-48s %-28s %s' % (q, when, why))
    print('\nallowed holders: ' + ', '.join(sorted(EXPLICIT_ALLOWED)))


def main(argv):
    global ROOT
    if '-h' in argv or '--help' in argv:
        print(__doc__.strip())
        return 0
    argv = list(argv)
    if '--root' in argv:
        k = argv.index('--root')
        if k + 1 >= len(argv):
            print('--root needs a directory', file=sys.stderr)
            return 2
        ROOT = os.path.abspath(argv[k + 1])
        del argv[k:k + 2]
    universe = Universe()
    lib = load_library(universe)
    if '--list-types' in argv:
        list_types(universe)
        return 0
    docs = '--no-docs' not in argv
    targets = [a for a in argv if not a.startswith('--')]
    if not targets:
        targets = ['sgcl', 'tests', 'benchmarks'] + (['docs', 'README.md'] if docs else [])
    checker = Checker(universe)
    lib_by_path = {f.path: f for f in lib}
    files = []
    for path in sources(targets):
        if path.endswith('.md'):
            if not docs:
                continue
            rel = shown(path)
            for text, start in doc_blocks(path):
                f = File(path, text, universe, local=True, line_offset=start, display=rel)
                settle(f.classes)
                files.append(f)
            continue
        if path in lib_by_path:
            f = lib_by_path[path]
        elif path.endswith(('.h', '.hpp')):
            f = header_file(path, universe)
        else:
            with open(path, encoding='utf-8', errors='replace') as fh:
                f = File(path, fh.read(), universe, local=True)
            settle(f.classes)
        files.append(f)
    for f in files:
        checker.check(f)
    for f in files:
        checker.helper_calls(f)
    count = 0
    for f in sorted(files, key=lambda x: x.display):
        for ln in sorted(f.findings):
            sev, rule, what = f.findings[ln]
            print('%s:%d: %s: %s: %s' % (f.display, ln, sev, rule, what))
            count += 1
    print('%d finding%s' % (count, '' if count == 1 else 's'), file=sys.stderr)
    return 1 if count else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
