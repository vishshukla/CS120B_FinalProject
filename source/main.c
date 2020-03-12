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
#define PATTERN_SIZE 6
#define POLY_MASK_32 0xB4BCD35C
#define POLY_MASK_31 0x7A5BC2E3

volatile unsigned char TimerFlag = 0;
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;
unsigned long reflex_highscore = 500;
unsigned long pattern_highscore = 0;
unsigned long Reflex_timer = 500;

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


//////////////////// JOYSTICK ////////////////////

unsigned short ADCValue;
unsigned const short joyStickUpValue = 700;
unsigned const short joyStickDownValue = 300;

void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}

bool joyStickUp() {
    ADCValue = ADC;
    return (ADCValue > joyStickUpValue) ? true : false;
}

bool joyStickDown() {
    ADCValue = ADC;
    return (ADCValue < joyStickDownValue) ? true : false;
}

enum GlobalSM {MainMenu, ReflexGame, PatternGame} GameState;
char displayReflex[20];
char displayPattern[20];
enum MainMenuSM {mainmenu_reset,mainmenu_start, mainmenu_select,mainmenu_pressed, mainmenu_hold} mainmenu_state;
unsigned short type = 0;
void MainMenuTick() {
    switch(mainmenu_state) {
        case mainmenu_reset:
            PORTB = 0x00;
            if((~PINA & 0x0E) == 0x00) mainmenu_state = mainmenu_start;
            break;
        case mainmenu_start:
            set_PWM(0);
            sprintf(displayReflex, "Reflex: %d", reflex_highscore);
            LCD_DisplayString(1,displayReflex);
            sprintf(displayPattern, "Pattern: %d", pattern_highscore);
            LCD_Cursor(17);
            // only for printing
            for(unsigned char i = 0; displayPattern[i] != '\0'; ++i) {
                LCD_WriteData(displayPattern[i]);
            }
            mainmenu_state = mainmenu_select;
            break; 
        case mainmenu_select:
            LCD_Cursor(1);
                if (((~PINA) & 0x0E) == 0x04) {
                    type = 1;
                    mainmenu_state = mainmenu_hold;
                }
                 if (((~PINA) & 0x0E) == 0x02) {
                     type = 0;
                     mainmenu_state = mainmenu_hold;
                 }
            break;
        case mainmenu_hold:
            if (((~PINA) & 0x0E) == 0x00) {
                GameState = (type) ? PatternGame : ReflexGame;
                mainmenu_state = mainmenu_start;
            }
            break;
    }

}

unsigned char reflex_pattern[REFLEX_SIZE];
unsigned char current_reflex_pos;


void ReflexSetup(unsigned char reflex_pattern[]) {
    for(unsigned int i = 0; i < REFLEX_SIZE; ++i) {
        reflex_pattern[i] = (int)get_rand() % 3;
    }
}

unsigned short lfsr = 0xABF1u;
int bit;

// RNG FROM SCRATH
  int get_rand() {
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    return lfsr =  (lfsr >> 1) | (bit << 15);
}

// int lfsr32, lfsr31;

// int shift_lfsr(int *lfsr, int polynominal_mask) {
//     int feedback;
//     feedback = *lfsr & 1;
//     *lfsr >>= 1;
//     if (feedback == 1)
//         *lfsr ^= polynominal_mask;
//     return *lfsr;
// }

// void init_lfsrs(void) {
//     lfsr32 = 0xABCDE;
//     lfsr31 = 0x23456789;
// }

// int get_rand(void) {
//     shift_lfsr(&lfsr32, POLY_MASK_32);
//     return (shift_lfsr(&lfsr32,POLY_MASK_32) ^ shift_lfsr(&lfsr31, POLY_MASK_31)) & 0xffff;
// }

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
    char score[30];
    sprintf(score,"Reflex Time: %d",Reflex_timer);
    if ((~PINA & 0x0E) == ((0x01 << (reflex_pattern[(current_reflex_pos - 1) % REFLEX_SIZE ])))) {
    } else if ((~PINA & 0x0E) != 0x00) {
    } else {
        set_PWM(0);
    }
    LCD_DisplayString(1,score);
}

bool exitReflexGame = false;
enum UpdateReflexScore {updatereflexscore_start, updatereflexscore_press, updatereflexscore_hold } updatereflexscore_state;

void UpdateReflexScoreTick() {
    switch(updatereflexscore_state) { //transitions
        case updatereflexscore_start:
            if((~PINA & 0x0E) != 0x00) {
                updatereflexscore_state = updatereflexscore_press;
            }
            break;
        case updatereflexscore_press:
            updatereflexscore_state = updatereflexscore_hold;
            break;
        case updatereflexscore_hold:
            if((~PINA & 0x0E) == 0x00) updatereflexscore_state = updatereflexscore_start;
            break;
    }
    switch(updatereflexscore_state) {
        case updatereflexscore_press:
            if ((~PINA & 0x0E) == (PORTB << 1)) {
                Reflex_timer = Reflex_timer - 10;
                set_PWM(261.23);
            } else {
                reflex_highscore = (Reflex_timer < reflex_highscore) ? Reflex_timer : reflex_highscore;
                set_PWM(100.00);
                exitReflexGame = true;
            }
            break;
        case updatereflexscore_hold:
            break;
        default: break;
    }
}

bool exitPatternGame = false;
unsigned char* patterngame_pattern[PATTERN_SIZE];
unsigned char current_pattern_pos = 0;


void PatternSetup(char patterngame_pattern[]) {
    for(unsigned int i = 0; i < PATTERN_SIZE; ++i) {
        patterngame_pattern[i] = ((int)get_rand() % 2);
    }
}

