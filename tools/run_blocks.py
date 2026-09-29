#!/usr/bin/env python3
# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
"""The examples of the docs, compiled and run (DESIGN 318).

Every ```cpp block of a page that holds `int main(` is a program, and the
programs of a page are numbered from 0 in their order (page_N in what the
check prints). Each is compiled (clang++ -std=c++20 -O1 -Wall
-Wno-unused-variable, the last because the examples count with
`for (int i : range(N))`; a warning counts as a failure) and run, its
standard input empty. A program followed by an answer block (its label
right after the program, or after one line of text, a sentence the reader
needs about it) has what it gives compared with the page, or only run:

  Output:            what the program writes, stdout and stderr in the
                     order written, after it ends
  Sample output:     what one run wrote, of a program whose output varies
                     from run to run with data that is not the reader's
                     (the time, ids, how threads interleave, random bytes,
                     core counts): compiled and run as a program with no
                     answer block is, not compared, counted as checked
  A request to it:   a server that runs until it is stopped: started, and
                     each `$ curl ...` line of the block run in turn (curl
                     itself, no shell; the first retried until the port
                     answers), its stdout compared with the lines under it,
                     the program killed. The port of the page (the one in
                     the first curl's URL) is replaced by a free one in the
                     program and in every request. When the block is
                     followed by `Its output:`, what the program wrote,
                     stdout and stderr, is compared with that block

A program with no answer block after it (the page shows no output: what it
prints may be the reader's own data, a directory listing, a path) is compiled
and run, and what it writes is not compared: a warning, a failed compile or
an exit status other than zero is a bad block, and the program counts as
checked. A program followed by an answer block under another label, of any
length (`The output:`, `Output (macOS):`), is not checked: each is named
with an UNCHECKED line, and the summary counts them apart, `unchecked N`
beside `bad N` (which counts only the programs checked), so a page with
blocks the checker passed over never reads as clean.

The time of a log line is the run's own: `time=<RFC 3339>` and
`"time":"<RFC 3339>"` are masked on both sides, and so are what an access
log line holds of the run (`duration=`, the client's port in `remote=`,
curl's version in `user_agent=`). A program that fetches from
the network is given a local server instead: `--serve URL=FILE` (repeated)
replaces URL in the program by a local address that answers with the
file's bytes, so a page may show a real URL and the check needs no network.
`--echo URL` (repeated) does the same for an echo service such as
httpbin.org/post: a POST there is answered with JSON whose "data" is the
body it carried, as httpbin answers. `--tls HOST:PORT` (repeated) replaces
HOST:PORT by localhost and a local TLS 1.3 server of the tree's test
certificate (tests/net/tls_testdata, a leaf for localhost), which answers
any request with `HTTP/1.1 200 OK` and closes, "h2" chosen by ALPN when
offered; `--tls HOST:PORT=TEXT` writes TEXT (with \n) at once instead, for
a client that sends nothing. The program is run with SSL_CERT_FILE naming
the test CA, so the system's roots trust it.

What a page's programs need (local servers, a fresh directory) is listed
in tools/run_blocks.pages, one line per page — its path from the root,
then the options — so that the check of any page needs no arguments and
no network, and the page itself carries nothing:

  docs/sgcl/net/http/client.md --tmp --serve https://example.com/a.txt=LICENSE --echo https://httpbin.org/post

  tools/run_blocks.py docs/sgcl/slog/README.md ...  [--root DIR] [--tmp]
                      [--fill] [--serve URL=FILE ...] [--echo URL ...]
                      [--tls HOST:PORT ...] [--skip N ...] [--strict]

--root is the tree the programs include from and run in (the repository by
default: this script's parent); --tmp runs each in a fresh directory
instead, with `tests` there a link to the tree's, for a program that reads
its test data; --fill writes what a program printed into an answer block
that holds {{OUTPUT}}; --strict fails the run (exit status 1) when any
program went unchecked, as a bad block does. --changed compiles and runs only the programs
whose text or answer differs from the page in git HEAD (a change of prose, a
title or a label costs no compilation). --skip N (repeated, one per
program; meant for a page's line in run_blocks.pages) names the program N
of the page that is not compiled at all: a fragment around
an `int main` (no includes, names the page leaves out), a server that runs
until it is stopped with no request block, a program that needs arguments;
it counts neither as checked nor as unchecked.
"""
import http.server
import json
import os
import shlex
import re
import socket
import socketserver
import ssl
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'run_blocks.pages')


