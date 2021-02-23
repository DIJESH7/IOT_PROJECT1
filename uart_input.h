/*
 * uart_input.h
 *
 *  Created on: Oct 29, 2020
 *      Author: dzespradhan
 */

#ifndef UART_INPUT_H_
#define UART_INPUT_H_


#define MAX_CHARS 80
#define MAX_FIELDS 10
typedef struct _USER_DATA
{
 char buffer[MAX_CHARS+1];
 uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;


//bool checkCommand(USER_DATA data);

bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber);
char* parseFields(USER_DATA* data);
char* getsUart0(USER_DATA* data);

#endif /* UART_INPUT_H_ */
