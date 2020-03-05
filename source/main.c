/*	Author: vshuk003
 *  Partner(s) Name: 
 *	Lab Section:
 *	Assignment: Lab #  Exercise #
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */
#include <avr/io.h>
#include <stdio.h> 
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h> 
#include <time.h> 
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif



// GLOBAL DEFAULTS
#define REFLEX_SIZE 10

volatile unsigned char TimerFlag = 0;
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;
unsigned char highscore = 0x00;

void TimerOn() {
	TCCR1B = 0X0B;
	OCR1A = 125;
	TIMSK1 = 0X02;
	TCNT1 = 0;

	_avr_timer_cntcurr = _avr_timer_M;

	SREG |= 0x80;
}


void TimerOff() {
	TCCR1B = 0X00;
}

void TimerISR() {
	TimerFlag = 1;
}

ISR(TIMER1_COMPA_vect) {
	_avr_timer_cntcurr--;

	if (_avr_timer_cntcurr == 0) {
		TimerISR();
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

void set_PWM(double f) {
    static double current_f;
    if (f != current_f) {
        if(!f) TCCR3B &= 0x08;
        else TCCR3B |= 0x03;
        
        if(f < 0.954) OCR3A = 0xFFFF;
        else if (f > 31250) OCR3A = 0x0000;
        else OCR3A = (short)(8000000 / (128 * f)) - 1;

        TCNT3 = 0;
        current_f = f;
    }
}
void PWM_on() {
    TCCR3A = (1 << COM3A0);
    TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);

    set_PWM(0);
}

void PWM_off() {
    TCCR3A = 0x00;
    TCCR3B = 0x00;
}



enum GlobalSM {MainMenu, ReflexGame, FollowPattern} GameState;

enum MainMenuSM {mainmenu_start, mainmenu_select, mainmenu_reflex, mainmenu_pattern, mainmenu_hold} mainmenu_state;

void MainMenuTick() {
    switch(mainmenu_state) {
        case mainmenu_start:
            PORTB = 0x00;
            char greeting[20];
            sprintf(greeting,"Main Menu, Score: %d",highscore);
            LCD_DisplayString(1,greeting);
            mainmenu_state = mainmenu_select;
            break;
        case mainmenu_select:
            if(((~PINA & 0x0F) == 0x01)) {
                mainmenu_state = mainmenu_hold;
            }
            break;
        case mainmenu_hold:
            if((~PINA & 0x0F) == 0x00) {
                mainmenu_state = mainmenu_start;
                GameState = ReflexGame;// change gamemode
            }
    }
}

unsigned char reflex_pattern[REFLEX_SIZE];
unsigned char current_reflex_pos;
unsigned char reflex_temp;


// RNG FROM SCRATH
void ReflexSetup(unsigned char reflex_pattern[]) {
    for(unsigned int i = 0; i < REFLEX_SIZE; ++i) {
        reflex_pattern[i] = (int)get_rand() % 3;
    }
}

unsigned short lfsr = 0xACE1u;
int bit;

  int get_rand() {
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    return lfsr =  (lfsr >> 1) | (bit << 15);
}

enum ReflexSM {reflex_start, reflex_on, reflex_off} reflex_state;
void ReflexTick() {
    switch(reflex_state) { //transitions
        case reflex_start:
            ReflexSetup(reflex_pattern); 
            reflex_state = reflex_on;
            break;
        case reflex_on:
            reflex_state = reflex_off;
            break;
        case reflex_off:
            reflex_state = reflex_on;
            break;
        default:
            reflex_state = reflex_on;
            break;
    }

    switch(reflex_state) { //actions
        case reflex_start:
        // CALL THIS WHEN USER HITS OPTION IN MENU
            break;
        case reflex_on:
            PORTB = (0x01 << reflex_pattern[current_reflex_pos]);
            current_reflex_pos = (current_reflex_pos + 1) % REFLEX_SIZE;
            break;
        case reflex_off:
            PORTB = 0x00;
        default:
            break;
    }
}

enum ReflexLCD {reflexlcd_s0 } reflexlcd_state;

void ReflexLCDTick() {
    char score[5];
    if ((~PINA & 0x07) == ((0x01 << reflex_pattern[(current_reflex_pos - 1) % REFLEX_SIZE ]))) {
        highscore++;
        set_PWM(261.23);
    } else if ((~PINA & 0x07) != 0x00) {
        highscore--;
        set_PWM(100.00);
    } else {
        set_PWM(0);
    }
    sprintf(score,"%d",highscore);
    LCD_DisplayString(1,score);
}

int main(void) {
    DDRA = 0x00; PORTA = 0xFF;
    DDRB = 0xFF; PORTB = 0x00;
    DDRC = 0xFF; PORTC = 0x00;
    DDRD = 0xFF; PORTD = 0x00;
    srand(time(0));
    
    
    const unsigned long timerPeriod = 3;
    TimerOn();
    TimerSet(timerPeriod);

    GameState = MainMenu;

    reflex_state = reflex_start;
    unsigned long Reflex_elapsedTime = 0;
    unsigned long Reflex_timer = 500;

    PWM_on();
	LCD_init();
    while (1) {
        switch(GameState) { //actions
            case MainMenu:
                MainMenuTick();
                break;
            case ReflexGame:
                if(Reflex_elapsedTime >= Reflex_timer) {
                    ReflexTick();
                    ReflexLCDTick();
                    Reflex_elapsedTime = 0;
                }
                if((~PINA & 0x04) || Reflex_timer < 200 ) { //reset
                    reflex_state = reflex_start;
                    GameState = MainMenu;
                    mainmenu_state = mainmenu_start;
                    Reflex_elapsedTime = 0;
                    Reflex_timer = 500;
                }
                Reflex_elapsedTime+=timerPeriod;
                break;
            case FollowPattern: //TODO
                break;
        }
        while (!TimerFlag) {}
	    TimerFlag = 0;
    }
    return 1;
}