def page_options(root, path):
    """The options listed for a page in run_blocks.pages, as a list of words"""
    rel = os.path.relpath(path, root)
    if not os.path.exists(PAGES):
        return []
    for line in open(PAGES):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        name, _, rest = line.partition(' ')
        if name == rel:
            return shlex.split(rest)
    return []
# a program: every ```cpp block that holds `int main(`
PROGRAM = re.compile(r'```cpp\n((?:(?!```).)*int main\((?:(?!```).)*)```', re.S)
# a program and its answer, the label right after it or after one line of text
BLOCK = re.compile(r'```cpp\n((?:(?!```).)*int main\((?:(?!```).)*)```\n\n(?:(?!```)[^\n]+\n\n)?(Output|Sample output|A request to it):\n\n```text\n(.*?)```', re.S)
# a program followed by an answer block under any label (not the next program
# after a line that ends in a colon): the ones BLOCK does not take are unchecked
LABELLED = re.compile(r'```cpp\n(?:(?!```).)*int main\((?:(?!```).)*```\n\n((?!```)[^\n]+):\n\n```(?!cpp)', re.S)
TIMES = [re.compile(r'time=\d{4}-\d\d-\d\dT[0-9:.]+(?:Z|[+-]\d\d:\d\d)'),
         re.compile(r'"time":"\d{4}-\d\d-\d\dT[0-9:.]+(?:Z|[+-]\d\d:\d\d)"')]
# what a server's log line holds of the run: the time taken, the client's port, curl's version
RUN = [(re.compile(r'duration=[0-9.]+[a-zµ]+'), 'duration=D'),
       (re.compile(r'remote=(127\.0\.0\.1|\[::1\]):\d+'), r'remote=\1:P'),
       (re.compile(r'user_agent=curl/[0-9.]+'), 'user_agent=curl/V')]
ITS_OUTPUT = re.compile(r'\A\n\nIts output:\n\n```text\n(.*?)```', re.S)


def masked(text):
    text = TIMES[0].sub('time=T', text)
    text = TIMES[1].sub('"time":"T"', text)
    for pattern, by in RUN:
        text = pattern.sub(by, text)
    return text


def requests_of(block):
    """The `$ curl ...` lines of a block and the expected stdout of each"""
    out = []
    for line in block.splitlines(keepends=True):
        if line.startswith('$ '):
            out.append([shlex.split(line[2:]), ''])
        elif out:
            out[-1][1] += line
    return out


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def serve_files(mapping, echoes):
    """A local server answering each path with its file: {path: bytes}; a
    POST to a path of echoes with its body as httpbin's data field"""
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_POST(self):
            data = self.rfile.read(int(self.headers.get('Content-Length', '0')))
            if self.path not in echoes:
                self.send_response(404)
                self.end_headers()
                return
            body = json.dumps({'data': data.decode('utf-8', 'replace')}).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            body = mapping.get(self.path)
            if body is None:
                self.send_response(404)
                self.end_headers()
                return
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    server = socketserver.ThreadingTCPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def serve_tls(root, hello=None):
    """A local TLS server of the tree's test certificate: any request is
    answered with a 200 and the connection closed; with hello, those bytes
    written at once"""
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_3
    ctx.set_alpn_protocols(['h2', 'http/1.1'])
    data = os.path.join(root, 'tests', 'net', 'tls_testdata')
    ctx.load_cert_chain(os.path.join(data, 'ecdsa.pem'), os.path.join(data, 'ecdsa.key'))

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            try:
                conn = ctx.wrap_socket(self.request, server_side=True)
                if hello is not None:
                    conn.sendall(hello)
                    conn.close()
                    return
                got = b''
                while b'\r\n\r\n' not in got:
                    part = conn.recv(4096)
                    if not part:
                        break
                    got += part
                conn.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
                conn.close()
            except (OSError, ssl.SSLError):
                pass

    server = socketserver.ThreadingTCPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def options(args, into):
    """The options of a command line or of a page's directive, into a dict"""
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--root':
            into['root'] = os.path.abspath(args[i + 1])
            i += 1
        elif a == '--tmp':
            into['tmp'] = True
        elif a == '--fill':
            into['fill'] = True
        elif a == '--strict':
            into['strict'] = True
        elif a == '--changed':
            into['changed'] = True
        elif a == '--serve':
            url, path = args[i + 1].split('=', 1)
            into['serve'].append((url, path))
            i += 1
        elif a == '--echo':
            into['echo'].append(args[i + 1])
            i += 1
        elif a == '--tls':
            into['tls'].append(args[i + 1])
            i += 1
        elif a == '--skip':
            into['skip'].add(int(args[i + 1]))
            i += 1
        else:
            into['pages'].append(a)
        i += 1
    return into


