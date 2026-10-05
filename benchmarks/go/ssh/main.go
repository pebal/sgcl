// SSH in Go, the ssh benchmark's other side (benchmarks/net/ssh.cpp has the
// module's): golang.org/x/crypto/ssh's client against its server in one
// process on the loopback, the same cases on the same bytes; prints
// "ssh <case> ns/op=… ops/s=… wall=…s MB/s=…".
//
//	ssh handshake [n]          a TCP connection, curve25519-sha256, an Ed25519 host key and
//	                           user key, the authentication, closed: per connection
//	ssh exec [n]               a session that runs "true" on one kept connection: open,
//	                           exec, the exit status, closed: per session
//	ssh throughput_gcm [MB]    MB megabytes written to a session's input in writes of 32 KB,
//	                           the server reading them to their end, aes128-gcm@openssh.com:
//	                           per write, and MB/s
//	ssh throughput_chacha [MB] the same with chacha20-poly1305@openssh.com
//
// A module of its own (golang.org/x/crypto, which the benchmarks' module does
// not require), built from the module cache without the network:
// benchmarks/compare.sh writes its go.mod when missing and builds it with
// GOFLAGS=-mod=mod GOPROXY=off GOSUMDB=off.
package main

import (
	"crypto/ed25519"
	"crypto/rand"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"time"

	"golang.org/x/crypto/ssh"
)

func serve(l net.Listener, config *ssh.ServerConfig) {
	for {
		c, err := l.Accept()
		if err != nil {
			return
		}
		go func(c net.Conn) {
			_, chans, reqs, err := ssh.NewServerConn(c, config)
			if err != nil {
				c.Close()
				return
			}
			go ssh.DiscardRequests(reqs)
			for nc := range chans {
				if nc.ChannelType() != "session" {
					nc.Reject(ssh.UnknownChannelType, "")
					continue
				}
				ch, creqs, err := nc.Accept()
				if err != nil {
					continue
				}
				go func(ch ssh.Channel, creqs <-chan *ssh.Request) {
					for r := range creqs {
						if r.Type != "exec" {
							r.Reply(false, nil)
							continue
						}
						r.Reply(true, nil)
						cmd := string(r.Payload[4:])
						if cmd == "sink" {
							io.Copy(io.Discard, ch)
						}
						ch.SendRequest("exit-status", false, []byte{0, 0, 0, 0})
						ch.Close()
						go ssh.DiscardRequests(creqs)
						return
					}
				}(ch, creqs)
			}
		}(c)
	}
}

func report(what string, wall time.Duration, ops float64, mb float64) {
	s := wall.Seconds()
	extra := ""
	if mb > 0 {
		extra = fmt.Sprintf(" MB/s=%.1f", mb/s)
	}
	fmt.Printf("ssh %s ns/op=%.1f ops/s=%.0f wall=%.2fs%s\n", what, s*1e9/ops, ops/s, s, extra)
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: ssh <handshake|exec|throughput_gcm|throughput_chacha> [n]")
		os.Exit(2)
	}
	what := os.Args[1]
	n := 0
	if len(os.Args) > 2 {
		n, _ = strconv.Atoi(os.Args[2])
	}
	_, hostPriv, _ := ed25519.GenerateKey(rand.Reader)
	hostSigner, _ := ssh.NewSignerFromKey(hostPriv)
	_, userPriv, _ := ed25519.GenerateKey(rand.Reader)
	userSigner, _ := ssh.NewSignerFromKey(userPriv)
	config := &ssh.ServerConfig{
		PublicKeyCallback: func(ssh.ConnMetadata, ssh.PublicKey) (*ssh.Permissions, error) { return nil, nil },
	}
	config.AddHostKey(hostSigner)
	config.KeyExchanges = []string{"curve25519-sha256"}
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	go serve(l, config)
	if what == "server" {
		// a server alone, for the module's client (its port on the first line)
		fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
		select {}
	}
	client := &ssh.ClientConfig{
		User:            "bench",
		Auth:            []ssh.AuthMethod{ssh.PublicKeys(userSigner)},
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
		Config:          ssh.Config{KeyExchanges: []string{"curve25519-sha256"}},
	}
	addr := l.Addr().String()
	if a := os.Getenv("SGCL_SSH_SERVER"); a != "" {
		addr = a // the module's server, for Go's client
	}
	switch what {
	case "handshake":
		if n == 0 {
			n = 2000
		}
		t0 := time.Now()
		for i := 0; i < n; i++ {
			c, err := ssh.Dial("tcp", addr, client)
			if err != nil {
				panic(err)
			}
			c.Close()
		}
		report(what, time.Since(t0), float64(n), 0)
	case "exec":
		if n == 0 {
			n = 5000
		}
		c, err := ssh.Dial("tcp", addr, client)
		if err != nil {
			panic(err)
		}
		t0 := time.Now()
		for i := 0; i < n; i++ {
			s, err := c.NewSession()
			if err != nil {
				panic(err)
			}
			if err := s.Run("true"); err != nil {
				panic(err)
			}
			s.Close()
		}
		report(what, time.Since(t0), float64(n), 0)
		c.Close()
	case "throughput_gcm", "throughput_chacha":
		if n == 0 {
			n = 1024
		}
		if what == "throughput_gcm" {
			client.Ciphers = []string{"aes128-gcm@openssh.com"}
		} else {
			client.Ciphers = []string{"chacha20-poly1305@openssh.com"}
		}
		c, err := ssh.Dial("tcp", addr, client)
		if err != nil {
			panic(err)
		}
		s, err := c.NewSession()
		if err != nil {
			panic(err)
		}
		in, _ := s.StdinPipe()
		if err := s.Start("sink"); err != nil {
			panic(err)
		}
		block := make([]byte, 32768)
		writes := n * 32
		t0 := time.Now()
		for i := 0; i < writes; i++ {
			if _, err := in.Write(block); err != nil {
				panic(err)
			}
		}
		in.Close()
		if err := s.Wait(); err != nil {
			panic(err)
		}
		report(what, time.Since(t0), float64(writes), float64(n))
		c.Close()
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
}
