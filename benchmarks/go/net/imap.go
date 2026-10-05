// The IMAP cases' Go side (benchmarks/net/imap.cpp has the SGCL side): Go's
// standard library has no IMAP, so this is a minimal client written by hand
// from RFC 9051 with the standard library alone (bufio, net, strconv),
// against the same server as the module's client ($SGCL_IMAP_SERVER, the
// module's `imap server`): commands written whole, responses read line by
// line with their literals, FETCH's items taken apart into a struct per
// message (UID, flags, the envelope's fields as strings, the body's bytes),
// SEARCH's numbers parsed. Prints one line, ns per operation.
//
//	imap_noop [n]            NOOP and its answer: per command
//	imap_fetch_flags [n]     UID FETCH 1:* (UID FLAGS) of INBOX: per message
//	imap_fetch_body [n]      UID FETCH 1:* (UID BODY.PEEK[]) of Big: per message, and MB/s
//	imap_fetch_envelope [n]  UID FETCH 1:* (UID FLAGS ENVELOPE) of INBOX: per message
//	imap_search [n]          UID SEARCH FROM "carol" of INBOX: per message searched
//	imap_append [n]          APPEND of a 2 KB message to "Drop": per message
//	imap_pipeline [n]        1000 NOOPs written at once, their answers read: per command
package main

import (
	"bufio"
	"bytes"
	"fmt"
	"net"
	"os"
	"strconv"
	"strings"
	"time"
)

type imapConn struct {
	c   net.Conn
	r   *bufio.Reader
	w   *bufio.Writer
	tag int
}

type imapMessage struct {
	uid      uint32
	flags    []string
	envelope []string
	body     []byte
}

// One response whole: its line and its literals, as one []byte with the
// literals inline, and the literals apart
func (ic *imapConn) response() ([]byte, [][]byte, error) {
	var resp []byte
	var lits [][]byte
	for {
		line, err := ic.r.ReadSlice('\n')
		if err != nil {
			return nil, nil, err
		}
		line = bytes.TrimRight(line, "\r\n")
		resp = append(resp, line...)
		if len(line) > 0 && line[len(line)-1] == '}' {
			open := bytes.LastIndexByte(line, '{')
			if open >= 0 {
				num := strings.TrimSuffix(string(line[open+1:len(line)-1]), "+")
				if n, err := strconv.Atoi(num); err == nil {
					lit := make([]byte, n)
					if _, err := ioReadFull(ic.r, lit); err != nil {
						return nil, nil, err
					}
					lits = append(lits, lit)
					resp = append(resp, lit...)
					continue
				}
			}
		}
		return resp, lits, nil
	}
}

func ioReadFull(r *bufio.Reader, b []byte) (int, error) {
	n := 0
	for n < len(b) {
		m, err := r.Read(b[n:])
		n += m
		if err != nil {
			return n, err
		}
	}
	return n, nil
}

// A command run: the untagged responses before its tagged answer
func (ic *imapConn) run(cmd string, untagged func([]byte, [][]byte)) error {
	ic.tag++
	tag := "G" + strconv.Itoa(ic.tag)
	ic.w.WriteString(tag + " " + cmd + "\r\n")
	if err := ic.w.Flush(); err != nil {
		return err
	}
	for {
		resp, lits, err := ic.response()
		if err != nil {
			return err
		}
		if bytes.HasPrefix(resp, []byte(tag+" ")) {
			if !bytes.HasPrefix(resp[len(tag)+1:], []byte("OK")) {
				return fmt.Errorf("%s", resp)
			}
			return nil
		}
		if untagged != nil && len(resp) > 2 && resp[0] == '*' {
			untagged(resp, lits)
		}
	}
}

