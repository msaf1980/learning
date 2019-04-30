package main

import (
	"fmt"
	"io"
	"log"
	"net"
	"strings"
	"time"
)

type Proto int

const (
	Tcp Proto = iota
	Udp
)

func (p Proto) String() string {
	return [...]string{"TCP", "UDP"}[p]
}

type Status int

const (
	NetSuccess    Status = iota
	NetConTimeout        // Connectiom timeout
	NetConRefused        // Connection refused
	NetAddrLookup        // Address lookup
	NetConEOF            // Connection closed
	OtherError           // Unparsed error
	Ended                // Set when worker go to shutdown
)

func (s Status) String() string {
	return [...]string{"SUCCESS", "TIMEOUT", "REFUSED", "ERRLOOKUP", "EOF", "ERROTHER", "ENDED"}[s]
}

func GetNetError(err error) Status {
	if err == nil {
		return NetSuccess
	}
	if err == io.EOF {
		return NetConEOF
	}
	netErr, ok := err.(net.Error)
	if ok {
		if netErr.Timeout() {
			return NetConTimeout
		} else if strings.Contains(err.Error(), " lookup ") {
			return NetAddrLookup
		} else if strings.HasSuffix(err.Error(), ": connection refused") {
			return NetConRefused
		} else if strings.HasSuffix(err.Error(), ": broken pipe") ||
			strings.HasSuffix(err.Error(), ": connection reset by peer") ||
			strings.HasSuffix(err.Error(), "EOF") {
			return NetConEOF
		}
	}
	return OtherError
}

type Operation int

const (
	Connect  Operation = iota
	Send               //Send
	Recv               // Recv
	SendRecv           // Send/Recv summary
)

func (o Operation) String() string {
	return [...]string{"CONNECT", "SEND", "RECV", "MSG"}[o]
}

type Result struct {
	Id        int
	Timestamp time.Time
	Con       int
	Proto     Proto
	Operation Operation
	Duration  time.Duration // duration
	Size      int
	Status    Status
}

func ResultNew(id int, proto Proto) *Result {
	r := new(Result)
	r.Proto = proto
	return r
}

func TcpWorker(id int, config Config, out chan<- Result) {
	r := ResultNew(id, Tcp)

	defer func(ch chan<- Result) {
		r.Status = Ended
		ch <- *r
	}(out)

	message := fmt.Sprintf("Hello from %d\n", id)
	bytes := []byte(message)
	count := config.Connections / config.Workers
	if config.Connections%config.Workers > 0 {
		count++
	}
	start := id * count
	end := start + count
	if end > config.Connections {
		end = config.Connections
	}
	pos := start
	b.Await()
	if config.Verbose {
		log.Printf("Started TCP worker %d\n", id)
	}
	for running {
		if pos == end {
			pos = start
		}
		if conns[pos].Connected && conns[pos].Count >= config.Send {
			// sended messages per connection reached, close
			conns[pos].Conn.Close()
			conns[pos].Connected = false
			conns[pos].Count = 0
		}
		if !conns[pos].Connected {
			var err error
			r.Operation = Connect
			r.Size = 0
			r.Timestamp = time.Now()
			conns[pos].Conn, err = net.DialTimeout("tcp", config.Addr, config.ConTimeout)
			r.Duration = time.Since(r.Timestamp)
			r.Status = GetNetError(err)
			if err == nil {
				conns[pos].Connected = true
				conns[pos].Count = 0
			}
			if config.Verbose && r.Status == OtherError {
				log.Print(err.Error())
			}
			out <- *r
		}
		if conns[pos].Connected {
			r.Operation = Send
			r.Timestamp = time.Now()
			conns[pos].Conn.SetDeadline(r.Timestamp.Add(config.Timeout))
			n, err := conns[pos].Conn.Write(bytes)
			r.Duration = time.Since(r.Timestamp)
			r.Size = n
			r.Status = GetNetError(err)
			out <- *r
			if err != nil {
				conns[pos].Connected = false
				conns[pos].Conn.Close()
				if config.Verbose && r.Status == OtherError {
					log.Print(err.Error())
				}
			} else {
				conns[pos].Count++
				r.Operation = Recv
				r.Timestamp = time.Now()
				n, err := conns[pos].Conn.Read(bytes)
				r.Duration = time.Since(r.Timestamp)
				r.Size = n
				r.Status = GetNetError(err)
				if err != nil {
					conns[pos].Connected = false
					conns[pos].Conn.Close()
					if config.Verbose && r.Status == OtherError {
						log.Print(err.Error())
					}
				}
				out <- *r
			}
		}
		time.Sleep(config.Delay)
		pos++
	}
	if config.Verbose {
		log.Printf("Shutdown TCP worker %d\n", id)
	}
}
