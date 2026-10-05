#!/usr/bin/env python3
# The Public Suffix List (https://publicsuffix.org) compiled into the table
# that sgcl/net/http/public_suffix.h looks a host up in. Run from the root
# of the repository:
#
#   python3 tools/gen_public_suffix.py [list] [--date YYYY-MM-DD] [--out header]
#
# list defaults to tools/public_suffix_list.dat (the copy in the tree) and
# out to sgcl/net/http/detail/public_suffix_table.h. A newer list is given
# on the command line, with the date it was taken (the list carries none).
# The list is MPL-2.0: its notice goes into the header, which is a form of
# it; the tree's NOTICE names it.
#
# The table. Every rule of both sections (ICANN and private, as browsers
# use both) becomes a key, the domain it names with "*." or "!" taken off,
# each label in its A-label form (IDNA: a label past ASCII as "xn--" and
# its punycode, RFC 3492), lower case; every parent domain of a key is a
# key too, with no rule of its own, so that a lookup that finds no key can
# stop. A key carries six bits: a rule names it, "*." under it is a rule,
# "!" names it as an exception, and each of the three again when the rule
# is in the private section. The keys are slots of an open-addressed hash
# table (linear probing, a power of two in size, at most 70% full), hashed
# by FNV-1a 32 over the bytes from the last to the first, so that a host
# is hashed once from its end and each of its suffixes looked up on the
# way. A slot is one 32-bit word: the chunk of the names (2 bits), the
# offset in it (16 bits), the length (8 bits) and the six bits; 0 is an
# empty slot. The names are pieces of string literals of at most 65000
# bytes (MSVC's limit is 65535 for a literal), a key that is the end of a
# longer one at a label boundary stored inside it.
import argparse
import os
import sys
import unicodedata

parser = argparse.ArgumentParser(description='Compile the Public Suffix List into a C++ table')
parser.add_argument('list', nargs='?', default='tools/public_suffix_list.dat')
parser.add_argument('--date', default='2024-01-07', help='the date of the list (it carries none)')
parser.add_argument('--source', default='the copy in the Ruby gem domain_name 0.6.20240107',
                    help='where the list was taken from, written into the header')
parser.add_argument('--out', default='sgcl/net/http/detail/public_suffix_table.h')
args = parser.parse_args()

RULE, WILD, EXC = 1, 2, 4
PRIVATE_SHIFT = 3   # RULE << 3 is a private rule, and so on
CHUNK = 65000


def a_label(label):
    label = unicodedata.normalize('NFC', label.lower())
    if all(ord(c) < 0x80 for c in label):
        return label
    return 'xn--' + label.encode('punycode').decode('ascii')


def to_ascii(domain):
    return '.'.join(a_label(l) for l in domain.split('.'))


flags = {}
section = None
rules = 0
with open(args.list, encoding='utf-8') as f:
    for line in f:
        line = line.strip()
        if line.startswith('// ===BEGIN ICANN DOMAINS==='):
            section = 'icann'
        elif line.startswith('// ===BEGIN PRIVATE DOMAINS==='):
            section = 'private'
        elif line.startswith('// ===END'):
            section = None
        if not line or line.startswith('//'):
            continue
        assert section, 'a rule outside the two sections: %r' % line
        rule = line.split()[0]
        if rule.startswith('!'):
            kind, name = EXC, rule[1:]
        elif rule.startswith('*.'):
            kind, name = WILD, rule[2:]
        else:
            kind, name = RULE, rule
        assert '*' not in name and '!' not in name, 'a wildcard elsewhere than the first label: %r' % rule
        key = to_ascii(name)
        assert key and all(key.split('.')), 'an empty label: %r' % rule
        bit = kind << PRIVATE_SHIFT if section == 'private' else kind
        flags[key] = flags.get(key, 0) | bit
        rules += 1
        labels = key.split('.')
        for i in range(1, len(labels)):
            flags.setdefault('.'.join(labels[i:]), 0)

assert rules > 1000, 'too few rules: is %s the list?' % args.list


def fnv(key):
    h = 2166136261
    for b in reversed(key.encode('ascii')):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