def local_servers(o, root):
    """The rewrites of the URLs the options name to the local servers
    started for them, and the environment the programs run in"""
    rewrites, env = [], dict(os.environ)
    if o['serve'] or o['echo']:
        files, echoes = {}, set()
        for n, (url, path) in enumerate(o['serve']):
            local = '/%d/%s' % (n, url.rstrip('/').rsplit('/', 1)[-1])
            files[local] = open(path if os.path.isabs(path) else os.path.join(root, path), 'rb').read()
            rewrites.append((url, local))
        for n, url in enumerate(o['echo']):
            local = '/echo%d/%s' % (n, url.rstrip('/').rsplit('/', 1)[-1])
            echoes.add(local)
            rewrites.append((url, local))
        server = serve_files(files, echoes)
        port = server.server_address[1]
        rewrites = [(url, 'http://127.0.0.1:%d%s' % (port, local)) for url, local in rewrites]
    if o['tls']:
        for spec in o['tls']:
            address, _, hello = spec.partition('=')
            server = serve_tls(root, hello.replace('\\n', '\n').encode() if hello else None)
            rewrites.append((address, 'localhost:%d' % server.server_address[1]))
        env['SSL_CERT_FILE'] = os.path.join(root, 'tests', 'net', 'tls_testdata', 'ca.pem')
    return rewrites, env


