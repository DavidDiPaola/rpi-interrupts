#include "biguart.h"
#include "io.h"
#include "gpio.h"
#include "interrupt.h" //DEBUG?

void uartInit (void)
{
    //set GPIO pin 14 to PL011 UART TX mode
    GPIOMODE(14, FSEL_AF0);
    //set GPIO pin 15 to PL011 UART RX mode
    GPIOMODE(15, FSEL_AF0);
    //turn off pull-up/pull-down resistors on the TX and RX pins
    GPIOPULLOF(14);
    GPIOPULLOF(15);

    //we must disable the PL011 before we configure it
    PL011_CR = 0;
    //wait for the UART to stop sending or receiving
    while(PL011_FR & (1<<PL011_FR_BUSY)){}
    //flush the transmit FIFO
    PL011_LCRH &= ~(1<<PL011_LCRH_FEN);
    //clear any pending interrupts
    PL011_ICR = ((1<<PL011_ICR_OEIC) |
                 (1<<PL011_ICR_BEIC) |
                 (1<<PL011_ICR_PEIC) |
                 (1<<PL011_ICR_FEIC) |
                 (1<<PL011_ICR_RTIC) |
                 (1<<PL011_ICR_TXIC) |
                 (1<<PL011_ICR_RXIC) |
                 (1<<PL011_ICR_DSRMIC) |
                 (1<<PL011_ICR_DCDMIC) |
                 (1<<PL011_ICR_CTSMIC) |
                 (1<<PL011_ICR_RIMIC));
    //use 115200 baud
    PL011_IBRD = PL011_BAUD_INT(115200);
    PL011_FBRD = PL011_BAUD_FRAC(115200);
    //8bit symbols, enable FIFOs
    PL011_LCRH = ((PL011_LCRH_WLEN_8BIT<<PL011_LCRH_WLEN) | (1<<PL011_LCRH_FEN));
    //enable RX, TX, UART
    PL011_CR = ((1<<PL011_CR_RXE) | (1<<PL011_CR_TXE) | (1<<PL011_CR_UARTEN));
}

char uartGetc(void)
{
    char ch = 0;

    //wait for a character to come in
    while((PL011_FR & 0x10) != 0){}
    //get char
    ch = (PL011_DR & 0xFF);

    return ch;
}

void uartPutc( char ch )
{
    //wait for transmission to finish
    while((PL011_FR & 0x20) != 0){}
    //send char
    PL011_DR = ch;
}

void uartPuts( char *s )
{
    while(*s){
        if( *s != '\n' ){
            uartPutc(*s);
        } else {
            uartPutc('\r');
            uartPutc('\n');
        }
        s++;
    }
}

void uartPutln( char *s )
{
    uartPuts(s);
    uartPuts("\n");
}

//DEBUG=============================================================================
void uartPut4(unsigned int n){
    n &= 0xF;

    switch(n){
        case 0x0:
            uartPutc('0');
            break;
        case 0x1:
            uartPutc('1');
            break;
        case 0x2:
            uartPutc('2');
            break;
        case 0x3:
            uartPutc('3');
            break;
        case 0x4:
            uartPutc('4');
            break;
        case 0x5:
            uartPutc('5');
            break;
        case 0x6:
            uartPutc('6');
            break;
        case 0x7:
            uartPutc('7');
            break;
        case 0x8:
            uartPutc('8');
            break;
        case 0x9:
            uartPutc('9');
            break;
        case 0xA:
            uartPutc('A');
            break;
        case 0xB:
            uartPutc('B');
            break;
        case 0xC:
            uartPutc('C');
            break;
        case 0xD:
            uartPutc('D');
            break;
        case 0xE:
            uartPutc('E');
            break;
        case 0xF:
            uartPutc('F');
            break;
    }
}
void uartPut32(unsigned int q){
    int i;

    uartPuts("0x");
    for(i=0; i<8; i++){
        uartPut4(q >> ((7-i)*4));
    }
    uartPuts("\n");
}

#define UART_MAXLEN 64
static volatile char uart_buffer[UART_MAXLEN];
static volatile int  uart_idx;
static volatile int  uart_busy;
//static volatile int  uart_done;

void uart_handler ( void )
{
    //ACK TX FIFO interrupt
    PL011_ICR |= (1<<PL011_ICR_TXIC);

    //if nothing is modifying the buffer or index
    if(!uart_busy){
        //if we can...
        if((uart_idx < UART_MAXLEN) && (uart_buffer[uart_idx] != '\0'))
        {
            //uart_done = 0;
            //send another character
            PL011_DR = uart_buffer[uart_idx];
            uart_idx++;
        }
        else
        {
            uart_buffer[0] = '\0';
            uart_idx = 0;
            //uart_done = 1;
        }
    }
}

void iuartInit( void ){
    uart_buffer[0] = '\0';
    uart_busy = 1;
    //uart_done = 1;
    //set RX FIFO interrupt when it becomes 7/8 full, TX FIFO interrupt when 1/8 full
    PL011_IFLS = ((PL011_IFLS_RXIFLSEL_SEVENEIGHTHS<<PL011_IFLS_RXIFLSEL) |
                  (PL011_IFLS_TXIFLSEL_EIGHTH<<PL011_IFLS_TXIFLSEL));
    //enable FIFO interrupts
    PL011_IMSC |= ((1<<PL011_IMSC_TXIM) | (1<<PL011_IMSC_RXIM));
    //enable the PL011 interrupt
    INTERRUPT_ENABLEIRQ2 |= (1<<(IRQUART-32));
}

void iuartPuts(char *s)
{
    int i = 0;

    //wait until transmission is over
    while(uart_buffer[0] != '\0'){}
    //take the uart
    uart_busy = 1;
    //if our string is zero characters, the ISR will start at index 1 but won't see a \0, so put one in manually
    uart_buffer[1] = '\0';

    //copy the string
    while((i < UART_MAXLEN) && (s[i] != '\0'))
    {
        uart_buffer[i] = s[i];
        i++;
    }
    if(i >= UART_MAXLEN){
        i = UART_MAXLEN-1;
    }
    uart_buffer[i] = '\0';

    //initialize the index
    uart_idx = 1;

    //leave the uart
    uart_busy = 0;
    //start the transmission
    PL011_DR = uart_buffer[0];
}

void iuartPutln(char *s)
{
    iuartPuts(s);
    iuartPuts("\r\n");
}

/*
Copyright (c) 2012 David Welch dwelch@dwelch.com, 2013 David DiPaola

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