# the names: longest first (most labels), each key that ends a stored one
# at a label boundary pointed into it
names = []       # chunks
place = {}       # key -> (chunk, offset)
for key in sorted(flags, key=lambda k: (-k.count('.'), -len(k), k)):
    if key in place:
        continue
    data = key.encode('ascii')
    assert len(data) < 256, 'a key of 256 bytes or more: %s' % key
    if not names or len(names[-1]) + len(data) > CHUNK:
        names.append(bytearray())
    chunk, offset = len(names) - 1, len(names[-1])
    names[-1] += data
    labels = key.split('.')
    for i in range(len(labels)):
        suffix = '.'.join(labels[i:])
        if suffix not in place:
            place[suffix] = (chunk, offset + len(key) - len(suffix))
assert len(names) <= 4, 'the names need more than four chunks'
assert all(len(c) < 65536 for c in names)

size = 1
while size * 7 < len(flags) * 10:
    size *= 2
slots = [0] * size
longest_probe = 0
for key in sorted(flags):
    chunk, offset = place[key]
    word = chunk << 30 | offset << 14 | len(key) << 6 | flags[key]
    assert word != 0
    i = fnv(key) & (size - 1)
    probe = 1
    while slots[i]:
        i = (i + 1) & (size - 1)
        probe += 1
    longest_probe = max(longest_probe, probe)
    slots[i] = word

with open(args.list, encoding='utf-8') as f:
    notice = []
    for line in f:
        if not line.startswith('//'):
            break
        notice.append(line.rstrip('\n'))
    while notice and not notice[-1].strip('/ '):
        notice.pop()


def literal(chunk):
    out = []
    for i in range(0, len(chunk), 96):
        out.append('            "%s"' % chunk[i:i + 96].decode('ascii'))
    return '\n'.join(out)


lines = []
lines.append('''//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// The Public Suffix List, compiled: a form of the list, under its own license
// SPDX-License-Identifier: MPL-2.0
//------------------------------------------------------------------------------
#pragma once

// Generated by tools/gen_public_suffix.py from the Public Suffix List of
// %s (%s): do not edit.
// %d rules, both sections, %d keys in %d slots, the longest probe %d.
// The notice of the list:
//
%s
//
// The rest of the library is Apache-2.0; this file is the list's and
// stays MPL-2.0 (NOTICE at the root of the tree).

#include <cstdint>

namespace sgcl::net::http::detail {
    // What a key's six bits say: a rule names it, "*." under it is a rule,
    // it is an exception ("!"); the same three of the private section
    inline constexpr uint32_t PslRule = 1;
    inline constexpr uint32_t PslWild = 2;
    inline constexpr uint32_t PslException = 4;
    inline constexpr uint32_t PslPrivateShift = 3;
    inline constexpr const char* PslDate = "%s";
    inline constexpr uint32_t PslSlotCount = %d;

    // The names, in pieces of string literals; a slot's chunk picks one
    inline constexpr const char* PslNames[] = {''' % (
    args.date, args.source, rules, len(flags), size, longest_probe,
    '\n'.join('//   ' + l[3:] if l.startswith('// ') else '//' for l in notice),
    args.date, size))
for i, c in enumerate(names):
    lines.append(literal(c) + ',')
lines.append('''    };

    // chunk << 30 | offset << 14 | length << 6 | bits, 0 for an empty slot,
    // at FNV-1a 32 of the key's bytes from the last to the first
    inline constexpr uint32_t PslSlots[PslSlotCount] = {''')
for i in range(0, size, 8):
    lines.append('        ' + ' '.join('0x%08x,' % w for w in slots[i:i + 8]))
lines.append('''    };
}
''')
os.makedirs(os.path.dirname(args.out), exist_ok=True)
with open(args.out, 'w', encoding='ascii') as f:
    f.write('\n'.join(lines))
print('%s: %d rules, %d keys, %d slots, longest probe %d, %d bytes of names in %d chunks'
      % (args.out, rules, len(flags), size, longest_probe, sum(len(c) for c in names), len(names)))