func imapDial(addr string) *imapConn {
	c, err := net.Dial("tcp", addr)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	ic := &imapConn{c: c, r: bufio.NewReaderSize(c, 65536), w: bufio.NewWriter(c)}
	if _, _, err := ic.response(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	if err := ic.run("LOGIN bench bench", nil); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	return ic
}

// The strings of a parenthesized list, quoted strings and atoms and NIL, as
// an envelope holds them (nested lists flattened)
func imapStrings(b []byte, i int) ([]string, int) {
	var out []string
	depth := 0
	for i < len(b) {
		switch c := b[i]; {
		case c == '(':
			depth++
			i++
		case c == ')':
			depth--
			i++
			if depth == 0 {
				return out, i
			}
		case c == ' ':
			i++
		case c == '"':
			j := i + 1
			var s []byte
			for j < len(b) && b[j] != '"' {
				if b[j] == '\\' {
					j++
				}
				s = append(s, b[j])
				j++
			}
			out = append(out, string(s))
			i = j + 1
		default:
			j := i
			for j < len(b) && b[j] != ' ' && b[j] != ')' && b[j] != '(' {
				j++
			}
			out = append(out, string(b[i:j]))
			i = j
		}
	}
	return out, i
}

// A FETCH response's items: UID, FLAGS, ENVELOPE, BODY[]
func imapFetch(resp []byte, lits [][]byte) (imapMessage, bool) {
	var m imapMessage
	open := bytes.IndexByte(resp, '(')
	if open < 0 {
		return m, false
	}
	b := resp[open+1:]
	i := 0
	lit := 0
	for i < len(b) && b[i] != ')' {
		for i < len(b) && b[i] == ' ' {
			i++
		}
		j := i
		for j < len(b) && b[j] != ' ' && b[j] != '[' {
			j++
		}
		name := string(b[i:j])
		switch {
		case name == "UID":
			k := j + 1
			for k < len(b) && b[k] >= '0' && b[k] <= '9' {
				k++
			}
			u, _ := strconv.ParseUint(string(b[j+1:k]), 10, 32)
			m.uid = uint32(u)
			i = k
		case name == "FLAGS":
			m.flags, i = imapStrings(b, j+1)
		case name == "ENVELOPE":
			m.envelope, i = imapStrings(b, j+1)
		case strings.HasPrefix(name, "BODY"):
			close := bytes.IndexByte(b[j:], ']')
			if close < 0 || lit >= len(lits) {
				return m, false
			}
			m.body = lits[lit]
			// past "{n}" and the literal's bytes
			k := j + close + 1
			brace := bytes.IndexByte(b[k:], '}')
			i = k + brace + 1 + len(lits[lit])
			lit++
		default:
			return m, false
		}
	}
	return m, m.uid != 0
}

func imapCase(what string, n int64) bool {
	addr := os.Getenv("SGCL_IMAP_SERVER")
	if addr == "" {
		fmt.Fprintln(os.Stderr, "SGCL_IMAP_SERVER: the address of the module's `imap server`")
		return false
	}
	ic := imapDial(addr)
	defer ic.c.Close()
	ok := true
	switch what {
	case "imap_noop":
		if n == 0 {
			n = 50000
		}
		for i := 0; i < 1000; i++ {
			ok = ok && ic.run("NOOP", nil) == nil
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			ok = ok && ic.run("NOOP", nil) == nil
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
	case "imap_fetch_flags", "imap_fetch_envelope", "imap_fetch_body":
		if n == 0 {
			n = 100
			if what == "imap_fetch_body" {
				n = 50
			}
		}
		mailbox, items, want := "INBOX", "(UID FLAGS)", 1000
		if what == "imap_fetch_envelope" {
			items = "(UID FLAGS ENVELOPE)"
		} else if what == "imap_fetch_body" {
			mailbox, items, want = "Big", "(UID BODY.PEEK[])", 100
		}
		ok = ic.run("SELECT "+mailbox, nil) == nil
		fetch := func() ([]imapMessage, error) {
			var out []imapMessage
			err := ic.run("UID FETCH 1:* "+items, func(resp []byte, lits [][]byte) {
				if bytes.Contains(resp[:min(len(resp), 24)], []byte(" FETCH ")) {
					if m, good := imapFetch(resp, lits); good {
						out = append(out, m)
					}
				}
			})
			return out, err
		}
		fetch()
		messages, total := 0, 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			ms, err := fetch()
			ok = ok && err == nil && len(ms) == want
			messages += len(ms)
			for _, m := range ms {
				total += len(m.body)
			}
		}
		wall := time.Since(t0).Seconds()
		extra := ""
		if what == "imap_fetch_body" {
			extra = fmt.Sprintf(" MB/s=%.1f", float64(total)/wall/1e6)
		}
		report(what, wall, float64(messages), extra)
	case "imap_search":
		if n == 0 {
			n = 50
		}
		ok = ic.run("SELECT INBOX", nil) == nil
		search := func() int {
			var found []uint32
			ic.run("UID SEARCH FROM \"carol\"", func(resp []byte, _ [][]byte) {
				for _, f := range strings.Fields(string(resp))[2:] {
					if u, err := strconv.ParseUint(f, 10, 32); err == nil {
						found = append(found, uint32(u))
					}
				}
			})
			return len(found)
		}
		search()
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			ok = ok && search() == 250
		}
		report(what, time.Since(t0).Seconds(), float64(n*1000), "")
	case "imap_append":
		if n == 0 {
			n = 5000
		}
		body := "From: a@b\r\nSubject: dropped\r\n\r\n"
		for len(body) < 2048 {
			body += "padding padding padding padding padding padding padding padding\r\n"
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			ok = ok && ic.run("APPEND Drop {"+strconv.Itoa(len(body))+"+}\r\n"+body, nil) == nil
		}
		report(what, time.Since(t0).Seconds(), float64(n), "")
	case "imap_pipeline":
		if n == 0 {
			n = 200
		}
		var batch strings.Builder
		for i := 0; i < 1000; i++ {
			batch.WriteString("n" + strconv.Itoa(i) + " NOOP\r\n")
		}
		text := batch.String()
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			ic.w.WriteString(text)
			ic.w.Flush()
			for {
				resp, _, err := ic.response()
				if err != nil {
					ok = false
					break
				}
				if bytes.HasPrefix(resp, []byte("n999 ")) {
					break
				}
			}
		}
		report(what, time.Since(t0).Seconds(), float64(n*1000), "")
	}
	ic.run("LOGOUT", nil)
	return ok
}
