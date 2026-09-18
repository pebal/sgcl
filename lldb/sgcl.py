# SGCL: a C++20 application framework
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
#
# LLDB formatters for the pointers and containers of sgcl.
# Load with `command script import <sgcl>/lldb/sgcl.py` (in ~/.lldbinit
# for every session). A tracked_ptr shows its address and the state of
# the object's slot when the collector's types are in the debug info; a
# root_ptr the same, with the cell of a block it holds its object by; its
# one child is the object. A container shows its size and its elements, read
# from the managed buffer or walked node by node, as the std containers
# do with their own formatters.

import lldb
import struct

# --- reading the process ---------------------------------------------------

def read_ptr(valobj, addr):
    err = lldb.SBError()
    v = valobj.GetProcess().ReadPointerFromMemory(addr, err)
    return v if err.Success() else None


def read_u8(valobj, addr):
    err = lldb.SBError()
    v = valobj.GetProcess().ReadUnsignedFromMemory(addr, 1, err)
    return v if err.Success() else None


def read_u32(valobj, addr):
    err = lldb.SBError()
    v = valobj.GetProcess().ReadUnsignedFromMemory(addr, 4, err)
    return v if err.Success() else None


def word_of(valobj):
    """The word of a one-word pointer object, wherever its members are."""
    valobj = raw(valobj)
    addr = valobj.GetLoadAddress()
    if addr == lldb.LLDB_INVALID_ADDRESS:
        # a value not in memory (a register, an expression result): its bytes
        data = valobj.GetData()
        err = lldb.SBError()
        v = data.GetUnsignedInt64(err, 0)
        return v if err.Success() else None
    return read_ptr(valobj, addr)


def raw(valobj):
    """The value with its real members, under a synthetic view."""
    return valobj.GetNonSyntheticValue()


def member(valobj, name):
    return raw(valobj).GetChildMemberWithName(name)


def member_u64(valobj, name):
    m = member(valobj, name)
    return m.GetValueAsUnsigned(0) if m.IsValid() else 0


def template_type(valobj, index):
    t = valobj.GetType()
    if t.IsReferenceType():
        t = t.GetDereferencedType()
    t = t.GetUnqualifiedType()
    return t.GetTemplateArgumentType(index)


def align_of(t):
    try:
        a = t.GetByteAlign()
        if a:
            return a
    except Exception:
        pass
    return min(8, max(1, t.GetByteSize()))


def align_up(n, a):
    return (n + a - 1) // a * a


# --- the cell of a root_ptr ---------------------------------------------------

