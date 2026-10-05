// The Go peer of the net::smtp tests, standard library only.
//
//	go_smtp server MODE [CERT KEY]   a minimal SMTP server written by hand on
//	                                 net/textproto; prints "port N", then one
//	                                 JSON line per message it takes (the
//	                                 envelope, the parameters, the data). MODE
//	                                 is a list of words joined by '+':
//	                                   plain       no extension but 8BITMIME
//	                                   pipelining  PIPELINING (replies of a
//	                                               batch written together)
//	                                   chunking    CHUNKING (BDAT)
//	                                   dsn         DSN
//	                                   utf8        SMTPUTF8
//	                                   size        SIZE 1000
//	                                   starttls    STARTTLS (CERT, KEY)
//	                                   auth        AUTH PLAIN LOGIN XOAUTH2
//	                                               (alice / secret / token)
//	                                   rejectbad   RCPT of "bad*" refused 550
//	                                   helo        EHLO refused (502)
//	                                   tempfail    MAIL refused 451
//	                                   garbage     the greeting is not a reply
//	                                   hangup      the connection closed after
//	                                               the greeting
//	                                   silent      no greeting at all
//	                                   injectdata  bytes after STARTTLS's 220
//	go_smtp client ADDR FROM TO...   net/smtp's client sends stdin as the
//	                                 message (smtp.SendMail; PLAIN with
//	                                 SMTP_USER/SMTP_PASS, the root CA with
//	                                 SMTP_CA); prints "ok" or the error
//	go_smtp bench ADDR N SIZE        net/smtp sends N messages of SIZE bytes
//	                                 over one connection; prints ns/op
package main