bool pattern_done = false;
bool checkpattern_done = false;
bool pattern_game_lost = false;
enum PatternSM { pattern_start, pattern_on, pattern_off} pattern_state;
char tempmsg[20];
unsigned char checkpattern_pos = 0x00;

void PatternTick() {
    switch(pattern_state) { //transitions
        case pattern_start:
            PatternSetup(patterngame_pattern); 
            pattern_done = false;
            pattern_state = pattern_on;
            current_pattern_pos = 0x00;
            break;
        case pattern_on:
            pattern_state = pattern_off;
            break;
        case pattern_off:
            if(current_pattern_pos < PATTERN_SIZE) {
                pattern_state = pattern_on;
            } else {
                pattern_done = true;
                if(checkpattern_done) {
                    pattern_state = pattern_start;
                    checkpattern_pos = 0x00;
                    current_pattern_pos = 0x00;
                    checkpattern_done = false;
                }
            }
            break;
        default:
            pattern_state = pattern_on;
            break;
    }

    switch(pattern_state) { //actions
        case pattern_start:
            break;
        case pattern_on:
            if(patterngame_pattern[current_pattern_pos]) {
                LCD_DisplayString(1, "RIGHT");
            } else {
                LCD_DisplayString(1, "LEFT");
            }
            current_pattern_pos++;
            break;
        case pattern_off:
            LCD_ClearScreen();
            break;
        default:
            break;
    }
}

unsigned long Pattern_timer = 600;
unsigned long pattern_check_timer = 0;

enum CheckPatternSM {checkpattern_start, checkpattern_move, checkpattern_hold} checkpattern_state;
void CheckPatternTick() {
    switch (checkpattern_state) {
        case checkpattern_start:
            if((short)ADC < 100 || (short)ADC > 800) {
                checkpattern_state = checkpattern_move;
            }
            break;
        case checkpattern_move:
            checkpattern_state = checkpattern_hold;
            break;
        case checkpattern_hold:
            if(!((short)ADC < 100 || (short)ADC > 800)) checkpattern_state = checkpattern_start;
            break;
    }
    switch (checkpattern_state) {
        case checkpattern_start: set_PWM(0); break;
        case checkpattern_move: 
            if( ( (short)ADC < 100 && (patterngame_pattern[checkpattern_pos] == 0x01) ) || ( (short)ADC > 600 && (patterngame_pattern[checkpattern_pos] == 0x00) )) {
                pattern_game_lost = true;
                LCD_DisplayString(1, "lol, trash");
                set_PWM(100);
            } else {
                checkpattern_pos++;
                set_PWM(261.23);
            }
            break;
        case checkpattern_hold: 
            if(checkpattern_pos == current_pattern_pos) {
                checkpattern_done = true;
                pattern_done = false;
                checkpattern_state = checkpattern_start;
                sprintf(tempmsg, "NICE! TIME: %d", pattern_check_timer);
                pattern_highscore = ((pattern_highscore > pattern_check_timer) || pattern_highscore == 0) ? pattern_check_timer : pattern_highscore;
                pattern_check_timer = 0x00;
                LCD_DisplayString(1,tempmsg);
            }
            break;
    }
}

unsigned long Reflex_elapsedTime = 0;

unsigned long Pattern_elapsedTime = 0;

void Reset() {
    reflex_state = reflex_start;
    pattern_state = pattern_start;
    mainmenu_state = mainmenu_start;
    checkpattern_state = checkpattern_start;
    GameState = MainMenu;
    mainmenu_state = mainmenu_reset;
    Reflex_elapsedTime = 0;
    pattern_check_timer = 0;
    checkpattern_pos = 0x00;
    current_pattern_pos = 0x00;
    Reflex_timer = 500;
    exitReflexGame = false;
    exitPatternGame = false;
    pattern_game_lost = false;
    checkpattern_done = false;
    pattern_done = false;
    type = 0;
}


int main(void) {
    DDRA = 0x00; PORTA = 0xFE;
    DDRB = 0xFF; PORTB = 0x00;
    DDRC = 0xFF; PORTC = 0x00;
    DDRD = 0xFF; PORTD = 0x00;
   // init_lfsrs();    
    
    
    const unsigned long timerPeriod = 5;
    TimerOn();
    TimerSet(timerPeriod);

    GameState = MainMenu;
    mainmenu_state = mainmenu_reset;
    long PatternLostMsg_elapsedTime = 0;

    PWM_on();
	LCD_init();
    ADC_init();
    while (1) {
        get_rand();
        switch(GameState) {
            case MainMenu:
                MainMenuTick();
                break;
            case ReflexGame:
                if(Reflex_elapsedTime >= Reflex_timer) {
                    ReflexTick();
                    ReflexLCDTick();
                    Reflex_elapsedTime = 0;
                }
                UpdateReflexScoreTick();
                if((~PINA & 0x08) || (exitReflexGame)) { //reset
                    Reset();
                }
                Reflex_elapsedTime+=timerPeriod;
                break;
            case PatternGame:
                if(Pattern_elapsedTime >= Pattern_timer) {
                    PatternTick();
                    Pattern_elapsedTime = 0;
                }
                if((pattern_done)) {
                    pattern_check_timer+=timerPeriod;
                    CheckPatternTick();
                }
                if(pattern_game_lost) {
                    if(PatternLostMsg_elapsedTime > 500) {
                        Reset();
                        PatternLostMsg_elapsedTime = 0;
                    }
                    PatternLostMsg_elapsedTime += timerPeriod;
                }
                if((~PINA & 0x08)) { //reset
                    Reset();
                }
                Pattern_elapsedTime +=timerPeriod;
                break;
        }
        while (!TimerFlag) {}
	    TimerFlag = 0;
    }
    return 1;
}
