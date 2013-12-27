// JeeBoot - Custom RFM12B driver for boot loader use, no interrupts
// 2012-11-01 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#ifndef RF12_h
#define RF12_h

#include <stdint.h>
#include <util/crc16.h>

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny44__)

#define RFM_IRQ_BIT     1
#define RFM_IRQ_DDR     DDRB
#define RFM_IRQ_PIN     PINB
#define RFM_IRQ_PORT    PORTB

#else 

// ATmega168, ATmega328, etc.
#define RFM_IRQ_BIT     2
#define RFM_IRQ_DDR     DDRD
#define RFM_IRQ_PIN     PIND
#define RFM_IRQ_PORT    PORTD

#endif

// version 1 did not include the group code in the crc
// version 2 does include the group code in the crc
#define RF12_VERSION    2

#define rf12_grp        rf12_buf[0]
#define rf12_hdr        rf12_buf[1]
#define rf12_len        rf12_buf[2]
#define rf12_data       (rf12_buf + 3)

#define RF12_HDR_CTL    0x80
#define RF12_HDR_DST    0x40
#define RF12_HDR_ACK    0x20
#define RF12_HDR_MASK   0x1F

#define RF12_MAXDATA    66

#define RF12_433MHZ     1
#define RF12_868MHZ     2
#define RF12_915MHZ     3

// EEPROM address range used by the rf12_config() code
#define RF12_EEPROM_ADDR ((uint8_t*) 0x20)
#define RF12_EEPROM_SIZE 32
#define RF12_EEPROM_EKEY (RF12_EEPROM_ADDR + RF12_EEPROM_SIZE)
#define RF12_EEPROM_ELEN 16

// shorthand to simplify sending out the proper ACK when requested
#define RF12_WANTS_ACK ((rf12_hdr & RF12_HDR_ACK) && !(rf12_hdr & RF12_HDR_CTL))
#define RF12_ACK_REPLY (rf12_hdr & RF12_HDR_DST ? RF12_HDR_CTL : \
            RF12_HDR_CTL | RF12_HDR_DST | (rf12_hdr & RF12_HDR_MASK))
            
// options fro RF12_sleep()
#define RF12_SLEEP 0
#define RF12_WAKEUP -1

extern volatile uint16_t rf12_crc;  // running crc value, should be zero at end
extern volatile uint8_t rf12_buf[]; // recv/xmit buf including hdr & crc bytes

// call this once with the node ID, frequency band, and optional group
static void rf12_initialize(uint8_t id, uint8_t band, uint8_t group);

// call this frequently, returns true if a packet has been received
static uint8_t rf12_recvDone(void);

// call this to check whether a new transmission can be started
// returns true when a new transmission may be started with rf12_sendStart()
static uint8_t rf12_canSend(void);

// call this only when rf12_recvDone() or rf12_canSend() return true
static void rf12_sendStart(uint8_t hdr, const void* ptr, uint8_t len);

#endif

// RFM12B driver implementation
// 2009-02-09 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php
// $Id: RF12.cpp 7733 2011-06-22 01:37:05Z jcw $

// maximum transmit / receive buffer: 3 header + data + 2 crc bytes
#define RF_MAX   (RF12_MAXDATA + 5)

// pins used for the RFM12B interface
#if defined(__AVR_ATmega1280__)

#define RFM_IRQ     2
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      0
#define SPI_SS      53
#define SPI_MOSI    51
#define SPI_MISO    50
#define SPI_SCK     52

#elif defined(__AVR_ATtiny84__)

#define RFM_IRQ     2
#define SS_DDR      DDRA
#define SS_PORT     PORTA
#define SS_BIT      7
#define SPI_SS      3   // PA7, pin 6
#define SPI_MISO    4   // PA6, pin 7
#define SPI_MOSI    5   // PA5, pin 8
#define SPI_SCK     6   // PA4, pin 9

#else

// ATmega328, etc.
#define RFM_IRQ     2
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      2       // for PORTB: 2 = d.10, 1 = d.9, 0 = d.8
#define SPI_SS      10      // do not change, must point to h/w SPI pin
#define SPI_MOSI    11
#define SPI_MISO    12
#define SPI_SCK     13

#endif 

// RF12 command codes
#define RF_RECEIVER_ON  0x82DD
#define RF_XMITTER_ON   0x823D
#define RF_IDLE_MODE    0x820D
#define RF_SLEEP_MODE   0x8205
#define RF_WAKEUP_MODE  0x8207
#define RF_TXREG_WRITE  0xB800
#define RF_RX_FIFO_READ 0xB000
#define RF_WAKEUP_TIMER 0xE000

// RF12 status bits
#define RF_LBD_BIT      0x0400
#define RF_RSSI_BIT     0x0100

