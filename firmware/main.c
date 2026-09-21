/*
 * main.c — MSP430FR6989 telemetry node (MSP-EXP430FR6989 LaunchPad)
 *
 * Every 500 ms it:
 *   1. samples the internal temperature sensor (ADC12_B),
 *   2. shows the rounded value on the on-board segment LCD,
 *   3. streams one framed reading over the USB backchannel UART.
 *
 * Between samples the CPU sleeps in LPM3 (only the 32 kHz crystal runs),
 * which is the low-power behaviour instrumentation employers look for.
 *
 * Frame format (see ../PROTOCOL.md):  $<seq>,TEMP,<value>*<CS>\r\n
 *
 * Toolchain: TI Code Composer Studio, or msp430-gcc + mspdebug.
 * Backchannel UART is eUSCI_A1 on P3.4 (TXD) / P3.5 (RXD), 9600 8N1.
 */
#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hal_LCD.h"

// --- Temperature-sensor factory calibration (TLV), 1.2 V reference ---
#define CALADC_15V_30C  (*((uint16_t *)0x1A1A))  // ADC @ 30 °C
#define CALADC_15V_85C  (*((uint16_t *)0x1A1C))  // ADC @ 85 °C

static volatile uint8_t sample_flag = 0;
static uint16_t seq = 0;

// ---------------------------------------------------------------- clocks
static void init_clocks(void)
{
    // Start the 32.768 kHz crystal (LFXT) on the LaunchPad for ACLK.
    PJSEL0 |= BIT4 | BIT5;                 // PJ.4/PJ.5 = LFXIN/LFXOUT
    CSCTL0_H = CSKEY_H;                     // unlock CS registers
    CSCTL1 = DCOFSEL_0;                     // DCO ~1 MHz
    CSCTL2 = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;
    CSCTL4 &= ~LFXTOFF;
    do {                                    // wait for the crystal to settle
        CSCTL5 &= ~LFXTOFFG;
        SFRIFG1 &= ~OFIFG;
    } while (SFRIFG1 & OFIFG);
    CSCTL0_H = 0;                           // relock CS registers
}

// ---------------------------------------------------------------- UART A1
static void init_uart(void)
{
    P3SEL0 |= BIT4 | BIT5;                  // P3.4/P3.5 -> UCA1TXD/UCA1RXD
    P3SEL1 &= ~(BIT4 | BIT5);

    UCA1CTLW0 = UCSWRST;                    // hold in reset while configuring
    UCA1CTLW0 |= UCSSEL__SMCLK;            // SMCLK = ~1 MHz
    // 9600 baud @ 1 MHz, oversampling: BR=6, BRF=8, BRS=0x20, UCOS16=1
    UCA1BR0 = 6;
    UCA1BR1 = 0;
    UCA1MCTLW = 0x2000 | (8 << 4) | UCOS16;
    UCA1CTLW0 &= ~UCSWRST;                  // release for operation
}

static void uart_putc(char c)
{
    while (!(UCA1IFG & UCTXIFG));
    UCA1TXBUF = c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

// ---------------------------------------------------------------- ADC temp
static void init_adc(void)
{
    // 1.2 V internal reference on for the temperature sensor.
    while (REFCTL0 & REFGENBUSY);
    REFCTL0 = REFVSEL_0 | REFON;

    ADC12CTL0 = ADC12SHT0_8 | ADC12ON;      // long sample time, ADC on
    ADC12CTL1 = ADC12SHP;                   // pulse-mode sampling
    ADC12CTL2 = ADC12RES_2;                 // 12-bit resolution
    ADC12CTL3 = ADC12TCMAP;                 // map temp sensor to channel A30
    ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_30; // VR+ = buffered VREF, temp channel

    __delay_cycles(400);                    // let the reference stabilise
}

static uint16_t adc_read_raw(void)
{
    ADC12CTL0 |= ADC12ENC | ADC12SC;        // start conversion
    while (!(ADC12IFGR0 & ADC12IFG0));      // wait for completion
    return ADC12MEM0;
}

// Convert the raw temp-sensor code to °C using the factory calibration.
static int read_temp_c_x10(void)
{
    int32_t raw = adc_read_raw();
    // degC = (raw - cal30) * (85 - 30) / (cal85 - cal30) + 30
    int32_t num = (raw - (int32_t)CALADC_15V_30C) * (85 - 30);
    int32_t den = (int32_t)CALADC_15V_85C - (int32_t)CALADC_15V_30C;
    int32_t c_x10 = (num * 10) / den + 300; // tenths of a degree
    return (int)c_x10;
}

// ---------------------------------------------------------------- timer
static void init_timer_500ms(void)
{
    // ACLK = 32768 Hz, /8 = 4096 Hz. 2048 counts = 500 ms, up mode.
    TA0CCR0 = 2048 - 1;
    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL__ACLK | ID__8 | MC__UP | TACLR;
}

// ---------------------------------------------------------------- framing
// Build "$<seq>,TEMP,<value>*<CS>\r\n" and send it.
static void send_frame(int temp_c_x10)
{
    char payload[24];
    char line[40];
    int ip = temp_c_x10 / 10;
    int fp = temp_c_x10 % 10;
    if (fp < 0) fp = -fp;

    // payload = "<seq>,TEMP,<int>.<tenth>"
    sprintf(payload, "%u,TEMP,%d.%d", (unsigned)seq, ip, fp);

    // XOR checksum over the payload bytes.
    unsigned char cs = 0;
    for (const char *p = payload; *p; ++p) cs ^= (unsigned char)*p;

    sprintf(line, "$%s*%02X\r\n", payload, cs);
    uart_puts(line);
    seq++;
}

// ---------------------------------------------------------------- main
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // stop watchdog
    PM5CTL0 &= ~LOCKLPM5;                   // unlock GPIO after FRAM boot

    init_clocks();
    init_uart();
    init_adc();
    LCD_init();
    LCD_clear();
    init_timer_500ms();

    __bis_SR_register(GIE);                 // enable interrupts

    for (;;) {
        __bis_SR_register(LPM3_bits | GIE); // sleep until the 500 ms tick
        if (sample_flag) {
            sample_flag = 0;
            int t_x10 = read_temp_c_x10();
            LCD_showInt((t_x10 + 5) / 10);  // rounded whole degrees
            send_frame(t_x10);
        }
    }
}

// ---------------------------------------------------------------- ISR
#pragma vector = TIMER0_A0_VECTOR
__interrupt void timer0_a0_isr(void)
{
    sample_flag = 1;
    __bic_SR_register_on_exit(LPM3_bits);   // wake main
}
