// POP3 in Go's standard library alone: Go has no POP3, so this side is a
// minimal client and a minimal server written by hand from RFC 1939 on
// bufio (benchmarks/net/pop3.cpp has the SGCL side, the same cases).
// Prints one line, ns per operation.
//
//	serve                 a minimal server: user "bench"/"bench", INBOX of 1000 messages of 4 KB
//	                      (the same bytes as bench_pop3's), USER, PASS, STAT, LIST, UIDL, RETR,
//	                      NOOP, QUIT; prints "port N", serves until killed
//	pop3_noop ADDR [n]    NOOP and its answer: per command
//	pop3_list ADDR [n]    LIST and UIDL of the 1000 messages: per message
//	pop3_retr ADDR [n]    RETR of each message in turn: per message
//	pop3_retr_batch ADDR [n]  RETR of all 1000 written at once, the answers read: per message
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

func report(what string, d time.Duration, n int) {
	ns := float64(d.Nanoseconds()) / float64(n)
	fmt.Printf("pop3 %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, ns, float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

func message(i int) string {
	var b strings.Builder
	fmt.Fprintf(&b, "From: Sender %d <s%d@example.com>\r\nSubject: Message %d\r\n\r\n", i, i, i)
	for b.Len() < 4096 {
		b.WriteString("The quick brown fox jumps over the lazy dog, line after line of it.\r\n")
	}
	return b.String()
}

func stuffed(m string) []byte {
	var out bytes.Buffer
	for _, line := range strings.SplitAfter(m, "\r\n") {
		if line == "" {
			continue
		}
		if line[0] == '.' {
			out.WriteByte('.')
		}
		out.WriteString(line)
	}
	out.WriteString(".\r\n")
	return out.Bytes()
}

func serve(c net.Conn, msgs [][]byte, sizes []int) {
	defer c.Close()
	r := bufio.NewReader(c)
	w := bufio.NewWriter(c)
	w.WriteString("+OK ready\r\n")
	w.Flush()
	for {
		line, err := r.ReadString('\n')
		if err != nil {
			return
		}
		f := strings.Fields(strings.TrimRight(line, "\r\n"))
		if len(f) == 0 {
			continue
		}
		switch strings.ToUpper(f[0]) {
		case "USER":
			w.WriteString("+OK\r\n")
		case "PASS":
			w.WriteString("+OK logged in\r\n")
		case "CAPA":
			w.WriteString("+OK\r\nUIDL\r\nTOP\r\nUSER\r\nPIPELINING\r\n.\r\n")
		case "NOOP":
			w.WriteString("+OK\r\n")
		case "STAT":
			total := 0
			for _, s := range sizes {
				total += s
			}
			fmt.Fprintf(w, "+OK %d %d\r\n", len(sizes), total)
		case "LIST":
			w.WriteString("+OK\r\n")
			for i, s := range sizes {
				fmt.Fprintf(w, "%d %d\r\n", i+1, s)
			}
			w.WriteString(".\r\n")
		case "UIDL":
			w.WriteString("+OK\r\n")
			for i := range sizes {
				fmt.Fprintf(w, "%d 1.%d\r\n", i+1, i+1)
			}
			w.WriteString(".\r\n")
		case "RETR":
			n, _ := strconv.Atoi(f[1])
			if n < 1 || n > len(msgs) {
				w.WriteString("-ERR no such message\r\n")
				break
			}
			fmt.Fprintf(w, "+OK %d octets\r\n", sizes[n-1])
			w.Write(msgs[n-1])
		case "QUIT":
			w.WriteString("+OK bye\r\n")
			w.Flush()
			return
		default:
			w.WriteString("-ERR unknown\r\n")
		}
		if r.Buffered() == 0 {
			w.Flush()
		}
	}
}

type client struct {
	c net.Conn
	r *bufio.Reader
	w *bufio.Writer
}

func (c *client) status() string {
	line, err := c.r.ReadString('\n')
	if err != nil || !strings.HasPrefix(line, "+OK") {
		fmt.Fprintln(os.Stderr, "bad reply:", line, err)
		os.Exit(1)
	}
	return line
}

func (c *client) body() []byte {
	var out []byte
	for {
		line, err := c.r.ReadSlice('\n')
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		if len(line) == 3 && line[0] == '.' {
			return out
		}
		if line[0] == '.' {
			line = line[1:]
		}
		out = append(out, line...)
	}
}

func (c *client) cmd(s string) string {
	c.w.WriteString(s + "\r\n")
	c.w.Flush()
	return c.status()
}

func dial(addr string) *client {
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	c := &client{conn, bufio.NewReaderSize(conn, 65536), bufio.NewWriter(conn)}
	c.status()
	c.cmd("USER bench")
	c.cmd("PASS bench")
	return c
}

func main() {
	switch os.Args[1] {
	case "serve":
		var msgs [][]byte
		var sizes []int
		for i := 0; i < 1000; i++ {
			m := message(i)
			msgs = append(msgs, stuffed(m))
			sizes = append(sizes, len(m))
		}
		l, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			panic(err)
		}
		fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
		for {
			c, err := l.Accept()
			if err != nil {
				return
			}
			go serve(c, msgs, sizes)
		}
	case "pop3_noop":
		c := dial(os.Args[2])
		n := count(3, 50000)
		start := time.Now()
		for i := 0; i < n; i++ {
			c.cmd("NOOP")
		}
		report("pop3_noop", time.Since(start), n)
		c.cmd("QUIT")
	case "pop3_list":
		c := dial(os.Args[2])
		rounds := count(3, 200)
		total := 0
		start := time.Now()
		for i := 0; i < rounds; i++ {
			c.w.WriteString("LIST\r\nUIDL\r\n")
			c.w.Flush()
			c.status()
			sizes := map[int]int{}
			for _, line := range strings.Split(string(c.body()), "\r\n") {
				f := strings.Fields(line)
				if len(f) == 2 {
					n, _ := strconv.Atoi(f[0])
					s, _ := strconv.Atoi(f[1])
					sizes[n] = s
				}
			}
			c.status()
			uids := map[int]string{}
			for _, line := range strings.Split(string(c.body()), "\r\n") {
				f := strings.Fields(line)
				if len(f) == 2 {
					n, _ := strconv.Atoi(f[0])
					uids[n] = f[1]
				}
			}
			total += len(sizes)
		}
		report("pop3_list", time.Since(start), total)
		c.cmd("QUIT")
	case "pop3_retr":
		c := dial(os.Args[2])
		rounds := count(3, 20)
		total := 0
		start := time.Now()
		for r := 0; r < rounds; r++ {
			for i := 1; i <= 1000; i++ {
				c.cmd("RETR " + strconv.Itoa(i))
				if len(c.body()) < 4096 {
					os.Exit(1)
				}
				total++
			}
		}
		report("pop3_retr", time.Since(start), total)
		c.cmd("QUIT")
	case "pop3_retr_batch":
		c := dial(os.Args[2])
		rounds := count(3, 20)
		total := 0
		start := time.Now()
		for r := 0; r < rounds; r++ {
			for i := 1; i <= 1000; i++ {
				c.w.WriteString("RETR " + strconv.Itoa(i) + "\r\n")
			}
			c.w.Flush()
			for i := 1; i <= 1000; i++ {
				c.status()
				if len(c.body()) < 4096 {
					os.Exit(1)
				}
				total++
			}
		}
		report("pop3_retr_batch", time.Since(start), total)
		c.cmd("QUIT")
	default:
		fmt.Fprintln(os.Stderr, "usage: pop3 serve | pop3 <pop3_noop|pop3_list|pop3_retr|pop3_retr_batch> ADDR [n]")
		os.Exit(2)
	}
}