// bits in the node id configuration byte
#define NODE_BAND       0xC0        // frequency band
#define NODE_ACKANY     0x20        // ack on broadcast packets if set
#define NODE_ID         0x1F        // id of this node, as A..Z or 1..31

// transceiver states, these determine what to do with each interrupt
enum {
    TXCRC1, TXCRC2, TXTAIL, TXDONE, TXIDLE,
    TXRECV,
    TXPRE1, TXPRE2, TXPRE3, TXSYN1, TXSYN2,
};

static uint8_t nodeid;              // address of this node
static uint8_t group;               // network group
static volatile uint8_t rxfill;     // number of data bytes in rf12_buf
static volatile int8_t rxstate;     // current transceiver state

#define RETRIES     8               // stop retrying after 8 times
#define RETRY_MS    1000            // resend packet every second until ack'ed

volatile uint16_t rf12_crc;         // running crc value
volatile uint8_t rf12_buf[RF_MAX];  // recv/xmit buf, including hdr & crc bytes

static void spi_initialize () {
    bitSet(SS_PORT, SS_BIT);
    bitSet(SS_DDR, SS_BIT);
    // digitalWrite(SPI_SS, 1);
    bitSet(DDRB, 2);
    // pinMode(SPI_SS, OUTPUT);
    // pinMode(SPI_MOSI, OUTPUT);
    // pinMode(SPI_MISO, INPUT);
    // pinMode(SPI_SCK, OUTPUT);
    DDRB |= bit(2) | bit(3) | bit(4) | bit(5);
#ifdef SPCR    
#if F_CPU <= 10000000
    // clk/4 is ok for the RF12's SPI
    SPCR = _BV(SPE) | _BV(MSTR);
#else
    // use clk/8 (2x 1/16th) to avoid exceeding RF12's SPI specs of 2.5 MHz
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
    SPSR |= _BV(SPI2X);
#endif
#else
    // ATtiny
    USICR = bit(USIWM0);
#endif
}

static uint8_t rf12_byte (uint8_t out) {
#ifdef SPDR
    SPDR = out;
    // this loop spins 4 usec with a 2 MHz SPI clock
    while (!(SPSR & _BV(SPIF)))
        ;
    return SPDR;
#else
    // ATtiny
    USIDR = out;
    byte v1 = bit(USIWM0) | bit(USITC);
    byte v2 = bit(USIWM0) | bit(USITC) | bit(USICLK);
#if F_CPU <= 5000000
    // only unroll if resulting clock stays under 2.5 MHz
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
#else
    for (uint8_t i = 0; i < 8; ++i) {
        USICR = v1;
        USICR = v2;
    }
#endif
    return USIDR;
#endif
}

static uint16_t rf12_xfer (uint16_t cmd) {
    bitClear(SS_PORT, SS_BIT);
    uint16_t reply = rf12_byte(cmd >> 8) << 8;
    reply |= rf12_byte(cmd);
    bitSet(SS_PORT, SS_BIT);
    return reply;
}

static void rf12_interrupt() {
    // a transfer of 2x 16 bits @ 2 MHz over SPI takes 2x 8 us inside this ISR
    rf12_xfer(0x0000);
    
    if (rxstate == TXRECV) {
        uint8_t in = rf12_xfer(RF_RX_FIFO_READ);

        if (rxfill == 0 && group != 0)
            rf12_buf[rxfill++] = group;
            
        rf12_buf[rxfill++] = in;
        rf12_crc = _crc16_update(rf12_crc, in);

        if (rxfill >= rf12_len + 5 || rxfill >= RF_MAX)
            rf12_xfer(RF_IDLE_MODE);
    } else {
        uint8_t out;

        if (rxstate < 0) {
            uint8_t pos = 3 + rf12_len + rxstate++;
            out = rf12_buf[pos];
            rf12_crc = _crc16_update(rf12_crc, out);
        } else
            switch (rxstate++) {
                case TXSYN1: out = 0x2D; break;
                case TXSYN2: out = rf12_grp; rxstate = - (2 + rf12_len); break;
                case TXCRC1: out = rf12_crc; break;
                case TXCRC2: out = rf12_crc >> 8; break;
                case TXDONE: rf12_xfer(RF_IDLE_MODE); // fall through
                default:     out = 0xAA;
            }
            
        rf12_xfer(RF_TXREG_WRITE + out);
    }
}

static void rf12_recvStart () {
    rxfill = rf12_len = 0;
    rf12_crc = ~0;
#if RF12_VERSION >= 2
    if (group != 0)
        rf12_crc = _crc16_update(~0, group);
#endif
    rxstate = TXRECV;    
    rf12_xfer(RF_RECEIVER_ON);
}

