package main

import (
	//"fmt"
	"log"
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
	Success  Status = iota
	Timeout         // Connectiom timeout
	ConReset        // Connection reset
	ConLost         // Lost connection during send/recv
	OtherError
	Ended
)

func (s Status) String() string {
	return [...]string{"Success", "Timeout", "Connection reset", "Connection lost", "Other error"}[s]
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
	Timestamp int32
	Con       int
	Proto     Proto
	Operation Operation
	Duration  time.Duration // duration
	Size      int64
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

	//metricPrefix := fmt.Sprintf("Hello from %d\n", config.MetricPrefix, id)
	count := config.Connections / config.Workers
	if config.Connections%config.Workers > 0 {
		count++
	}
	start := id * count
	end := start + count
	if end > config.Connections {
		end = config.Connections
	}
	//var pos := start
	b.Await()
	if config.Verbose {
		log.Printf("Started TCP worker %d\n", id)
	}
	for running {
		/*
			if pos == end {
				pos = start
			}
				SEEK:
					for i := pos; i < end; i++ {
						if ! conns[i].Connected {
							break
						}
					}
		*/
		r.Timestamp = int32(time.Now().Unix())
		time.Sleep(time.Second)
	}
	if config.Verbose {
		log.Printf("Shutdown TCP worker %d\n", id)
	}
}
