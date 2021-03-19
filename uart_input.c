/*
 * uart_input.c
 *
 *  Created on: Oct 29, 2020
 *      Author: dzespradhan
 */


//-----------------------------------------------------------------------------
// isCommand
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "uart_input.h"

bool enterispressed=false;

bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{

//char *str=NULL;
//str=getFieldString(&data,1);

    int i=data->fieldPosition[0];
    char check[MAX_CHARS];
    int loop=1;
    int store=0;
    while(loop)
    {

      char value= data->buffer[i];
      check[store]=value;
      if (data->buffer[i]=='\0')
      {
       loop=0;
      }
     i++;
     store++;
    }


    int comp=strcmp(check,strCommand);
    if (comp==0&&data->fieldCount==minArguments+1)
    {
        return true;
    }
    return false;



}


//-----------------------------------------------------------------------------
// getFieldString
//-----------------------------------------------------------------------------
char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{

    if (fieldNumber>data->fieldCount)
      {
          return NULL;

      }

    int i=data->fieldPosition[fieldNumber];

    char check[MAX_CHARS];
    int loop=1;
    int store=0;
   while(loop)
    {

      char value= data->buffer[i];
      check[store]=value;


        if (data->buffer[i]=='\0')
                       {
                   return check;
                       }
        i++;
        store++;


    }


}
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{


    if (fieldNumber>data->fieldCount)
          {
              return NULL;

          }

        int i=data->fieldPosition[fieldNumber];

        char check[MAX_CHARS];
    int loop=1;
    int store=0;
    int answer=0;
       while(loop)
        {

          char value= data->buffer[i];
          check[store]=value;


            if (data->buffer[i]=='\0')
                           {
                answer=atoi(check);
                       return answer;
                           }
            i++;
            store++;


        }

}
//-----------------------------------------------------------------------------
// parseFields
//-----------------------------------------------------------------------------


char* parseFields(USER_DATA* data)
{
    int check=0;
    int count=0;
    int fieldcount=0;
    while (1)
    {
        //for set 1 2
        if (count==0&&!((data->buffer[count]>=32&&data->buffer[count]<=47)||(data->buffer[count]>=58&&data->buffer[count]<=65)||(data->buffer[count]>=123&&data->buffer[count]<=126)))
                {

            data->fieldPosition[fieldcount]=count;

                    if (data->buffer[count]>=48 && data->buffer[count]<=57)
                           {
                               data->fieldType[fieldcount]= 'n';

                               fieldcount++;
                           }
                           if (data->buffer[count]>=65&& data->buffer[count]<=90)
                           {
                               data->fieldType[fieldcount]= 'a';
                               fieldcount++;
                           }
                           if (data->buffer[count]>=97 && data->buffer[count]<=122)
                           {
                               data->fieldType[fieldcount]= 'a';
                               fieldcount++;
                           }
                           count++;
                }

        //for one delimeter
        if ((data->buffer[count]>=32&&data->buffer[count]<=47)||(data->buffer[count]>=58&&data->buffer[count]<=65)||(data->buffer[count]>=123&&data->buffer[count]<=126))
        {
            data->buffer[count]='\0';


            count++;
          while((data->buffer[count]>=32&&data->buffer[count]<=47)||(data->buffer[count]>=58&&data->buffer[count]<=65)||(data->buffer[count]>=123&&data->buffer[count]<=126))
          {
              count++;
              if (!((data->buffer[count]>=32&&data->buffer[count]<=47)||(data->buffer[count]>=58&&data->buffer[count]<=65)||(data->buffer[count]>=123&&data->buffer[count]<=126)))
              {
                  break;
              }
          }
            data->fieldPosition[fieldcount]=count;
            if (data->buffer[count]>=48 && data->buffer[count]<=57)
                   {
                       data->fieldType[fieldcount]= 'n';
                       fieldcount++;
                   }
                   if (data->buffer[count]>=65&& data->buffer[count]<=90)
                   {
                       data->fieldType[fieldcount]= 'a';
                       fieldcount++;
                   }
                   if (data->buffer[count]>=97 && data->buffer[count]<=122)
                   {
                       data->fieldType[fieldcount]= 'a';
                       fieldcount++;
                   }


                   //data->buffer[count]='\0';

                 /*  if (data->buffer[count]=='\0')
                      {
                          data->buffer[count]=0;
                          return data;
                      }*/



        }
count++;
        if (fieldcount==MAX_FIELDS)
                           {
                                     data->fieldType[fieldcount]=0;
                                     data->fieldCount=fieldcount;
                                     return data;
                           }
        if (data->buffer[count]=='\0')
                              {
                                  data->fieldType[fieldcount]=0;
                                  data->fieldCount=fieldcount;
                                  return data;
                              }


    }
}


//-----------------------------------------------------------------------------
// getsUart0
//-----------------------------------------------------------------------------


char* getsUart0(USER_DATA* data)
{
    int count=0;
    while(1)
    {
    char c = getcUart0();

    //for backspace entered
    if (c==8||c==127)
    {
        if (count>0)
        {
            count--;
        }
        continue;
    }

    //for return pressed
    if (c==13)
    {
        data->buffer[count]=0;
        enterispressed=true;
        return data;
    }

    //if character is anything greater than a space
    if (c>=32)
    {
        data->buffer[count]=c;
        count++;
        if (count==MAX_CHARS)
        {
            data->buffer[count]==0;
            enterispressed=true;
            return data;
        }
    }

    }

}
