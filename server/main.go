package main

import (
    "log"
    "github.com/jcw/jeebus"
)

const Version = "0.0.1"

func init() {
    log.SetFlags(log.Ltime) // only display HH:MM:SS time in log entries
}

func main() {
    println("\nServer", Version, "/ JeeBus", jeebus.Version)
    jeebus.Run()
}