import (
	"bufio"
	"crypto/tls"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/smtp"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

type record struct {
	From   string   `json:"from"`
	To     []string `json:"to"`
	Mail   string   `json:"mail"`
	Rcpt   []string `json:"rcpt"`
	Data   string   `json:"data"`
	TLS    bool     `json:"tls"`
	User   string   `json:"user"`
	Helo   string   `json:"helo"`
	ViaBDAT bool    `json:"bdat"`
}

var out sync.Mutex

func emit(r record) {
	b, _ := json.Marshal(r)
	out.Lock()
	os.Stdout.Write(append(b, '\n'))
	out.Unlock()
}

func has(mode, word string) bool {
	for _, w := range strings.Split(mode, "+") {
		if w == word {
			return true
		}
	}
	return false
}

func serve(c net.Conn, mode string, cfg *tls.Config) {
	defer c.Close()
	if has(mode, "silent") {
		time.Sleep(30 * time.Second)
		return
	}
	r := bufio.NewReader(c)
	w := bufio.NewWriter(c)
	reply := func(s string) {
		w.WriteString(s + "\r\n")
		if r.Buffered() == 0 || !has(mode, "pipelining") {
			w.Flush()
		}
	}
	if has(mode, "garbage") {
		reply("hello there")
		return
	}
	reply("220 go.test ESMTP")
	if has(mode, "hangup") {
		return
	}
	rec := record{}
	tlsOn := false
	inMail := false
	for {
		line, err := r.ReadString('\n')
		if err != nil {
			return
		}
		line = strings.TrimRight(line, "\r\n")
		verb := strings.ToUpper(line)
		if i := strings.IndexByte(verb, ' '); i >= 0 {
			verb = verb[:i]
		}
		switch verb {
		case "EHLO":
			if has(mode, "helo") {
				reply("502 5.5.2 no EHLO here")
				continue
			}
			rec.Helo = strings.TrimSpace(line[4:])
			ext := []string{"go.test"}
			ext = append(ext, "8BITMIME")
			if has(mode, "pipelining") {
				ext = append(ext, "PIPELINING")
			}
			if has(mode, "chunking") {
				ext = append(ext, "CHUNKING")
			}
			if has(mode, "dsn") {
				ext = append(ext, "DSN")
			}
			if has(mode, "utf8") {
				ext = append(ext, "SMTPUTF8")
			}
			if has(mode, "size") {
				ext = append(ext, "SIZE 1000")
			}
			if has(mode, "starttls") && !tlsOn {
				ext = append(ext, "STARTTLS")
			}
			if has(mode, "auth") {
				ext = append(ext, "AUTH PLAIN LOGIN XOAUTH2")
			}
			for i, e := range ext {
				if i == len(ext)-1 {
					reply("250 " + e)
				} else {
					reply("250-" + e)
				}
			}
		case "HELO":
			rec.Helo = strings.TrimSpace(line[4:])
			reply("250 go.test")
		case "STARTTLS":
			if has(mode, "injectdata") {
				w.WriteString("220 go ahead\r\n250 injected\r\n")
				w.Flush()
				return
			}
			reply("220 2.0.0 ready")
			tc := tls.Server(c, cfg)
			if err := tc.Handshake(); err != nil {
				return
			}
			c = tc
			r = bufio.NewReader(c)
			w = bufio.NewWriter(c)
			tlsOn = true
		case "AUTH":
			f := strings.Fields(line)
			ok := false
			if len(f) >= 2 && strings.EqualFold(f[1], "PLAIN") {
				resp := ""
				if len(f) >= 3 {
					resp = f[2]
				} else {
					reply("334 ")
					resp, _ = r.ReadString('\n')
					resp = strings.TrimSpace(resp)
				}
				b, _ := base64.StdEncoding.DecodeString(resp)
				p := strings.Split(string(b), "\x00")
				ok = len(p) == 3 && p[1] == "alice" && p[2] == "secret"
				rec.User = "alice"
			} else if len(f) >= 2 && strings.EqualFold(f[1], "LOGIN") {
				reply("334 VXNlcm5hbWU6")
				u, _ := r.ReadString('\n')
				reply("334 UGFzc3dvcmQ6")
				pw, _ := r.ReadString('\n')
				ub, _ := base64.StdEncoding.DecodeString(strings.TrimSpace(u))
				pb, _ := base64.StdEncoding.DecodeString(strings.TrimSpace(pw))
				ok = string(ub) == "alice" && string(pb) == "secret"
				rec.User = string(ub)
			} else if len(f) >= 3 && strings.EqualFold(f[1], "XOAUTH2") {
				b, _ := base64.StdEncoding.DecodeString(f[2])
				ok = string(b) == "user=alice\x01auth=Bearer token\x01\x01"
				if !ok {
					reply("334 eyJzdGF0dXMiOiI0MDEifQ==")
					r.ReadString('\n')
				}
				rec.User = "alice"
			}
			if ok {
				reply("235 2.7.0 ok")
			} else {
				rec.User = ""
				reply("535 5.7.8 bad credentials")
			}
		case "MAIL":
			if has(mode, "tempfail") {
				reply("451 4.3.0 try later")
				continue
			}
			rec.Mail = line
			from := line[strings.Index(line, "<")+1 : strings.Index(line, ">")]
			rec.From = from
			inMail = true
			reply("250 2.1.0 ok")
		case "RCPT":
			if !inMail {
				reply("503 5.5.1 MAIL first")
				continue
			}
			to := line[strings.Index(line, "<")+1 : strings.Index(line, ">")]
			if has(mode, "rejectbad") && strings.HasPrefix(to, "bad") {
				reply("550 5.1.1 no such user")
				continue
			}
			rec.To = append(rec.To, to)
			rec.Rcpt = append(rec.Rcpt, line)
			reply("250 2.1.5 ok")
		case "DATA":
			if len(rec.To) == 0 {
				reply("554 5.5.1 no valid recipients")
				continue
			}
			reply("354 go on")
			w.Flush()
			var data strings.Builder
			for {
				l, err := r.ReadString('\n')
				if err != nil {
					return
				}
				if l == ".\r\n" {
					break
				}
				if strings.HasPrefix(l, ".") {
					l = l[1:]
				}
				data.WriteString(l)
			}
			rec.Data = data.String()
			rec.TLS = tlsOn
			emit(rec)
			rec = record{Helo: rec.Helo, User: rec.User}
			inMail = false
			reply("250 2.0.0 queued as GO1")
		case "BDAT":
			f := strings.Fields(line)
			n, _ := strconv.Atoi(f[1])
			buf := make([]byte, n)
			if _, err := io.ReadFull(r, buf); err != nil {
				return
			}
			rec.Data += string(buf)
			if len(f) >= 3 && strings.EqualFold(f[2], "LAST") {
				if len(rec.To) == 0 {
					reply("554 5.5.1 no valid recipients")
					rec = record{Helo: rec.Helo, User: rec.User}
					continue
				}
				rec.TLS = tlsOn
				rec.ViaBDAT = true
				emit(rec)
				rec = record{Helo: rec.Helo, User: rec.User}
				inMail = false
				reply("250 2.0.0 queued as GO2")
			} else {
				reply("250 2.0.0 chunk")
			}
		case "RSET":
			rec = record{Helo: rec.Helo, User: rec.User}
			inMail = false
			reply("250 2.0.0 ok")
		case "NOOP":
			reply("250 2.0.0 ok")
		case "VRFY":
			reply("252 2.5.2 cannot verify")
		case "QUIT":
			reply("221 2.0.0 bye")
			return
		default:
			reply("500 5.5.1 what")
		}
	}
}

func main() {
	switch os.Args[1] {
	case "server":
		mode := os.Args[2]
		var cfg *tls.Config
		if len(os.Args) >= 5 {
			cert, err := tls.LoadX509KeyPair(os.Args[3], os.Args[4])
			if err != nil {
				panic(err)
			}
			cfg = &tls.Config{Certificates: []tls.Certificate{cert}}
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
			go serve(c, mode, cfg)
		}
	case "client":
		addr, from, to := os.Args[2], os.Args[3], os.Args[4:]
		msg, _ := io.ReadAll(os.Stdin)
		var auth smtp.Auth
		host, _, _ := net.SplitHostPort(addr)
		if u := os.Getenv("SMTP_USER"); u != "" {
			auth = smtp.PlainAuth("", u, os.Getenv("SMTP_PASS"), host)
		}
		if ca := os.Getenv("SMTP_CA"); ca != "" {
			// a client by hand: SendMail's TLS takes the system's roots
			pem, _ := os.ReadFile(ca)
			pool := x509.NewCertPool()
			pool.AppendCertsFromPEM(pem)
			c, err := smtp.Dial(addr)
			if err != nil {
				fmt.Println("error:", err)
				return
			}
			if err := c.StartTLS(&tls.Config{RootCAs: pool, ServerName: "localhost"}); err != nil {
				fmt.Println("error:", err)
				return
			}
			if auth != nil {
				if err := c.Auth(smtp.PlainAuth("", os.Getenv("SMTP_USER"), os.Getenv("SMTP_PASS"), host)); err != nil {
					fmt.Println("error:", err)
					return
				}
			}
			if err := c.Mail(from); err != nil {
				fmt.Println("error:", err)
				return
			}
			for _, t := range to {
				if err := c.Rcpt(t); err != nil {
					fmt.Println("error:", err)
					return
				}
			}
			wc, err := c.Data()
			if err != nil {
				fmt.Println("error:", err)
				return
			}
			wc.Write(msg)
			if err := wc.Close(); err != nil {
				fmt.Println("error:", err)
				return
			}
			c.Quit()
			fmt.Println("ok")
			return
		}
		if err := smtp.SendMail(addr, auth, from, to, msg); err != nil {
			fmt.Println("error:", err)
			return
		}
		fmt.Println("ok")
	case "bench":
		addr := os.Args[2]
		n, _ := strconv.Atoi(os.Args[3])
		size, _ := strconv.Atoi(os.Args[4])
		msg := []byte("From: a@example.com\r\nTo: b@example.com\r\nSubject: bench\r\n\r\n" + strings.Repeat("x", size) + "\r\n")
		c, err := smtp.Dial(addr)
		if err != nil {
			panic(err)
		}
		c.Hello("localhost")
		start := time.Now()
		for i := 0; i < n; i++ {
			c.Mail("a@example.com")
			c.Rcpt("b@example.com")
			w, _ := c.Data()
			w.Write(msg)
			if err := w.Close(); err != nil {
				panic(err)
			}
		}
		el := time.Since(start)
		c.Quit()
		fmt.Printf("ns/op=%d\n", el.Nanoseconds()/int64(n))
	}
}