def main():
    args = sys.argv[1:]
    if not args or '--help' in args or '-h' in args:
        print(__doc__.strip())
        return 0
    base = options(args, {'root': ROOT, 'tmp': False, 'fill': False, 'strict': False, 'changed': False, 'serve': [], 'echo': [], 'tls': [], 'skip': set(), 'pages': []})
    root, fill, pages = base['root'], base['fill'], base['pages']

    work = tempfile.mkdtemp(prefix='run_blocks-')
    flags = ['-framework', 'ImageIO', '-framework', 'CoreGraphics', '-framework', 'CoreFoundation',
             '-framework', 'Accelerate'] if sys.platform == 'darwin' else []
    bad = 0
    unchecked = 0
    for page in pages:
        path = page if os.path.isabs(page) else os.path.join(root, page)
        text = open(path).read()
        o = {'tmp': base['tmp'], 'serve': list(base['serve']), 'echo': list(base['echo']), 'tls': list(base['tls']), 'skip': set(base['skip']), 'pages': []}
        options(page_options(root, path), o)
        tmp = o['tmp']
        rewrites, env = local_servers(o, root)
        answered = {m.start(): m for m in BLOCK.finditer(text)}
        labelled = {m.start(): m for m in LABELLED.finditer(text)}
        # --changed: a program whose text and answer are as in HEAD is not compiled again
        as_in_head = set()
        if base['changed']:
            rel = os.path.relpath(path, root)
            h = subprocess.run(['git', '-C', root, 'show', 'HEAD:' + rel], capture_output=True, text=True)
            if h.returncode == 0:
                head_answered = {m.start(): m for m in BLOCK.finditer(h.stdout)}
                for hp in PROGRAM.finditer(h.stdout):
                    hm = head_answered.get(hp.start())
                    as_in_head.add((hp.group(1), hm.group(2) if hm else None, hm.group(3) if hm else None))
        for k, p in enumerate(PROGRAM.finditer(text)):
            name = os.path.basename(path)[:-3] + '_%d' % k
            if k in o['skip']:
                print(name, 'skipped')
                continue
            m = answered.get(p.start())
            if m is None and p.start() in labelled:
                line = text.count('\n', 0, p.start()) + 1
                print('%s:%d UNCHECKED: the answer of a program under "%s:", which the checker does not read (it reads "Output:" with a ```text block)' % (os.path.basename(path), line, labelled[p.start()].group(1)))
                unchecked += 1
                continue
            if m is None:
                code, kind, expected = p.group(1), None, None   # no answer block: run, not compared
            else:
                code, kind, expected = m.group(1), m.group(2), m.group(3)
            if base['changed'] and (code, kind, expected) in as_in_head:
                print(name, 'as in HEAD, not compiled')
                continue
            request = None
            its_output = None
            if kind == 'A request to it':
                request = requests_of(expected)
                page_port = None
                for argv, _ in request:
                    for a in argv:
                        u = re.match(r'https?://(?:localhost|127\.0\.0\.1):(\d+)', a)
                        if u and page_port is None:
                            page_port = u.group(1)
                if not request or any(argv[0] != 'curl' for argv, _ in request) or page_port is None:
                    print(name, 'BLOCK: lines `$ curl ... http://localhost:<port>/...`, curl only')
                    bad += 1
                    continue
                port = str(free_port())
                code = code.replace(':' + page_port, ':' + port)
                request = [([a.replace(':' + page_port, ':' + port) for a in argv], want) for argv, want in request]
                m_out = ITS_OUTPUT.match(text, m.end())
                its_output = m_out.group(1) if m_out else None
            for url, local in rewrites:
                code = code.replace(url, local)
            src = os.path.join(work, name + '.cpp')
            exe = os.path.join(work, name)
            open(src, 'w').write(code)
            c = subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wno-unused-variable', '-I' + root, src, '-o', exe] + flags,
                               capture_output=True, text=True)
            if c.returncode != 0 or c.stderr.strip():
                print(name, 'COMPILE', c.stderr[:3000])
                bad += 1
                if c.returncode != 0:
                    continue
            cwd = root
            if tmp:
                cwd = tempfile.mkdtemp(dir=work)
                os.symlink(os.path.join(root, 'tests'), os.path.join(cwd, 'tests'))
            if request:
                log = open(os.path.join(work, name + '.log'), 'w+')
                proc = subprocess.Popen([exe], cwd=cwd, stdout=log, stderr=subprocess.STDOUT, env=env)
                got, failed = [], None
                for n, (argv, want) in enumerate(request):
                    deadline = time.time() + 20
                    while True:
                        r = subprocess.run(argv[:1] + ['--max-time', '10'] + argv[1:], cwd=root, capture_output=True, text=True)
                        # 7: nothing listens yet; the first request waits for the server
                        if r.returncode != 7 or n or time.time() > deadline or proc.poll() is not None:
                            break
                        time.sleep(0.05)
                    if r.returncode != 0:
                        failed = ' '.join(argv) + ': curl exit %d %s' % (r.returncode, r.stderr[:300])
                        break
                    got.append((want, r.stdout))
                time.sleep(0.3)   # what the server writes after its answer (a log line)
                proc.kill()
                proc.wait()
                log.seek(0)
                written = log.read()
                if failed:
                    print(name, 'NO ANSWER', failed, written[:500])
                    bad += 1
                    continue
                differs = [(w, g) for w, g in got if masked(w) != masked(g)]
                if its_output is not None and masked(its_output) != masked(written):
                    differs.append((its_output, written))
                if differs:
                    for w, g in differs:
                        print(name, 'DIFFERS\n--- page\n' + w + '--- program\n' + g)
                    bad += 1
                else:
                    print(name, 'ok')
                continue
            else:
                # stdout and stderr in one stream, in the order written: a
                # log line on io::stderr is part of what the page shows
                try:
                    r = subprocess.run([exe], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                       text=True, cwd=cwd, timeout=300, env=env)
                except subprocess.TimeoutExpired:
                    print(name, 'TIMEOUT after 300 s')
                    bad += 1
                    continue
                out = r.stdout
                if r.returncode != 0:
                    print(name, 'EXIT', r.returncode, out[:1000])
                    bad += 1
                    continue
            if kind is None:
                # no answer block: run to its end, not compared
                print(name, 'ok (run only, not compared)')
                continue
            if '{{' in expected:
                if fill and not request:
                    text = text.replace(m.group(0), m.group(0).replace('```text\n' + expected, '```text\n' + out))
                    open(path, 'w').write(text)
                    print(name, 'FILLED:\n' + out)
                else:
                    print(name, 'OUTPUT (placeholder):\n' + out)
            elif kind == 'Sample output':
                # one run's output: run to its end, not compared
                print(name, 'ok (sample output, not compared)')
            elif masked(out) != masked(expected):
                print(name, 'DIFFERS\n--- page\n' + expected + '--- program\n' + out)
                bad += 1
            else:
                print(name, 'ok')
    print('bad', bad)
    if unchecked:
        print('unchecked', unchecked)
    return 1 if bad or (base['strict'] and unchecked) else 0


if __name__ == '__main__':
    sys.exit(main())
