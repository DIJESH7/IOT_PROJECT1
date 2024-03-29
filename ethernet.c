//DIJESH PRADHAN      1001516650
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

extern uint8_t strLength;
uint16_t topic_size = 0;
uint16_t message_size = 0;
extern bool check_return;
uint32_t sequencenum = 0;
uint32_t acknum = 0;
uint16_t flag = 0;

uint16_t data_length = 0;
char topic[80];
char message[100];
uint16_t eeprom_address = 0;
char *topic1;
char *message1;
uint8_t qos = 2;
uint8_t server_ip[4];
uint8_t client_ip[4];

char subscribed_topics[3][80];

uint16_t packet_id = 12;
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
    sendpublish,
    waitPublish,
    subscribe,
    unsubscribe,
    suback,
    unsuback,
    disconnect,
    disconnectack,
    finack
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
//
//uint32_t MQTTremaininglength(uint32_t X)
//{
//    uint32_t encodedByte = 0;
//    do
//    {
//        encodedByte = X % 128;
//        X = X / 128;
//        if (X > 0)
//        {
//            encodedByte = encodedByte | 128;
//        }
//
//    }
//    while (X > 0);
//    return encodedByte;
//}
uint32_t MQTTremaininglength(uint32_t X, uint8_t bytes)
{
    uint32_t encodedByte = 0;
    uint32_t temp = 0;
    bytes = 0;

    do
    {
        encodedByte = X % 128;
        X = X / 128;
        if (X == 0 && bytes == 0)
        {
            temp = encodedByte;
        }
        if (X > 0)
        {
            bytes++;
            encodedByte = encodedByte | 128;
            encodedByte = encodedByte << 8;
            temp = encodedByte | X;

        }

    }
    while (X > 0);

    return temp;
}

//uint32_t decodeRemainingLength(uint32_t X, uint8_t bytes)
//{
//
//}
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)
#define MAX_PACKET_SIZE 1522

int main(void)

