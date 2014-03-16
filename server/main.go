// jeeboot: a server for remote nodes running the JeeBoot loader in firmware.
package main

import (
	"flag"
	"fmt"
	"io/ioutil"

	"github.com/jcw/flow"
	_ "github.com/jcw/flow/gadgets"
	_ "github.com/jcw/jeebus/gadgets/rfdata"
	_ "github.com/jcw/jeebus/gadgets/serial"
)

const Version = "0.1.0"

var (
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
	hexFiles = flag.String("firmware", "../firmware",
		"location of the Intel hex firware files to serve")
)

func main() {
	flag.Parse()

	if *showInfo {
		fmt.Println("JeeBus", Version, "+ Flow", flow.Version)
		return
	}

	// load configuration from file
	config, err := ioutil.ReadFile(*configFile)
	if err != nil {
		panic(err)
	}

	// main flow is: serial -> rf12demo -> jeeboot -> serial
	// other valid packets are routed to the bootServer gadget
	
	c := flow.NewCircuit()
	c.Add("sp", "SerialPort")
	c.Add("rf", "Sketch-RF12demo")
	c.Add("sk", "Sink")
	c.Add("jb", "JeeBoot")
	c.AddCircuitry("server", &bootServer{})
	c.Connect("sp.From", "rf.In", 0)
	c.Connect("rf.Out", "server.In", 0)
	c.Connect("rf.Rej", "sk.In", 0) // throw away rejected serial port msgs
	c.Connect("rf.Oob", "jb.In", 0)
	c.Connect("jb.Out", "sp.To", 0)
	c.Connect("server.Out", "sp.To", 0)
	c.Feed("sp.Port", *serialPort)
	c.Feed("jb.Cfg", config)
	c.Run()
}

type configData struct {
	SwIDs map[string]string
	HwIDs map[string]struct{ Board, Group, Node, SwID int }
}

type bootServer struct {
	flow.Gadget
	In  flow.Input
	Out flow.Output
}

func (g *bootServer) Run() {
	g.Out.Send(1) // reset the serial port
	<-g.In        // wait for some input before sending out the init command
	<-g.In
	g.Out.Send(fmt.Sprintf("%db %dg 31i 1c 1q v", *freqBand/100, *netGroup))

	for m := range g.In {
		fmt.Println("in:", m)
	}
}