def resolve_root(valobj, cell):
    """(object address, mode text) of a root_ptr from the address of its cell."""
    if not cell:
        return None, "?"
    target = read_ptr(valobj, cell)
    block = cell & ~127          # a block is a cache line, 128 or 64 bytes; the larger mask names the line
    return target, "cell %d of block 0x%x" % ((cell - block) // 8, block)


# --- the state of an object's slot (the collector's types in the debug info) -

STATE_NAMES = {0: "Used", 1: "Reachable", 2: "UniqueLock", 4: "Destroyed", 6: "UniqueReleased", 8: "BadAlloc", 16: "Reserved", 32: "Unused"}


def state_of(valobj, object_addr):
    """'Reachable|P|Fresh' and so on for the slot of a managed object, or None."""
    try:
        target = valobj.GetTarget()
        globals_ = target.FindFirstGlobalVariable("sgcl::detail::Heap::globals")
        if not globals_.IsValid():
            return None
        base = globals_.GetChildMemberWithName("base")
        table = globals_.GetChildMemberWithName("biased_table")
        if not base.IsValid() or not table.IsValid():
            return None
        base = base.GetValueAsUnsigned(0)
        size = globals_.GetChildMemberWithName("size").GetValueAsUnsigned(0)
        if object_addr - base >= size:
            return None
        page = read_ptr(valobj, table.GetValueAsUnsigned(0) + (object_addr >> 16) * 8)
        if not page:
            return None
        page_type = target.FindFirstType("sgcl::detail::Page")
        if not page_type.IsValid():
            return None
        p = target.CreateValueFromAddress("page", lldb.SBAddress(page, target), page_type)
        data = member_u64(p, "data")
        multiplier = member_u64(p, "multiplier")
        index = ((object_addr - data) * multiplier) >> 32 if multiplier else 0
        state = read_u8(valobj, page + page_type.GetByteSize() + index)
        if state is None:
            return None
        parity = " P" if state & 64 else ""
        fresh = " Fresh" if state & 128 else ""
        return STATE_NAMES.get(state & ~(64 | 128), "%d" % state) + parity + fresh
    except Exception:
        return None


# --- pointers ---------------------------------------------------------------

def is_root_pointer(valobj):
    t = valobj.GetType()
    if t.IsReferenceType():
        t = t.GetDereferencedType()
    return t.GetUnqualifiedType().GetCanonicalType().GetName().startswith("sgcl::root_ptr<")


def object_of(valobj):
    """(object address, mode text) of a tracked_ptr (its word) or a root_ptr (its cell's word)."""
    word = word_of(valobj)
    if word is None:
        return None, "?"
    if is_root_pointer(valobj):
        return resolve_root(valobj, word)
    return word, None


def pointer_summary(valobj, internal_dict):
    obj, mode = object_of(valobj)
    if mode == "?":
        return "?"
    if not obj:
        return "null" + (" (%s)" % mode if mode else "")
    parts = ["0x%x" % obj]
    if mode:
        parts.append(mode)
    state = state_of(valobj, obj)
    if state:
        parts.append(state)
    return parts[0] + " (" + ", ".join(parts[1:]) + ")" if len(parts) > 1 else parts[0]


class PointerChildren:
    """One child: the object, as its type."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.object = None

    def update(self):
        self.object = None
        obj, mode = object_of(self.valobj)
        pointee = template_type(self.valobj, 0)
        if obj and pointee.IsValid() and pointee.GetByteSize() > 0 and pointee.GetName() != "void":
            self.object = self.valobj.CreateValueFromAddress("object", obj, pointee)

    def has_children(self):
        return self.object is not None

    def num_children(self):
        return 1 if self.object is not None else 0

    def get_child_index(self, name):
        return 0 if name == "object" else -1

    def get_child_at_index(self, index):
        return self.object if index == 0 else None


def unique_ptr_summary(valobj, internal_dict):
    word = word_of(valobj)   # the first word of a std::unique_ptr is its pointer, in libc++ and libstdc++
    if not word:
        return "null"
    state = state_of(valobj, word)
    return "0x%x" % word + (" (%s)" % state if state else "")


class UniqueChildren(PointerChildren):
    """sgcl::unique_ptr: the object, from the first word"""

    def update(self):
        self.object = None
        obj = word_of(self.valobj)
        pointee = template_type(self.valobj, 0)
        if obj and pointee.IsValid() and pointee.GetByteSize() > 0 and pointee.GetName() != "void":
            self.object = self.valobj.CreateValueFromAddress("object", obj, pointee)


class WeakChildren(PointerChildren):
    """sgcl::weak_ptr: the target while the cell holds it"""

    def update(self):
        self.object = None
        cell = (word_of(member(self.valobj, "_cell")))
        target = read_ptr(self.valobj, cell) if cell else None
        pointee = template_type(self.valobj, 0)
        if target and pointee.IsValid() and pointee.GetByteSize() > 0 and pointee.GetName() != "void":
            self.object = self.valobj.CreateValueFromAddress("object", target, pointee)


def weak_ptr_summary(valobj, internal_dict):
    cell = (word_of(member(valobj, "_cell")))
    if not cell:
        return "expired (no cell)"
    target = read_ptr(valobj, cell)          # WeakCell: target, then flags
    flags = read_u32(valobj, cell + 8)
    if not target:
        return "expired"
    text = "0x%x" % target
    if flags:
        names = [n for b, n in ((1, "watched"), (2, "expired"), (4, "drained")) if flags & b]
        text += " (" + ", ".join(names) + ")"
    return text


# --- containers -------------------------------------------------------------

def element_type_of(valobj):
    return template_type(valobj, 0)


class VectorChildren:
    """sgcl::vector<T> and the dynamic sgcl::array<T>: the elements of the managed buffer."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.data = 0
        self.size = 0
        self.elem = None

    def update(self):
        self.elem = element_type_of(self.valobj)
        self.size = member_u64(self.valobj, "_size")
        self.data = (word_of(member(self.valobj, "_ptr"))) or 0
        if not self.data:
            self.size = 0

    def has_children(self):
        return True

    def num_children(self):
        return self.size

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except ValueError:
            return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= self.size:
            return None
        return self.valobj.CreateValueFromAddress("[%d]" % index, self.data + index * self.elem.GetByteSize(), self.elem)


def vector_summary(valobj, internal_dict):
    size = member_u64(valobj, "_size")
    cap = member(valobj, "_capacity")
    return "size=%d capacity=%d" % (size, cap.GetValueAsUnsigned(0)) if cap.IsValid() else "size=%d" % size


class DequeChildren:
    """sgcl::deque<T>: the elements through the map of blocks."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj

    def update(self):
        self.elem = element_type_of(self.valobj)
        esize = max(1, self.elem.GetByteSize())
        block = max(1, 4096 // esize)
        self.block_size = 1 << (block.bit_length() - 1)
        self.size = member_u64(self.valobj, "_size")
        self.start = member_u64(self.valobj, "_start")
        self.map = (word_of(member(self.valobj, "_map"))) or 0
        if not self.map:
            self.size = 0

    def has_children(self):
        return True

    def num_children(self):
        return self.size

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except ValueError:
            return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= self.size:
            return None
        i = self.start + index
        block = read_ptr(self.valobj, self.map + (i // self.block_size) * 8)
        if not block:
            return None
        return self.valobj.CreateValueFromAddress("[%d]" % index, block + (i % self.block_size) * self.elem.GetByteSize(), self.elem)


def deque_summary(valobj, internal_dict):
    return "size=%d" % member_u64(valobj, "_size")


class ListChildren:
    """sgcl::list and forward_list: the nodes from the sentinel; the element after the links."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.nodes = []

    def update(self):
        self.nodes = []
        self.elem = element_type_of(self.valobj)
        name = self.valobj.GetType().GetUnqualifiedType().GetName()
        forward = "forward_list<" in name
        links = 1 if forward else 2
        self.offset = slot_offset(self.valobj, links * 8, self.elem)
        head = (word_of(member(self.valobj, "_head" if forward else "_sentinel")))
        if not head:
            return
        limit = 1 << 20
        node = read_ptr(self.valobj, head + (0 if forward else 8))   # next: forward_list's only link, list's second
        while node and node != head and len(self.nodes) < limit:
            self.nodes.append(node)
            node = read_ptr(self.valobj, node + (0 if forward else 8))

    def has_children(self):
        return True

    def num_children(self):
        return len(self.nodes)

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except ValueError:
            return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self.nodes):
            return None
        return self.valobj.CreateValueFromAddress("[%d]" % index, self.nodes[index] + self.offset, self.elem)