{

    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader*) buffer;
    USER_DATA data_input;
    socket tcp;

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

    uint32_t temp_ip = readEeprom(eeprom_address);
    server_ip[0] = (temp_ip & 0xff000000) >> 24;
    server_ip[1] = (temp_ip & 0x00ff0000) >> 16;
    server_ip[2] = (temp_ip & 0x0000ff00) >> 8;
    server_ip[3] = (temp_ip & 0x000000ff);

    uint32_t temp2_ip = readEeprom(eeprom_address + 1);
    client_ip[0] = (temp2_ip & 0xff000000) >> 24;
    client_ip[1] = (temp2_ip & 0x00ff0000) >> 16;
    client_ip[2] = (temp2_ip & 0x0000ff00) >> 8;
    client_ip[3] = (temp2_ip & 0x000000ff);
    etherSetIpAddress(client_ip[0], client_ip[1], client_ip[2], client_ip[3]);
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
        tcp.dest_Ip[i] = server_ip[i];
    }
    tcp.dest_port = 1883;
    tcp.source_port = 10000;
    if (server_ip[0] == 192 && server_ip[1] == 168 && server_ip[2] == 2
            && server_ip[3] == 1)
    {
        current_state = sendArpReq;
    }
    uint8_t optionslength = 4;
    uint8_t options[] = { 0x02, 0x04, 0x05, 0xB4 };

    //check_state = true;

    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    while (true)
    {
//        if (check_state == true)
//        {

        // Put terminal processing here
        if (kbhitUart0())
        {
            getsUart0(&data_input);
            if (check_return)
            {
                putsUart0(data_input.buffer);

                // Parse fields
                parseFields(&data_input);

                // Echo back the parsed field information (type and fields)

                putcUart0('\n');
                putcUart0('\r');
                bool valid = false;
                if (isCommand(&data_input, "setmqtt", 4))
                {
                    server_ip[0] = getFieldInteger(&data_input, 1);
                    server_ip[1] = getFieldInteger(&data_input, 2);
                    server_ip[2] = getFieldInteger(&data_input, 3);
                    server_ip[3] = getFieldInteger(&data_input, 4);
                    uint32_t allmqtt;
                    allmqtt = server_ip[3] | (server_ip[2] << 8)
                            | (server_ip[1] << 16) | (server_ip[0] << 24);
                    writeEeprom(eeprom_address, allmqtt);
                    putsUart0("mqtt ip set");
                    putcUart0('\n');
                    putcUart0('\r');

                    valid = true;

                }

                if (isCommand(&data_input, "setip", 4))
                {
                    client_ip[0] = getFieldInteger(&data_input, 1);
                    client_ip[1] = getFieldInteger(&data_input, 2);
                    client_ip[2] = getFieldInteger(&data_input, 3);
                    client_ip[3] = getFieldInteger(&data_input, 4);
                    uint32_t allclient;
                    allclient = client_ip[3] | (client_ip[2] << 8)
                            | (client_ip[1] << 16) | (client_ip[0] << 24);
                    writeEeprom(eeprom_address + 1, allclient);
                    etherSetIpAddress(client_ip[0], client_ip[1], client_ip[2],
                                      client_ip[3]);
                    etherGetIpAddress(tcp.source_Ip);
                    putsUart0("client ip set");
                    putcUart0('\n');
                    putcUart0('\r');

                    valid = true;

                }
                if (isCommand(&data_input, "reboot", 0))
                {
                    putsUart0("Microcontroller rebooting...\n\r");
                    NVIC_APINT_R = (0x05FA0000 | NVIC_APINT_SYSRESETREQ);
                }

                if (isCommand(&data_input, "connect", 0))
                {

                    current_state = sendArpReq;
                    valid = true;
                    putsUart0("connection started\n\r");
                    //check_state = false;

                }
                if (isCommand(&data_input, "tcp", 0))
                {
                    current_state = sendTcpSyn;
                    valid = true;
                }
                if (isCommand(&data_input, "publish", 2))
                {
                    current_state = sendpublish;
                    topic1 = getFieldString(&data_input, 1);
                    strcpy(topic, topic1);
                    topic_size = strLength;
                    message1 = getFieldString(&data_input, 2);
                    strcpy(message, message1);
                    message_size = strLength;
                    valid = true;
                    putsUart0("Message published\n\r");
                    //check_state = false;

                }

                if (isCommand(&data_input, "subscribe", 1))
                {
                    current_state = subscribe;
                    topic1 = getFieldString(&data_input, 1);
                    strcpy(topic, topic1);
                    topic_size = strLength;
                    valid = true;
                    putsUart0("Topic subscribed\n\r");
                    //check_state = false;

                }
                if (isCommand(&data_input, "unsubscribe", 1))
                {
                    current_state = unsubscribe;
                    topic1 = getFieldString(&data_input, 1);
                    strcpy(topic, topic1);
                    topic_size = strLength;
                    valid = true;
                    putsUart0("Topic unsubscribed\n\r");
                    //check_state = false;

                }

                if (isCommand(&data_input, "disconnect", 0))
                {
                    current_state = disconnect;
                    valid = true;
                    putsUart0("Disconnected Thank you\n\r");
                    //check_state = false;
                }
                if (!valid)
                {
                    putsUart0("Invalid command\n\r");

                }

                putcUart0('\n');
                putcUart0('\r');
            }

        }
        // }

        if (current_state == sendArpReq)
        {
            current_state = waitArpResp;

            etherSendArpRequest(data, server_ip);
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
        if (current_state == disconnect)
        {
            Mqttpacket *mqtt_disconnect = (Mqttpacket*) revtcp->data;

            mqtt_disconnect->Control_Header = 0xE0;

            mqtt_disconnect->Packet_Remaining_length[0] = 0;
            data_length = 2 + mqtt_disconnect->Packet_Remaining_length[0];
            flag = 0x5000 | 0x0010 | 0x0008|0x0001;      //PSH and ACK

            sendTCP(data, tcp, flag, sequencenum, acknum, data_length, 0, 0);
            current_state = finack;
        }

        if (current_state == unsubscribe)
        {
            Mqttpacket *mqtt_unsubscribe = (Mqttpacket*) revtcp->data;
            mqtt_unsubscribe->Control_Header = 0xA2;
            uint8_t bytes = 0;
            uint8_t unsubscribetopic_length = topic_size + sizeof(uint16_t);
            uint32_t Remaining_length = MQTTremaininglength(
                    unsubscribetopic_length + 2, bytes);
            data_length = 2 + Remaining_length;
            mqtt_unsubscribe->Packet_Remaining_length[0] = Remaining_length;
            //packet_id
            mqtt_unsubscribe->Packet_Remaining_length[1] = 0x00;
            mqtt_unsubscribe->Packet_Remaining_length[2] = 0x04;
            mqtt_unsubscribe->Packet_Remaining_length[3] = topic_size >> 8;

            mqtt_unsubscribe->Packet_Remaining_length[4] = topic_size & 0x00FF;

            uint8_t i = 0;

            for (i = 0; i < topic_size; i++)
            {
                mqtt_unsubscribe->Packet_Remaining_length[5 + i] = topic[i];
            }

            flag = 0x5000 | 0x0010 | 0x0008;      //PSH and ACK

            sendTCP(data, tcp, flag, sequencenum, acknum, data_length, 0, 0);
            current_state = unsuback;

        }

        if (current_state == subscribe)
        {
            Mqttpacket *mqtt_subscribe = (Mqttpacket*) revtcp->data;
            mqtt_subscribe->Control_Header = 0x82;
            uint8_t bytes = 0;
            uint8_t subscribetopic_length = topic_size + sizeof(uint16_t);
            uint32_t Remaining_length = MQTTremaininglength(
                    subscribetopic_length + 3, bytes);
            data_length = 2 + Remaining_length;

            mqtt_subscribe->Packet_Remaining_length[0] = Remaining_length;
            if (bytes > 0)
            {
                mqtt_subscribe->Packet_Remaining_length[1] = Remaining_length
                        & 0xFF00;
            }
            //packet_id
            mqtt_subscribe->Packet_Remaining_length[1 + bytes] = 0x00;
            mqtt_subscribe->Packet_Remaining_length[2 + bytes] = 0x04;

//            //
//            mqtt_subscribe->Packet_Remaining_length[3] = 0x00;
//            mqtt_subscribe->Packet_Remaining_length[4] = 0x04;
            mqtt_subscribe->Packet_Remaining_length[3 + bytes] = topic_size
                    >> 8;

            mqtt_subscribe->Packet_Remaining_length[4 + bytes] = topic_size
                    & 0x00FF;

            uint8_t i = 0;
            uint8_t numtopics = 0;

            for (i = 0; i < topic_size; i++)
            {
                mqtt_subscribe->Packet_Remaining_length[5 + i + bytes] =
                        topic[i];
                subscribed_topics[numtopics][i] = topic[i];
                numtopics++;
            }

//            mqtt_subscribe->Packet_Remaining_length[5] = 'd';
//            mqtt_subscribe->Packet_Remaining_length[6] = 'z';
//            mqtt_subscribe->Packet_Remaining_length[7] = 'e';
//            mqtt_subscribe->Packet_Remaining_length[8] = 's';
            mqtt_subscribe->Packet_Remaining_length[5 + topic_size + bytes] = 2;

            current_state = suback;

            flag = 0x5000 | 0x0010 | 0x0008;      //PSH and ACK

            sendTCP(data, tcp, flag, sequencenum, acknum, data_length, 0, 0);

        }

        if (current_state == sendpublish)
        {

            Mqttpacket *mqtt_publish = (Mqttpacket*) revtcp->data;

            uint8_t publishvariableheader_length = topic_size
                    + sizeof(uint16_t);
            uint8_t bytes = 0;

            uint32_t Remaining_length = MQTTremaininglength(
                    publishvariableheader_length + sizeof(uint16_t)
                            + message_size,
                    bytes);

            mqtt_publish->Control_Header = 0x30 | qos;
            mqtt_publish->Packet_Remaining_length[0] = Remaining_length & 0xFF;

            if (bytes > 0)
            {
                mqtt_publish->Packet_Remaining_length[1] = Remaining_length
                        & 0xFF00;
            }
            data_length = 2 + Remaining_length;

            mqtt_publish->Packet_Remaining_length[1 + bytes] = topic_size >> 8;
            mqtt_publish->Packet_Remaining_length[2 + bytes] = topic_size
                    & 0x00FF;

            uint8_t i = 0;

            for (i = 0; i < topic_size; i++)
            {
                mqtt_publish->Packet_Remaining_length[3 + i + bytes] = topic[i];
            }
            uint8_t qos_length = 0;

            if (packet_id > 0)
            {
                mqtt_publish->Packet_Remaining_length[3 + topic_size + bytes] =
                        0x00;
                mqtt_publish->Packet_Remaining_length[4 + topic_size + bytes] =
                        0x08;
                qos_length = 2;
            }
//
//            mqtt_publish->Packet_Remaining_length[3 + topic_size + bytes] =
//                    message_size >> 8;
//            mqtt_publish->Packet_Remaining_length[4 + topic_size + bytes] =
//                    message_size & 0x00FF;

//
//
            for (i = 0; i < message_size; i++)
            {
                mqtt_publish->Packet_Remaining_length[3 + topic_size + i
                        + qos_length + bytes] = message[i];
            }
//            for(i=0;i<3;i++)
//            {
//                if (strcmp(subscribed_topics[i],topic))
//                {
//                    putsUart0("dijeshshdh");
//                }
//            }

            flag = 0x5000 | 0x0010 | 0x0008;      //PSH and ACK
            current_state = waitPublish;
            sendTCP(data, tcp, flag, sequencenum, acknum, data_length, 0, 0);

            //check_state = true;

        }

        if (current_state == sendMqttConnect)
        {
            Mqttpacket *mqtt_connect = (Mqttpacket*) revtcp->data;
            char clientID[4] = "dzes";
            uint16_t clientID_length = sizeof(clientID);
            uint8_t bytes = 0;
            uint8_t connect_variable_length = 10;
            uint32_t Remaining_length = MQTTremaininglength(
                    connect_variable_length + sizeof(clientID_length)
                            + clientID_length,
                    bytes);

            mqtt_connect->Control_Header = 0x10;   //for connect

            //Remaining Length
            mqtt_connect->Packet_Remaining_length[0] = Remaining_length;

            //Start of variable header connect
            mqtt_connect->Packet_Remaining_length[1] = 0x00;
            mqtt_connect->Packet_Remaining_length[2] = 0x04;

            //Protocol Name MQTT
            mqtt_connect->Packet_Remaining_length[3] = 77;   //M
            mqtt_connect->Packet_Remaining_length[4] = 81;   //Q
            mqtt_connect->Packet_Remaining_length[5] = 84;   //T
            mqtt_connect->Packet_Remaining_length[6] = 84;   //T

            //Protocol level
            mqtt_connect->Packet_Remaining_length[7] = 0x04;

            //connect flags
            mqtt_connect->Packet_Remaining_length[8] = htons(2);

            //Keep Alive for 128 seconds
            mqtt_connect->Packet_Remaining_length[9] = 0x00;
            mqtt_connect->Packet_Remaining_length[10] = 0x5A;

            //ClientIDlength
            mqtt_connect->Packet_Remaining_length[11] = 0x00;
            mqtt_connect->Packet_Remaining_length[12] = 0x04;

            //ClientID
            mqtt_connect->Packet_Remaining_length[13] = 'd';
            mqtt_connect->Packet_Remaining_length[14] = 'z';
            mqtt_connect->Packet_Remaining_length[15] = 'e';
            mqtt_connect->Packet_Remaining_length[16] = 's';

            data_length = 2 + Remaining_length;

            flag = 0x5000 | 0x0010 | 0x0008;      //PSH and ACK

            sendTCP(data, tcp, flag, sequencenum, acknum, data_length, 0, 0);
            current_state = waitMqttResp;
            // check_state = true;
        }

        // Packet processing
        if (etherIsDataAvailable())
        {

            // Get packet
            etherGetPacket(data, MAX_PACKET_SIZE);
            waitMicrosecond(100000);
            // Handle ARP request
//            if (etherIsIp(data)&& current_state==waitPublish)
//            {
//
//
//
//
//            }

            if (etherIsArpResponse(data) && current_state == waitArpResp)
            {
                uint8_t i = 0;
                uint8_t mqtt_mac[6];
                for (i = 0; i < 6; i++)
                {
                    mqtt_mac[i] = data->sourceAddress[i];
                    tcp.dest_Hw[i] = mqtt_mac[i];
                }

                putcUart0('\n');
                putcUart0('\r');
                current_state = sendTcpSyn;
            }


                if (current_state==finack)
                {
                    sequencenum = ntohl(revtcp->acknowledgementNumber);
                    acknum = ntohl(revtcp->sequenceNumber) + 1;
                    flag=0x5010;
                    data_length=0;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                                              0, 0);
                    acknum = 0;
                                    sequencenum = 0;
                    current_state=idle;
                }
                if (current_state == waitTcpSynAck)
                {

                    sequencenum = ntohl(revtcp->acknowledgementNumber);
                    acknum = ntohl(revtcp->sequenceNumber) + 1;
                    flag = 0x5010;
                    data_length = 0;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                            0, 0);

                    putsUart0("Done");

                    current_state = sendMqttConnect;

                }

                if (current_state == waitMqttResp)
                {
//                    uint16_t rev_data_length=0;
//                    uint8_t rev_tcpHeaderLength =(htons(revtcp->offsetFields&0xF0)>>4)*4;
//                    rev_data_length=(htons(revip->length) - rev_tcpHeaderLength - sizeof(ipHeader));


                    sequencenum = ntohl(revtcp->acknowledgementNumber);
                    acknum = ntohl(revtcp->sequenceNumber) + 4;
                    data_length = 0;
                    flag = 0x5010;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                            0, 0);
                    setPinValue(BLUE_LED, 1);
                    putsUart0("Connection established\n\r");

                    current_state = idle;
                }

                if (current_state == waitPublish)
                {

                    //                                }

                    sequencenum = ntohl(revtcp->acknowledgementNumber);
                    acknum = ntohl(revtcp->sequenceNumber) + 4;
                    data_length = 0;
                    flag = 0x5010;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                            0, 0);
                    current_state = idle;

                    // check_state = true;
                }

                if (current_state == suback)
                {
                    sequencenum = ntohl(revtcp->acknowledgementNumber);
                    acknum = ntohl(revtcp->sequenceNumber) + 5;
                    data_length = 0;
                    flag = 0x5010;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                            0, 0);
                    current_state = idle;
                    //check_state = true;

                }

                if (current_state == unsuback)
                {
                    sequencenum = ntohl(revtcp->acknowledgementNumber);
                    acknum = ntohl(revtcp->sequenceNumber) + 4;
                    data_length = 0;
                    flag = 0x5010;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                            0, 0);
                    current_state = idle;
                    //check_state = true;

                }
                if (current_state == disconnectack)
                {
                    sequencenum = ntohl(revtcp->acknowledgementNumber);
                    acknum = ntohl(revtcp->sequenceNumber) + 1;
                    data_length = 0;
                    flag = 0x5010;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                            0, 0);
                    acknum = 0;
                    sequencenum = 0;
                    flag = 0x5011;
                    sendTCP(data, tcp, flag, sequencenum, acknum, data_length,
                                            0, 0);

                    setPinValue(BLUE_LED, 0);
                    current_state = idle;
                    //check_state = true;

                }
            }

        }

    }

}

