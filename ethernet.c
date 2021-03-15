// Ethernet Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Configuring Wireshark to examine packets
//-----------------------------------------------------------------------------

// sudo ethtool --offload eno2 tx off rx off
// in wireshark, preferences->protocol->ipv4->validate the checksum if possible
// in wireshark, preferences->protocol->udp->validate the checksum if possible

//-----------------------------------------------------------------------------
// Sending UDP test packets
//-----------------------------------------------------------------------------

// test this with a udp send utility like sendip
//   if sender IP (-is) is 192.168.1.1, this will attempt to
//   send the udp datagram (-d) to 192.168.1.199, port 1024 (-ud)
// sudo sendip -p ipv4 -is 192.168.1.1 -p udp -ud 1024 -d "on" 192.168.1.199
// sudo sendip -p ipv4 -is 192.168.1.1 -p udp -ud 1024 -d "off" 192.168.1.199

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "clock.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "uart_input.h"
#include "wait.h"
#include "eth0.h"
#include "tm4c123gh6pm.h"

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4
uint32_t ReadIndex = 0;
uint32_t WriteIndex = 0;
uint32_t BUFFER_LENGTH = 100;
char chartxBuffer[100];
uint8_t mqtt_ip[];
extern uint8_t macAddress[];
extern uint8_t ipAddress[];
extern uint8_t ipGwAddress[];

//-----------------------------------------------------------------------------
// Subroutines                
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
}

void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    etherGetMacAddress(mac);
    putsUart0("HW: ");
    for (i = 0; i < 6; i++)
    {
        sprintf(str, "%02x", mac[i]);
        putsUart0(str);
        if (i < 6 - 1)
            putcUart0(':');
    }
    putcUart0('\n');
    putcUart0('\r');
    etherGetIpAddress(ip);
    putsUart0("IP: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    if (etherIsDhcpEnabled())
        putsUart0(" (dhcp)");
    else
        putsUart0(" (static)");
    putcUart0('\n');
    putcUart0('\r');
    etherGetIpSubnetMask(ip);
    putsUart0("SN: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    putcUart0('\r');
    etherGetIpGatewayAddress(ip);
    putsUart0("GW: ");
    for (i = 0; i < 4; i++)
    {
        sprintf(str, "%u", ip[i]);
        putsUart0(str);
        if (i < 4 - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    putcUart0('\r');
    if (etherIsLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
}
void displayUart0(char str[])
{

    bool full;

    full = ((WriteIndex + 1) % BUFFER_LENGTH) == ReadIndex;

    if (!full)
    {
        while (str[WriteIndex] != '\0')
        {
            chartxBuffer[WriteIndex] = str[WriteIndex];
            WriteIndex = (WriteIndex + 1) % BUFFER_LENGTH;
        }
        if (UART0_FR_R & UART_FR_TXFE)
        {
            UART0_DR_R = chartxBuffer[ReadIndex];
            ReadIndex = (ReadIndex + 1) % BUFFER_LENGTH;
        }

        UART0_IM_R = UART_IM_TXIM;
    }

}

bool checkCommand(USER_DATA data_input)
{
    bool valid = false;
    if (isCommand(&data_input, "mqtt", 4))
    {
        mqtt_ip[0] = getFieldInteger(&data_input, 1);
        mqtt_ip[1] = getFieldInteger(&data_input, 2);
        mqtt_ip[2] = getFieldInteger(&data_input, 3);
        mqtt_ip[3] = getFieldInteger(&data_input, 4);

        putsUart0("mqtt ip set");
        putcUart0('\n');
        putcUart0('\r');
//           char str[16];
//           sprintf(str, " %u", mqtt_ip[1]);
//           putsUart0(str);
//           putcUart0('\n');
        valid = true;
        return valid;
    }
    if (isCommand(&data_input, "connect", 0))
    {
        valid = true;
        return valid;
    }

    if (isCommand(&data_input, "reboot", 0))
    {
        putsUart0("rebooted");
        valid = true;
        return valid;
    }

    if (!valid)
    {
        putsUart0("Invalid command\n\r");
        return valid;
    }
    return valid;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

int main(void)

{
    uint8_t *udpData;
    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader*) buffer;
    USER_DATA data_input;
    socket tcp;
    // Init controller
    initHw();

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init ethernet interface (eth0)
    putsUart0("\nStarting eth0\n");
    etherSetMacAddress(2, 3, 4, 5, 6, 109);
    etherDisableDhcpMode();
    etherSetIpAddress(192, 168, 2, 109);
    etherSetIpSubnetMask(255, 255, 255, 0);
    etherSetIpGatewayAddress(192, 168, 2, 1);
    etherInit(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    waitMicrosecond(100000);
    displayConnectionInfo();

    // Flash LED
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);

    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    while (true)
    {

        // Put terminal processing here
        if (kbhitUart0())
        {
            getsUart0(&data_input);
            putsUart0(data_input.buffer);

            // Parse fields
            parseFields(&data_input);

            // Echo back the parsed field information (type and fields)

            putcUart0('\n');
            putcUart0('\r');
            bool valid = false;
            if (isCommand(&data_input, "connect", 0))
            {
                uint8_t ip[4];
                etherGetIpGatewayAddress(ip);
                etherSendArpRequest(data, ip);
                putsUart0("send arp");
                valid = true;

            }
            if (isCommand(&data_input, "tcp", 0))
            {
                uint16_t flag = 0x002;

                sendTCP(data, tcp, flag);

            }

            valid = checkCommand(data_input);
            putcUart0('\n');
            putcUart0('\r');

        }

        if (etherIsDataAvailable())
        {

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);
             waitMicrosecond(100000);
            // Handle ARP request

            if (etherIsArpResponse(data))
            {
                uint8_t i = 0;
                uint8_t mqtt_mac[6];
                for (i = 0; i < 6; i++)
                {
                    mqtt_mac[i] = data->sourceAddress[i];
                    tcp.dest_Hw[i] = mqtt_mac[i];
                    tcp.source_Hw[i] = macAddress[i];
                    char str[16];
                    sprintf(str, " %u", tcp.source_Hw[i]);
                    putsUart0(str);
                    putsUart0(".");

                }

                for (i = 0; i < 4; i++)
                {

                    tcp.dest_Ip[i] = ipGwAddress[i];
                    tcp.source_Ip[i] = ipAddress[i];
                }
                tcp.dest_port = 1883;
                tcp.source_port = 100000;

                //waitMicrosecond(100000);

                putcUart0('\n');
                putcUart0('\r');
            }

        }

    }

    // Packet processing

}

