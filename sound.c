#include "sound.h"

#define NUM_OSCILLATORS 4

extern uint32_t notetable[128];

/**
 * Oscillator state.
 */
typedef struct
{
  uint32_t *phaseptr;
  uint32_t freq;
  uint32_t phase;
} oscillator_state_t;


/**
 * This structure mirrors the oscillator-updating code in the SysTick handler
 * and allows it to be modified in a structured manner.
 * The __pad members should not be touched!
 */
typedef struct
{
  uint16_t __pad1[3];
  uint8_t volume;
  uint8_t __pad2;
  union {
    uint16_t waveform_code[4];
    struct {
      uint16_t __pad3;
      uint8_t duty;
      uint8_t __pad4;
      uint16_t __pad5[2];
    };
  };
  uint16_t __pad6;
} oscillator_control_t;


extern volatile oscillator_state_t oscillators[4];
extern volatile oscillator_control_t osc_update_base[4];


#define TEST_FREQ 2000000

/**
 * Modifies the waveform generation code to generate a pulse wave for the
 * specified voice.
 */
static inline void oscillator_set_pulse(int oscnum, uint8_t duty)
{
  osc_update_base[oscnum].waveform_code[0] = 0x15d2;      /* asr r2, #23 */
  osc_update_base[oscnum].waveform_code[1] = 0x3a00|duty; /* sub r2, #<duty> */ 
  osc_update_base[oscnum].waveform_code[2] = 0x0409;      /* lsl r1, #16 */
  osc_update_base[oscnum].waveform_code[3] = 0x404a;      /* eor r2, r1 */
}


/**
 * Modifies the waveform generation code to generate a sawtooth wave for the
 * specified voice.
 */
static inline void oscillator_set_sawtooth(int oscnum)
{
#if 0
  osc_update_base[oscnum].waveform_code[0] = 0x4252; /* neg r2, 2 */
  osc_update_base[oscnum].waveform_code[1] = 0x13d2; /* asr r2, #15 */
  osc_update_base[oscnum].waveform_code[2] = 0x434a; /* mul r2, r1 */ 
  osc_update_base[oscnum].waveform_code[3] = 0x46c0; /* nop */   
#else
  osc_update_base[oscnum].waveform_code[0] = 0x13d2; /* asr r2, #15 */
  osc_update_base[oscnum].waveform_code[1] = 0x434a; /* mul r2, r1 */ 
  osc_update_base[oscnum].waveform_code[2] = 0x46c0; /* nop */   
  osc_update_base[oscnum].waveform_code[3] = 0x46c0; /* nop */
#endif
}


/**
 * Modifies the waveform generation code to generate a ramp (reverse sawtooth)
 * wave for the specified voice.
 */
static inline void oscillator_set_lofi_sawtooth(int oscnum)
{
  osc_update_base[oscnum].waveform_code[0] = 0x1792; /* asr r2, #30 */
  osc_update_base[oscnum].waveform_code[1] = 0x03d2; /* lsl r2, #15 */
  osc_update_base[oscnum].waveform_code[2] = 0x434a; /* mul r2, r1 */ 
  osc_update_base[oscnum].waveform_code[3] = 0x46c0; /* nop */
}



void sound_init(void)
{
//  int i;
//  for (i = 0; i < NUM_OSCILLATORS; i++) {
//    oscillators[i].freq = TEST_FREQ;
//    osc_update_base[i].volume = 255;
//  }
}


void sound_set_duty_cycle(uint8_t val, uint8_t oscmask)
{
  int i;
  for (i = 0; i < NUM_OSCILLATORS; i++) {
    if (oscmask & (1 << i)) {
      /* is the oscillator already in pulse mode? */
      if (osc_update_base[i].waveform_code[0] == 0x15d2) {
        osc_update_base[i].duty = val;
      }
      /* if not, switch to pulse mode */
      else {
        oscillator_set_pulse(i, val);
      }
    }
  }
}


void sound_set_sawtooth(uint8_t oscmask)
{
  int i;
  for (i = 0; i < NUM_OSCILLATORS; i++) {
    if (oscmask & (1 << i)) {
//      /* is the oscillator already in sawtooth mode? if not, switch */
//      if (osc_update_base[i].waveform_code[0] != 0x13d2) {
        oscillator_set_sawtooth(i);
//      }
    }
  }
}


void sound_set_lofi_sawtooth(uint8_t oscmask)
{
  int i;
  for (i = 0; i < NUM_OSCILLATORS; i++) {
    if (oscmask & (1 << i)) {
//      /* is the oscillator already in sawtooth mode? if not, switch */
//      if (osc_update_base[i].waveform_code[0] != 0x13d2) {
        oscillator_set_lofi_sawtooth(i);
//      }
    }
  }
}


static uint16_t current_note = 0;
static int16_t detune_amounts[NUM_OSCILLATORS];

void update_frequencies()
{
  int i;
  for (i = 0; i < NUM_OSCILLATORS; i++) {
    uint16_t note = current_note + detune_amounts[i];
    uint8_t basenote = note >> 9;
    uint32_t fracnote = note & ((1 << 9)-1);
    uint32_t basefreq = notetable[basenote];
    uint32_t delta = (notetable[basenote+1]-basefreq) >> 9;
    oscillators[i].freq = basefreq + fracnote*delta;
  }  
}  


void sound_set_detune(uint8_t mode, uint8_t val)
{
  switch (mode) {
    case 0:
      detune_amounts[0] = 0;
      detune_amounts[1] = 0;
      detune_amounts[2] = val;
      detune_amounts[3] = val;
      break;
    case 1:
      detune_amounts[0] = 0;
      detune_amounts[1] = 0;
      detune_amounts[2] = val;
      detune_amounts[3] = -val;
      break;
    case 2:
      detune_amounts[0] = 0;
      detune_amounts[1] = val*2;
      detune_amounts[2] = val;
      detune_amounts[3] = -(val >> 1);
      break;
    case 3:
      detune_amounts[0] = 0;
      detune_amounts[1] = -val*2;
      detune_amounts[2] = val*3;
      detune_amounts[3] = val;
      break;

  }
  update_frequencies();
}


uint32_t freq_for_note(uint16_t note)
{
  uint8_t basenote = note >> 9;
  uint32_t fracnote = note & ((1 << 9)-1);
  uint32_t basefreq = notetable[basenote];
  uint32_t delta = (notetable[basenote+1]-basefreq) >> 9;
  return basefreq + fracnote*delta;
}


void note_on(uint8_t notenum)
{
  current_note = notenum << 9;
  int i;
  for (i = 0; i < NUM_OSCILLATORS; i++) {
    oscillators[i].phase = 0;
    osc_update_base[i].volume = 255;
  }
  update_frequencies();
}


void note_off(uint8_t notenum)
{
  /* TODO: proper handling */
  if (notenum != (current_note >> 9)) {
    return;
  }

  int i;
  for (i = 0; i < NUM_OSCILLATORS; i++) {
    osc_update_base[i].volume = 0;
  }
}