static uint8_t rf12_recvDone () {
    // if (digitalRead(RFM_IRQ) == 0)
    if (bitRead(RFM_IRQ_PIN, RFM_IRQ_BIT) == 0)
        rf12_interrupt();
        
    if (rxstate == TXRECV && (rxfill >= rf12_len + 5 || rxfill >= RF_MAX)) {
        rxstate = TXIDLE;
        if (rf12_len > RF12_MAXDATA)
            rf12_crc = 1; // force bad crc if packet length is invalid
        if (!(rf12_hdr & RF12_HDR_DST) || (nodeid & NODE_ID) == 31 ||
                (rf12_hdr & RF12_HDR_MASK) == (nodeid & NODE_ID)) {
            return 1; // it's a broadcast packet or it's addressed to this node
        }
    }
    if (rxstate == TXIDLE)
        rf12_recvStart();
    return 0;
}

static uint8_t rf12_canSend () {
    // no need to test with interrupts disabled: state TXRECV is only reached
    // outside of ISR and we don't care if rxfill jumps from 0 to 1 here
    if (rxstate == TXRECV && rxfill == 0 &&
            (rf12_byte(0x00) & (RF_RSSI_BIT >> 8)) == 0) {
        rf12_xfer(RF_IDLE_MODE); // stop receiver
        //XXX just in case, don't know whether these RF12 reads are needed!
        // rf12_xfer(0x0000); // status register
        // rf12_xfer(RF_RX_FIFO_READ); // fifo read
        rxstate = TXIDLE;
        rf12_grp = group;
        return 1;
    }
    return 0;
}

static void rf12_sendStart (uint8_t hdr, const void* ptr, uint8_t len) {
    rf12_len = len;
    memcpy((void*) rf12_data, ptr, len);
    rf12_hdr = hdr & RF12_HDR_DST ? hdr :
                (hdr & ~RF12_HDR_MASK) + (nodeid & NODE_ID);
    
    rf12_crc = ~0;
#if RF12_VERSION >= 2
    rf12_crc = _crc16_update(rf12_crc, rf12_grp);
#endif
    rxstate = TXPRE1;
    rf12_xfer(RF_XMITTER_ON); // bytes will be fed via interrupts
}

/*
  Call this once with the node ID (0-31), frequency band (0-3), and
  optional group (0-255 for RF12B, only 212 allowed for RF12).
*/
static void rf12_initialize (uint8_t id, uint8_t band, uint8_t g) {
    nodeid = id;
    group = g;
		printf("RF12 id=%d b=%d g=%d\n", id, band, g);
    
    spi_initialize();
    
    // pinMode(RFM_IRQ, INPUT);
    // digitalWrite(RFM_IRQ, 1); // pull-up
    bitClear(RFM_IRQ_DDR, RFM_IRQ_BIT);
    bitSet(RFM_IRQ_PORT, RFM_IRQ_BIT);

    rf12_xfer(0x0000); // intitial SPI transfer added to avoid power-up problem

    rf12_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd
    
    // wait until RFM12B is out of power-up reset, this takes several *seconds*
    rf12_xfer(RF_TXREG_WRITE); // in case we're still in OOK mode
    // while (digitalRead(RFM_IRQ) == 0)
    while (bitRead(RFM_IRQ_PIN, RFM_IRQ_BIT) == 0)
        rf12_xfer(0x0000);
        
    rf12_xfer(0x80C7 | (band << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF 
    rf12_xfer(0xA640); // 868MHz 
    rf12_xfer(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps
    //rf12_xfer(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm 
    rf12_xfer(0x94B2); // VDI,FAST,134kHz,-?dBm,-91dBm 
    rf12_xfer(0xC2AC); // AL,!ml,DIG,DQD4 
    if (group != 0) {
        rf12_xfer(0xCA83); // FIFO8,2-SYNC,!ff,DR 
        rf12_xfer(0xCE00 | group); // SYNC=2DXX； 
    } else {
        rf12_xfer(0xCA8B); // FIFO8,1-SYNC,!ff,DR 
        rf12_xfer(0xCE2D); // SYNC=2D； 
    }
    rf12_xfer(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN 
    //rf12_xfer(0x9850); // !mp,90kHz,MAX OUT 
    rf12_xfer(0x9857); // !mp,90kHz,MIN OUT 
    rf12_xfer(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0 
    rf12_xfer(0xE000); // NOT USE 
    rf12_xfer(0xC800); // NOT USE 
    rf12_xfer(0xC049); // 1.66MHz,3.1V 

    rxstate = TXIDLE;
    // if ((nodeid & NODE_ID) != 0)
    //     attachInterrupt(0, rf12_interrupt, LOW);
    // else
    //     detachInterrupt(0);
}

void rf12_sendNow(uint8_t hdr, const void* ptr, uint8_t len) {
  while (!rf12_canSend())
    rf12_recvDone(); // keep the driver state machine going, ignore incoming
  rf12_sendStart(hdr, ptr, len);
}

void rf12_sendWait(uint8_t mode) {
  while (rxstate < TXIDLE)
    rf12_recvDone();
}
