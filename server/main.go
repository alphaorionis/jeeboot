// jeeboot: a server for remote nodes running the JeeBoot loader in firmware.
package main

import (
	"flag"
	"fmt"

	"github.com/jcw/flow"
	_ "github.com/jcw/flow/gadgets"
	_ "github.com/jcw/housemon/gadgets/rfdata"
	_ "github.com/jcw/jeeboot/server/gadgets"
	_ "github.com/jcw/jeebus/gadgets/serial"
)

const Version = "0.1.0"

var (
	describe = flag.Bool("d", false,
		"describe the main circuit used as JeeBoot server")
	showInfo = flag.Bool("i", false,
		"display some information about this tool")
	serialPort = flag.String("dev", "/dev/ttyUSB0",
		"serial port with attached JeeLink/JeeNode running RF12demo")
	freqBand = flag.Int("band", 868,
		"frequency band used to listen for incoming JeeBoot requests")
	netGroup = flag.Int("group", 212,
		"net group used to listen for incoming JeeBoot requests")
	configFile = flag.String("config", "config.json",
		"configuration file containing the swid/hwid details")
)

func main() {
	flag.Parse()

	if *showInfo {
		fmt.Printf("JeeBoot + JeeBus (%s) + Flow (%s)\n", Version, flow.Version)
		flow.PrintRegistry()
		return
	}

	// main processing pipeline: serial, rf12demo, jeeboot, serial
	// firmware: jeeboot, readtext, intelhex, binaryfill, calccrc, bootdata
	// other valid packets are routed to the bootServer gadget

	c := flow.NewCircuit()
	c.Add("sp", "SerialPort")
	c.Add("rf", "Sketch-RF12demo")
	c.Add("sk", "Sink")
	c.Add("cf", "ReadFileJSON")
	c.Add("jb", "JeeBoot")
	c.Add("rd", "ReadFileText")
	c.Add("hx", "IntelHexToBin")
	c.Add("bf", "BinaryFill")
	c.Add("cs", "CalcCrc16")
	c.Add("bd", "BootData")
	c.Add("sv", "BootServer")
	c.Connect("sp.From", "rf.In", 0)
	c.Connect("rf.Out", "sv.In", 0)
	c.Connect("rf.Rej", "sk.In", 0) // throw away rejected serial port msgs
	c.Connect("rf.Oob", "jb.In", 0)
	c.Connect("cf.Out", "jb.Cfg", 0)
	c.Connect("jb.Files", "rd.In", 0)
	c.Connect("rd.Out", "hx.In", 0)
	c.Connect("hx.Out", "bf.In", 0)
	c.Connect("bf.Out", "cs.In", 0)
	c.Connect("cs.Out", "bd.In", 0)
	c.Connect("jb.Out", "sp.To", 0)
	c.Connect("sv.Out", "sp.To", 0)
	c.Feed("sp.Port", *serialPort)
	c.Feed("cf.In", *configFile)
	c.Feed("bf.Len", 64)

	if *describe {
		flow.PrintDescription(c)
		return
	}

	c.Run()
}

func init() {
	flow.Registry["BootServer"] = func() flow.Circuitry { return new(BootServer) }
}

type BootServer struct {
	flow.Gadget
	In  flow.Input
	Out flow.Output
}

func (g *BootServer) Run() {
	g.Out.Send(1) // reset the serial port
	<-g.In        // wait for some input before sending out the init command
	g.Out.Send(fmt.Sprintf("%db %dg 31i 1c 1q v", *freqBand/100, *netGroup))

	for m := range g.In {
		fmt.Println("in:", m)
	}
}
