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
#include <stdlib.h>
#include "clock.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "uart_input.h"
#include "wait.h"
#include "eth0.h"
#include "tm4c123gh6pm.h"
#include "eeprom.h"

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
extern bool enterispressed;
uint32_t sequencenum = 0;
uint32_t acknum = 0;
uint16_t flag = 0;
uint32_t SENDACK = 0;
uint16_t data_length = 0;

uint16_t eeprom_address = 0;
typedef enum _mqttstate
{
    idle,
    sendArpReq,
    waitArpResp,
    sendTcpSyn,
    waitTcpSynAck,
    sendTcpAck,
    sendMqttConnect,
    waitMqttResp,
} mqttstate;
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

uint32_t MQTTremaininglength(uint32_t X)
{
    uint32_t encodedByte = 0;
    do
    {
        encodedByte = X % 128;
        X = X / 128;
        if (X > 0)
        {
            encodedByte = encodedByte | 128;
        }

    }
    while (X > 0);
    return encodedByte;
}

void encodeUTF(uint8_t *output_data, char clientID[], uint16_t clientID_length)
{
//  output_data[0]= clientID_length>>8;
//  output_data[1]= clientID_length& 0x00FF;
    output_data[0] = 0x00;
    output_data[1] = 0x04;
    uint8_t i = 0;
    for (i = 2; i < clientID_length + 2; i++)
    {
        output_data[i] = clientID[i - 2];
    }
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
    optionsa option_struct;
    mqttstate current_state = idle;
    ipHeader *revip = (ipHeader*) data->data;
    tcpHeader *revtcp = (tcpHeader*) revip->data;

    // Init controller
    initHw();
    initEeprom();

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
    uint8_t i = 0;
    etherGetMacAddress(tcp.source_Hw);
    etherGetIpAddress(tcp.source_Ip);
    for (i = 0; i < 4; i++)
    {
        tcp.dest_Ip[i] = ipGwAddress[i];
    }
    tcp.dest_port = 1883;
    tcp.source_port = 10000;
    uint8_t optionslength = 4;
    uint8_t options[] = { 0x02, 0x04, 0x05, 0xB4 };
    option_struct.type = 0x02;
    option_struct.length = 0x04;
    option_struct.a = 0x04C4;

    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    while (true)
    {

        // Put terminal processing here
        if (kbhitUart0())
        {
            getsUart0(&data_input);

            if (enterispressed)
            {
                enterispressed = false;
                putsUart0(data_input.buffer);

                // Parse fields
                parseFields(&data_input);

                // Echo back the parsed field information (type and fields)

                putcUart0('\n');
                putcUart0('\r');
                bool valid = false;
                if (isCommand(&data_input, "mqtt", 4))
                {
                    mqtt_ip[0] = getFieldInteger(&data_input, 1);
                    mqtt_ip[1] = getFieldInteger(&data_input, 2);
                    mqtt_ip[2] = getFieldInteger(&data_input, 3);
                    mqtt_ip[3] = getFieldInteger(&data_input, 4);
                    //              uint32_t *allmqtt;
                    //                allmqtt=mqtt_ip[0] | (mqtt_ip[1]<<4)| (mqtt_ip[2]<<8) | (mqtt_ip[3]<<12);
                    //                  writeEeprom(eeprom_address,allmqtt);
//                    putsUart0("mqtt ip set");
                    putcUart0('\n');
                    putcUart0('\r');

                    valid = true;

                }
                if (isCommand(&data_input, "connect", 0))
                {

                    current_state = sendArpReq;
                    valid = true;

                }
                if (isCommand(&data_input, "tcp", 0))
                {
                    current_state = sendTcpSyn;
                    valid = true;
                }

                valid = checkCommand(data_input);
                putcUart0('\n');
                putcUart0('\r');

            }

        }

        if (current_state == sendArpReq)
        {
            current_state = waitArpResp;
            uint8_t ip[4];
            etherGetIpGatewayAddress(ip);
            etherSendArpRequest(data, ip);
            putsUart0("send arp");

        }

        if (current_state == sendTcpSyn)
        {

            flag = 0x6002;
            data_length = 0;
            sendTCP(data, tcp, flag, sequencenum, acknum, data_length, options,
                    optionslength);
            current_state = waitTcpSynAck;
        }

        if (current_state == sendMqttConnect)
        {
            Mqttpacket *mqtt_connect = (Mqttpacket*) revtcp->data;
            char clientID[4] = "dzes";
            uint16_t clientID_length = sizeof(clientID);

            //uint32_t test = MQTTremaininglength(129);

            uint32_t Remaining_length = MQTTremaininglength(
                    sizeof(connectvariableheader) + sizeof(clientID_length)
                            + clientID_length);

            mqtt_connect->Control_Header = 0x10;   //for connect
            mqtt_connect->Packet_Remaining_length[0] = Remaining_length;

            data_length = 2 + Remaining_length;

            connectvariableheader *Mqtt_variable_connect =
                    (connectvariableheader*) (mqtt_connect->Packet_Remaining_length
                            + 1);

            uint8_t *output_data =
                    (uint8_t*) Mqtt_variable_connect->data_payload;

            Mqtt_variable_connect->protocol_level = 0x04;
            Mqtt_variable_connect->Connect_flags = htons(2); //for clean session
            Mqtt_variable_connect->keepalive_secs = htons(30);
            Mqtt_variable_connect->length = htons(0x04);
           // strcpy(Mqtt_variable_connect->message, "MQTT");
            Mqtt_variable_connect->message[0]='M';
            Mqtt_variable_connect->message[1]='Q';
            Mqtt_variable_connect->message[2]='T';
            Mqtt_variable_connect->message[3]='T';
           // uint8_t output[6];
            encodeUTF(output_data, clientID, clientID_length);

//            sequencenum = ntohl(revtcp->acknowledgementNumber);
//                         acknum = ntohl(revtcp->sequenceNumber) + 4;
            flag = 0x5000 | 0x0010 | 0x0008;      //PSH and ACK

            sendTCP(data, tcp, flag, sequencenum, acknum, data_length, 0, 0);
            current_state = idle;
        }

        if (etherIsDataAvailable())
        {

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);
            waitMicrosecond(100000);
            // Handle ARP request

            if (etherIsArpResponse(data) && current_state == waitArpResp)
            {
                uint8_t i = 0;
                uint8_t mqtt_mac[6];
                for (i = 0; i < 6; i++)
                {
                    mqtt_mac[i] = data->sourceAddress[i];
                    tcp.dest_Hw[i] = mqtt_mac[i];
                }

                //waitMicrosecond(100000);

                putcUart0('\n');
                putcUart0('\r');
            }

            waitMicrosecond(100000);

            if (etherIsIp(data) && current_state == waitTcpSynAck
                    && (ntohs(revtcp->offsetFields) & 0x012))
            {

                sequencenum = ntohl(revtcp->acknowledgementNumber);
                acknum = ntohl(revtcp->sequenceNumber) + 1;
                flag = 0x5010;
                data_length = 0;
                sendTCP(data, tcp, flag, sequencenum, acknum, data_length, 0,
                        0);
                SENDACK = 0;
                putsUart0("Done");

                waitMicrosecond(5000000);
                current_state = sendMqttConnect;
//                flag = 0x5000 | 0x0001;
//                sendTCP(data, tcp, flag, sequencenum, acknum, 0,0,0);
            }

        }
//

    }

    // Packet processing

}