def list_summary(valobj, internal_dict):
    size = member(valobj, "_size")
    if size.IsValid():
        return "size=%d" % size.GetValueAsUnsigned(0)
    return "size=%d" % valobj.GetNumChildren()


def nested_type(t, name):
    """A nested typedef of a class or of one of its bases, resolved."""
    seen = [t]
    while seen:
        c = seen.pop(0)
        try:
            n = c.FindDirectNestedType(name)
            if n.IsValid():
                while n.IsTypedefType():
                    n = n.GetTypedefedType()
                return n
        except Exception:
            pass
        for i in range(c.GetNumberOfDirectBaseClasses()):
            seen.append(c.GetDirectBaseClassAtIndex(i).GetType())
    return None


def value_type_of(valobj):
    """The value_type of a map or set: the pair, or the key."""
    t = valobj.GetType()
    if t.IsReferenceType():
        t = t.GetDereferencedType()
    t = t.GetUnqualifiedType()
    vt = nested_type(t, "value_type")
    if vt is not None:
        return vt
    name = t.GetName()
    if "::map<" in name or "::multimap<" in name or "::unordered_map<" in name or "::unordered_multimap<" in name:
        k = t.GetTemplateArgumentType(0)
        v = t.GetTemplateArgumentType(1)
        pair = valobj.GetTarget().FindFirstType("std::pair<const %s, %s>" % (k.GetName(), v.GetName()))
        return pair if pair.IsValid() else None
    return t.GetTemplateArgumentType(0)


def slot_offset(valobj, base_size, elem):
    """Where a node's element sits: the `slot` field of the container's
    node type (StoredNode or Node, nested in the container or its base),
    or the base's size rounded up to the element's alignment (the element
    may sit in the base's tail padding: RbNodeBase is 25 bytes)."""
    t = valobj.GetType()
    if t.IsReferenceType():
        t = t.GetDereferencedType()
    t = t.GetUnqualifiedType()
    for name in ("StoredNode", "Node"):
        nt = nested_type(t, name)
        if nt is None:
            continue
        for i in range(nt.GetNumberOfFields()):
            f = nt.GetFieldAtIndex(i)
            if f.GetName() == "slot":
                return f.GetOffsetInBytes()
    return align_up(base_size, align_of(elem))


