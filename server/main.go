package main

import (
    "github.com/jcw/jeebus"
)

const Version = "0.2.0"

func main() {
    println("\nJeeBoot Server", Version, "/ JeeBus", jeebus.Version)
    jeebus.Run()
}
