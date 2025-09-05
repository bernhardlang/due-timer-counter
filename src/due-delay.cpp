/* due-delay.cpp
 *
 * Copyright (C) 2018-2025 Bernhard Lang, www.bernhard-lang.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include "due-delay.h"

int8_t c_due_delay::begin(uint8_t use_tioab, ext_event_t trigger, edge_t edge) {
  int8_t result;

  result = c_due_clock::begin(use_tioab);
  if (result < 0)
    return result;

  cmr_bits
    = (trigger << TC_CMR_EEVT_Pos) | (edge << TC_CMR_EEVTEDG_Pos)
    | TC_CMR_WAVE | TC_CMR_WAVSEL_UP | TC_CMR_WAVE | TC_CMR_CPCSTOP;
  tioa_bits = 0;
  tiob_bits = 0;
  external_trigger_select = trigger;

  return 0;
}

int8_t c_due_delay::begin(uint8_t tioab,
                          c_due_delay::trigger_mode_t trigger_mode,
                          ext_event_t trigger_select, edge_t edge) {
  if (trigger_mode == trig_soft)
    return begin(tioab, EXT_XC0, EDGE_NONE);

  int8_t result = begin(tioab, trigger_select, edge);
  if (result < 0)
    return result;

  if (trigger_mode == trig_ext_single_shot)
    cmr_bits |= TC_CMR_CPCDIS;
  //enable_external_event(true);

  return 0;
}

c_due_clock &c_due_delay::set_frequency(double frequency) {
  c_due_clock::set_frequency(frequency);
  inv_mck_div = 1.0 / (double) mck_div;
  return *this;
}

c_due_delay &c_due_delay::set_pulse(uint8_t tioab, double pulse_start,
				    double pulse_length) {
  set_frequency(1.0 / (pulse_start + pulse_length));
  return set_delay(tioab, pulse_start);
}

c_due_delay &c_due_delay::set_start_stop(uint8_t tioab, double pulse_start,
				         double pulse_end) {
  set_frequency(1.0 / pulse_end);
  return set_delay(tioab, pulse_start);
}

c_due_delay &c_due_delay::set_delay(uint8_t tioab, double pulse_start) {
  uint32_t r_ab_val = mck_div * pulse_start;

  if (tioab & TIOA)
    tioa_bits = TC_CMR_ACPA_SET | TC_CMR_ACPC_CLEAR;
  if (tioab & TIOB)
    tiob_bits = TC_CMR_BCPB_SET | TC_CMR_BCPC_CLEAR | TC_CMR_EEVT_XC0;

  if (tioab & TIOA)
    p_channel()->TC_RA = r_ab_val;
  if (tioab & TIOB)
    p_channel()->TC_RB = r_ab_val;

  configure(cmr_bits | tioa_bits | tiob_bits);

  return *this;
}

c_due_delay &c_due_delay::set_action(uint8_t tioab, trigger_t trigger,
                                     action_t action) {
  if (tioab & TIOA)
    switch (trigger) {
    case TRIGGER_AB:  tioa_bits = (action << TC_CMR_ACPA_Pos);  break;
    case TRIGGER_C:   tioa_bits = (action << TC_CMR_ACPC_Pos);  break;
    case TRIGGER_EXT: tioa_bits = (action << TC_CMR_AEEVT_Pos); break;
    }

  if (tioab & TIOB)
    switch (trigger) {
    case TRIGGER_AB:  tiob_bits = (action << TC_CMR_BCPB_Pos);  break;
    case TRIGGER_C:   tiob_bits = (action << TC_CMR_BCPC_Pos);  break;
    case TRIGGER_EXT: tiob_bits = (action << TC_CMR_BEEVT_Pos); break;
    }

  configure(cmr_bits | tioa_bits | tiob_bits);

  return *this;
}

double c_due_delay::delay(tioab_t tioab) const {
  return (tioab == TIOA ? p_channel()->TC_RA : p_channel()->TC_RB)
    * inv_mck_div;
}

double c_due_delay::pulse_length(tioab_t tioab) const {
  return (tioab == TIOA ? (p_channel()->TC_RC - p_channel()->TC_RA)
	  : (p_channel()->TC_RC - p_channel()->TC_RB))
    * inv_mck_div;
}

uint8_t c_due_delay::isr_bit(irq_select_t irq) const {
  switch (irq) {
  case irq_rca: return TC_IER_CPAS;
  case irq_rcb: return TC_IER_CPBS;
  case irq_rcc: return TC_IER_CPCS;
  }
  return 0; // dummy
}

c_due_delay &c_due_delay::attach_pulse_interrupt(irq_select_t irq,
                                                 void (*isr)(uint32_t status)) {
  attach_interrupt(isr_bit(irq), isr);
  return *this;
}

c_due_delay &c_due_delay::start(bool external_trigger) {
  if (used) {
    enable(true);
    if (external_trigger) {
      enable_external_event(true);
      if (external_trigger_interrupt_disabled)
        p_channel()->TC_IER = TC_IDR_ETRGS;
      started = true;
    }
    //-->>p_channel()->TC_IER = TC_IER_CPCS | TC_IER_ETRGS;
  }
  return *this;
}

c_due_delay &c_due_delay::stop(void) {
  if (external_events_enabled())
    enable_external_event(false);
  c_due_clock::stop();
  return *this;
}


/******************************************************************************/
/*                               instantiation                                */
/******************************************************************************/

c_due_delay due_delay1(c_due_timer_counter::timer0);
c_due_delay due_delay2(c_due_timer_counter::timer1);
c_due_delay due_delay3(c_due_timer_counter::timer2);
c_due_delay due_delay4(c_due_timer_counter::timer3);
c_due_delay due_delay5(c_due_timer_counter::timer4);
c_due_delay due_delay6(c_due_timer_counter::timer5);
c_due_delay due_delay7(c_due_timer_counter::timer6);
c_due_delay due_delay8(c_due_timer_counter::timer7);
c_due_delay due_delay9(c_due_timer_counter::timer8);
c_due_delay *due_delays[9] = {
  &due_delay1, &due_delay2, &due_delay3, &due_delay4, &due_delay5, &due_delay6,
  &due_delay7, &due_delay8, &due_delay9
};