class TreeChildren:
    """sgcl::map, multimap, set, multiset: in order from the leftmost node."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.nodes = []

    def update(self):
        self.nodes = []
        self.elem = value_type_of(self.valobj)
        if self.elem is None:
            return
        self.offset = slot_offset(self.valobj, 25, self.elem)   # parent, left, right, red
        size = member_u64(self.valobj, "_size")
        header = (word_of(member(self.valobj, "_header")))
        if not header or not size:
            return
        node = read_ptr(self.valobj, header + 8)             # leftmost
        while node and len(self.nodes) < size:
            self.nodes.append(node)
            node = self.successor(node)

    def successor(self, x):
        right = read_ptr(self.valobj, x + 16)
        if right:
            y = right
            while True:
                left = read_ptr(self.valobj, y + 8)
                if not left:
                    return y
                y = left
        y = read_ptr(self.valobj, x)
        while y and x == read_ptr(self.valobj, y + 16):
            x = y
            y = read_ptr(self.valobj, y)
        return y

    def has_children(self):
        return True

    def num_children(self):
        return len(self.nodes)

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except ValueError:
            return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self.nodes):
            return None
        return self.valobj.CreateValueFromAddress("[%d]" % index, self.nodes[index] + self.offset, self.elem)


class HashChildren:
    """sgcl::unordered_map, multimap, set, multiset: the chain from the sentinel."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.nodes = []

    def update(self):
        self.nodes = []
        self.elem = value_type_of(self.valobj)
        if self.elem is None:
            return
        self.offset = slot_offset(self.valobj, 16, self.elem)   # next, hash
        size = member_u64(self.valobj, "_size")
        before = (word_of(member(self.valobj, "_before_begin")))
        if not before or not size:
            return
        node = read_ptr(self.valobj, before)
        while node and len(self.nodes) < size:
            self.nodes.append(node)
            node = read_ptr(self.valobj, node)

    def has_children(self):
        return True

    def num_children(self):
        return len(self.nodes)

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except ValueError:
            return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self.nodes):
            return None
        return self.valobj.CreateValueFromAddress("[%d]" % index, self.nodes[index] + self.offset, self.elem)


def size_summary(valobj, internal_dict):
    return "size=%d" % member_u64(valobj, "_size")


class ArrayChildren(VectorChildren):
    """sgcl::array<T> (dynamic): _ptr and _size, as a vector; the inline one keeps LLDB's own view."""
    pass


def array_summary(valobj, internal_dict):
    return "size=%d" % member_u64(valobj, "_size")


# --- registration -----------------------------------------------------------

def __lldb_init_module(debugger, internal_dict):
    m = __name__
    cat = "sgcl"
    debugger.HandleCommand("type category delete %s" % cat)
    def add(regex, summary=None, synthetic=None):
        if synthetic:
            debugger.HandleCommand("type synthetic add -w %s -x '%s' -l %s.%s" % (cat, regex, m, synthetic))
        if summary:
            debugger.HandleCommand("type summary add -w %s -x '%s' -F %s.%s -e" % (cat, regex, m, summary))
    add(r"^sgcl::(tracked_ptr|root_ptr)<.+>$", "pointer_summary", "PointerChildren")
    add(r"^sgcl::atomic<.+>$", None, None)
    add(r"^sgcl::unique_ptr<.+>$", "unique_ptr_summary", "UniqueChildren")
    add(r"^sgcl::weak_ptr<.+>$", "weak_ptr_summary", "WeakChildren")
    add(r"^sgcl::vector<.+>$", "vector_summary", "VectorChildren")
    add(r"^sgcl::array<.+, (18446744073709551615|-1)(ul|UL|ull|ULL)?>$", "array_summary", "ArrayChildren")
    add(r"^sgcl::deque<.+>$", "deque_summary", "DequeChildren")
    add(r"^sgcl::(list|forward_list)<.+>$", "list_summary", "ListChildren")
    add(r"^sgcl::(map|multimap|set|multiset)<.+>$", "size_summary", "TreeChildren")
    add(r"^sgcl::unordered_(map|multimap|set|multiset)<.+>$", "size_summary", "HashChildren")
    debugger.HandleCommand("type category enable %s" % cat)
