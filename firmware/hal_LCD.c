/*
 * hal_LCD.c — segment LCD driver for the MSP-EXP430FR6989 LaunchPad.
 * Register-level (no driverlib), so it builds under both CCS and msp430-gcc.
 *
 * LCD_C is clocked from ACLK, 4-mux. main.c runs ACLK off the internal VLO
 * (~9.4 kHz) — no crystal — which gives a ~70 Hz LCD frame rate (flicker-free).
 * The module only configures registers here; it never waits on a clock, so it
 * is safe to call even though no external crystal is started.
 */
#include <msp430.h>
#include "hal_LCD.h"

// LCD memory index of each digit's first byte (TI FR6989 LaunchPad map).
static const int pos[6] = { 9, 5, 3, 18, 14, 7 };

// Segment patterns for '0'..'9' (two LCDMEM bytes each) and a few letters.
static const unsigned char digit[10][2] = {
    {0xFC, 0x28}, /* 0 */
    {0x60, 0x20}, /* 1 */
    {0xDB, 0x00}, /* 2 */
    {0xF3, 0x00}, /* 3 */
    {0x67, 0x00}, /* 4 */
    {0xB7, 0x00}, /* 5 */
    {0xBF, 0x00}, /* 6 */
    {0xE4, 0x00}, /* 7 */
    {0xFF, 0x00}, /* 8 */
    {0xF7, 0x00}, /* 9 */
};

void LCD_init(void)
{
    // LCD_C clock source defaults to ACLK (= VLO from init_clocks); no crystal.

    // Enable the LCD segment/COM pins used by the on-board display.
    LCDCPCTL0 = 0xFFFF;
    LCDCPCTL1 = 0xFC3F;
    LCDCPCTL2 = 0x0FFF;

    // ACLK, divide by 1, pre-divide by 16, 4-mux, low-power waveform.
    LCDCCTL0 = LCDDIV__1 | LCDPRE__16 | LCD4MUX | LCDLP;

    // Internal charge pump for contrast; VLCD ~ 2.60 V.
    LCDCVCTL = VLCD_1 | VLCDREF_0 | LCDCPEN;
    LCDCCPCTL = LCDCPCLKSYNC;

    LCDCMEMCTL = LCDCLRM;   // clear LCD memory
    LCDCCTL0 |= LCDON;      // turn the LCD on
}

void LCD_clear(void)
{
    LCDCMEMCTL = LCDCLRM;
}

void LCD_showChar(char c, int position)
{
    int p;
    if (position < 1 || position > 6) return;
    p = pos[position - 1];

    if (c >= '0' && c <= '9') {
        LCDMEM[p]     = digit[c - '0'][0];
        LCDMEM[p + 1] = digit[c - '0'][1];
    } else if (c == '-') {
        LCDMEM[p]     = 0x03;   // middle bars only
        LCDMEM[p + 1] = 0x00;
    } else { // space / unknown -> blank
        LCDMEM[p]     = 0x00;
        LCDMEM[p + 1] = 0x00;
    }
}

// Right-aligned signed integer across positions 4,5,6.
void LCD_showInt(int value)
{
    int neg = value < 0;
    char d0, d1, d2;
    if (neg) value = -value;
    if (value > 999) value = 999;

    d0 = (char)('0' + (value % 10));
    d1 = (value >= 10) ? (char)('0' + ((value / 10) % 10)) : ' ';
    d2 = (value >= 100) ? (char)('0' + ((value / 100) % 10)) : (neg ? '-' : ' ');

    LCD_showChar(d2, 4);
    LCD_showChar(d1, 5);
    LCD_showChar(d0, 6);
}
