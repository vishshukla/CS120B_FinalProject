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


enum GlobalSM {MainMenu, ReflexGame, FollowPattern} GameState;

enum MainMenuSM {mainmenu_start, mainmenu_select, mainmenu_reflex, mainmenu_pattern, mainmenu_hold} mainmenu_state;

void MainMenuTick() {
    switch(mainmenu_state) {
        case mainmenu_start:
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


void ReflexSetup(unsigned char reflex_pattern[]) {
    for(unsigned int i = 0; i < REFLEX_SIZE; ++i) {
        reflex_pattern[i] = rand() % 3;
    }
}

enum ReflexSM {reflex_start, reflex_on, reflex_off} reflex_state;
void ReflexTick() {
    switch(reflex_state) { //transitions
        case reflex_start:
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
            break;
        case reflex_on:
            reflex_temp = (0x01 << reflex_pattern[current_reflex_pos]);
            current_reflex_pos = (current_reflex_pos + 1) % REFLEX_SIZE;
            break;
        case reflex_off:
            reflex_temp = 0x00;
        default:
            break;
    }

}

unsigned char speaker_temp = 0x00;
enum SpeakerSM {speaker_start, speaker_s0, speaker_s1, speaker_wait} Speaker_state;
bool hitRight = true;
void SpeakerSM_Tick() {
    switch(Speaker_state) {
        case speaker_start:
            Speaker_state = speaker_s0;
            break;
        case speaker_s0:
            if ((~PINA & 0x07) == ((0x01 << reflex_pattern[(current_reflex_pos - 1) % REFLEX_SIZE ]))) Speaker_state = speaker_s1;
            //else hitRight = false;
            break;
        case speaker_s1:
            if (reflex_temp == 0x00) Speaker_state = speaker_wait;
            break;
        case speaker_wait:
            if (reflex_temp != 0x00) Speaker_state = speaker_s0;
            break;
        default: break;
    }

    switch(Speaker_state) {
        case speaker_start:
        case speaker_s0: 
			speaker_temp = 0x00; 
			break;
        case speaker_s1:
            speaker_temp = !speaker_temp;   
			break;
        case speaker_wait:
            speaker_temp = 0x00;
            break;
        default: break;
    }
}


enum SpeakerReflexSM {speakerreflex_state} SpeakerReflex_state;

void SpeakerReflexSMsTick() {
	switch(SpeakerReflex_state) { // transitions
		case speakerreflex_state: 
		default: break;
	}

	switch(SpeakerReflex_state) { // actions
		case speakerreflex_state: PORTB = reflex_temp | (speaker_temp << 4);
                 break;
		default: break;
	}
}



int main(void) {
    DDRA = 0x00; PORTA = 0xFF;
    DDRB = 0xFF; PORTB = 0x00;
    srand(time(0));
    
    const unsigned long timerPeriod = 3;
    TimerOn();
    TimerSet(timerPeriod);

    GameState = MainMenu;

    reflex_state = reflex_on;
    unsigned long Reflex_elapsedTime = 0;
    unsigned long Reflex_timer = 1000;
    ReflexSetup(reflex_pattern); // CALL THIS WHEN USER HITS OPTION IN MENU
    while (1) {
        switch(GameState) { //actions
            case MainMenu:
                MainMenuTick();
                break;
            case ReflexGame:
                if(Reflex_elapsedTime >= Reflex_timer) {
                    ReflexTick();
                    Reflex_elapsedTime = 0;
                    Reflex_timer -= 50;
                }
                if((~PINA & 0x04) || Reflex_timer < 200 ) { //reset
                    PORTB = 0x00;
                    reflex_state = reflex_start;
                    GameState = MainMenu;
                    Reflex_elapsedTime = 0;
                    Reflex_timer = 1000;
                    hitRight = true;
                    break;
                }
                SpeakerSM_Tick();
                SpeakerReflexSMsTick();
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
