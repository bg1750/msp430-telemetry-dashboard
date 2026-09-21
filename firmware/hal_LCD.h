/*
 * hal_LCD.h — minimal driver for the on-board segment LCD (FH-1138P)
 * of the MSP-EXP430FR6989 LaunchPad.
 *
 * Segment maps and digit positions follow TI's LaunchPad HAL conventions
 * for this exact board. If your characters look scrambled, your board
 * revision's segment map differs — drop in TI's hal_LCD.c from the
 * MSP-EXP430FR6989 examples and this API still applies.
 */
#ifndef HAL_LCD_H
#define HAL_LCD_H

void LCD_init(void);
void LCD_clear(void);

// Show a single character at LCD position 1..6 (1 = leftmost).
void LCD_showChar(char c, int position);

// Show a signed integer right-aligned across positions 4..6 (e.g. temp in °C).
void LCD_showInt(int value);

#endif // HAL_LCD_H
