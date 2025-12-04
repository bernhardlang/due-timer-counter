/*
 * due-clock.cpp - Use Arduino Due TC timers/counters as clocks on output pins,
 * respectively as internal interrupt or trigger sources
 *
 * Copyright (C) 2018-2025 Bernhard Lang, www.bernhard-lang.net
 *
 * Based on code by Dominique Lovy, Université de Genève, May 2014
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include "due-clock.h"


/******************************************************************************/
/*                            class c_due_clock                               */
/******************************************************************************/

c_due_clock::c_due_clock(timer_no_t timer_no)
  : c_due_timer_counter(timer_no), pulse_count(0), mode_bits(0) {
  effective_frequency = -1;

  // default is square wave
  outa = { percent, 50, TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_SET };
  outb = { percent, 50, TC_CMR_BCPB_SET | TC_CMR_BCPC_CLEAR };

  cmr_bits = TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_EEVT_XC0;
  // EEVT_XCx (x=0..2) needed to keep TIOB as output => external event trigger
  // on XC0
}

int8_t c_due_clock::begin(uint8_t use_tioab, double frequency) {
  int8_t result;

  result = c_due_timer_counter::begin(use_tioab);

  if (frequency > 0)
    set_frequency(frequency);

  return result;
}

uint32_t c_due_clock::get_pulse_len_tc(const out_par_t *out_def,
                                       uint32_t rc_val) {
  uint32_t len;
  switch (out_def->length_unit) {
  case percent: len = round(rc_val  * out_def->pulse_length / 100.0);  break;
  case pertenm: len = round(rc_val  * out_def->pulse_length * 0.0001); break;
  case ppm:     len = round(rc_val  * out_def->pulse_length * 1e-6);   break;
  case micros:  len = round(mck_div * out_def->pulse_length * 1e-6);   break;
  case millis:  len = round(mck_div * out_def->pulse_length * 1e-6);   break;
  case seconds: len = round(mck_div * out_def->pulse_length);          break;
  default : len = out_def->pulse_length;
  }

  if (len < 1) len = 1;
  if (len >= rc_val) len = rc_val - 1;

  return len;
}

/*
 max. frequency : VARIANT_MCK / 4 = 21 MHz (next 16.2, 14, 12, 10.5, 9.333, 8.4)
 min. rep. width = 47.6 ns, min. pulse width = 23.8 ns
 min. frequency 1 : VARIANT_MCK / 2 / (2^32 - 1) = 0.009779 Hz
 min. frequency 4 : VARIANT_MCK / 128 / (2^32 - 1) = 0.0001527 Hz
 min. frequency 5 : 32768 / (2^32 - 1) = 7.629e-6 Hz (ca 1/36.4 h !)
*/

c_due_clock &c_due_clock::set_frequency(double new_frequency) {
  double freqlim;

  if (!is_used())
    return *this;

  demanded_frequency = new_frequency;

  if (mode_bits & quad)
    new_frequency *= 2.0;

  // configure clock source and conditions
  cmr_clock_bits = TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC;
  freqlim = (double)VARIANT_MCK/2/0xffffffffUL;
  if (new_frequency > freqlim) {
    cmr_clock_bits |= TC_CMR_TCCLKS_TIMER_CLOCK1;
    mck_div = VARIANT_MCK/2;
  } else {
    if (new_frequency > freqlim/4) {
      cmr_clock_bits |= TC_CMR_TCCLKS_TIMER_CLOCK2;
      mck_div = VARIANT_MCK/8;
    } else {
      if (new_frequency > freqlim/16) {
        cmr_clock_bits |= TC_CMR_TCCLKS_TIMER_CLOCK3;
        mck_div = VARIANT_MCK/32;
      } else {
        if (new_frequency > freqlim/64) {
          cmr_clock_bits |= TC_CMR_TCCLKS_TIMER_CLOCK4;
          mck_div = VARIANT_MCK/128;
        } else {
          cmr_clock_bits |= TC_CMR_TCCLKS_TIMER_CLOCK5;
          mck_div = 32768;
        }
      }
    }
  }

  rc_val = round(mck_div / (double) new_frequency);

  if (rc_val < 2)
    rc_val = 2;
  effective_frequency = mck_div / (double) rc_val;

  if (mode_bits & quad)
    effective_frequency /= 2.0;

  if (!(mode_bits & delayed_update))
    update_registers();

  return *this;
}

c_due_clock &c_due_clock::set_period(long microseconds) {
  double fr;

  fr = 1000000.0 / (double) microseconds;
  set_frequency(fr);

  return *this;
}

c_due_clock &c_due_clock::set_out_a(pulse_unit_t unit, uint32_t length,
                                    tio_mode_t tio_mode) {
  if (!is_used()) return *this;

  outa.length_unit = unit;
  outa.pulse_length = length;
  switch (tio_mode) {
  case normal:
    outa.channel_mode_bits = TC_CMR_ACPA_CLEAR | TC_CMR_ACPC_SET;
    break;

  case inverted:
    outa.channel_mode_bits = TC_CMR_ACPA_SET | TC_CMR_ACPC_CLEAR;
    break;

  case toggle: outa.channel_mode_bits = TC_CMR_ACPA_TOGGLE; break;
  }

  if (!(mode_bits & delayed_update))
    update_registers();

  return *this;
}

c_due_clock &c_due_clock::set_out_b(pulse_unit_t unit, uint32_t length,
                                    tio_mode_t tio_mode) {
  if (!is_used()) return *this;

  outb.length_unit = unit;
  outb.pulse_length = length;

  switch (tio_mode) {
  case normal:
    outb.channel_mode_bits
      = TC_CMR_BCPB_CLEAR | TC_CMR_BCPC_SET | TC_CMR_EEVT_XC0;
    break;

  case inverted:
    outb.channel_mode_bits
      = TC_CMR_BCPB_SET | TC_CMR_BCPC_CLEAR | TC_CMR_EEVT_XC0;
    break;

  case toggle:
    outb.channel_mode_bits = TC_CMR_BCPB_TOGGLE | TC_CMR_EEVT_XC0;
  }

  if (!(mode_bits & delayed_update))
    update_registers();

  return *this;
}

c_due_clock &c_due_clock::set_out(uint8_t tioab, pulse_unit_t pulse_unit,
                                  uint32_t pulse_length, tio_mode_t tio_mode) {
  mode_bits |= delayed_update;
  if (tioab & TIOA)
    set_out_a(pulse_unit, pulse_length, tio_mode);
  if (tioab & TIOB)
    set_out_b(pulse_unit, pulse_length, tio_mode);
  mode_bits &= ~delayed_update;
  if (tioab)
    update_registers();
  return *this;
}

c_due_clock &c_due_clock::set_out(pulse_unit_t unit, uint32_t length,
                                  tio_mode_t tio_mode) {
  mode_bits |= delayed_update;
  set_out_a(unit, length, tio_mode);
  mode_bits &= ~delayed_update;
  set_out_b(unit, length, tio_mode);
  return *this;
}

c_due_clock::pulse_unit_t c_due_clock::get_out(uint8_t tioab) const {
  return tioab == TIOA ? outa.length_unit : outb.length_unit;
}

uint32_t c_due_clock::get_length(uint8_t tioab) const {
  return tioab == TIOA ? outa.pulse_length : outb.pulse_length;
}

c_due_clock &c_due_clock::set_quad(qd_direction_t direction, double freq) {
  if (tioab_pin_used) {
    bool was_quad = mode_bits & quad;

    outb.channel_mode_bits
      = TC_CMR_BCPB_TOGGLE | TC_CMR_BCPC_NONE | TC_CMR_BSWTRG_CLEAR;

    if (direction == qd_forward) {
      mode_bits |= quad | quad_forward;
      outa.channel_mode_bits
	= TC_CMR_ACPA_NONE | TC_CMR_ACPC_TOGGLE | TC_CMR_ASWTRG_SET;
    } else {
      mode_bits = (mode_bits | quad) & ~quad_forward;
      outa.channel_mode_bits
	= TC_CMR_ACPA_NONE | TC_CMR_ACPC_TOGGLE | TC_CMR_ASWTRG_CLEAR;
    }

    if (freq > 0.0) {
      set_frequency(freq);
    } else {
      if (!was_quad)
        set_frequency(demanded_frequency);
      else
        update_registers();
    }
  }

  return *this;
}

c_due_clock::qd_direction_t c_due_clock::qd_direction(void) const {
  if ((!(mode_bits & quad)) || (mode_bits & quad_forward))
    return qd_forward;
  return qd_backward;
}

void c_due_clock::start(uint32_t n_pulses) {
  if (n_pulses > 0) { // run counter for n_pulses cycles as configured before
    pulse_count = n_pulses;
    tc()->TC_CHANNEL[channel()].TC_IER = TC_IER_CPCS;
  }

  c_due_timer_counter::start();
}

void c_due_clock::update_registers(void) {
  bool isstarted;

  if ((isstarted = is_started()))
    stop();
  if (mode_bits & quad) {
    p_channel()->TC_RA = rc_val / 2;
    p_channel()->TC_RB = rc_val / 2;
  } else {
    p_channel()->TC_RA = get_pulse_len_tc(&outa, rc_val);
    p_channel()->TC_RB = get_pulse_len_tc(&outb, rc_val);
  }

  p_channel()->TC_RC = rc_val;

  configure(cmr_clock_bits | cmr_bits | outa.channel_mode_bits
	    | outb.channel_mode_bits);

  if (isstarted)
    start();
}

void c_due_clock::irq_handler(uint32_t status) {
  if (pulse_count) {
    if (--pulse_count == 0)
      stop();
  }
  c_due_timer_counter::irq_handler(status);
}

double c_due_clock::get_frequency(void) const {
  if (is_started())
    return mode_bits & quad ? effective_frequency : effective_frequency / 2.0;

  return 0;
}


/******************************************************************************/
/*                               instantiation                                */
/******************************************************************************/

#ifndef __no_clock_instances__
c_due_clock due_clock1(c_due_timer_counter::timer0);
c_due_clock due_clock2(c_due_timer_counter::timer1);
c_due_clock due_clock3(c_due_timer_counter::timer2);
c_due_clock due_clock4(c_due_timer_counter::timer3);
c_due_clock due_clock5(c_due_timer_counter::timer4);
c_due_clock due_clock6(c_due_timer_counter::timer5);
c_due_clock due_clock7(c_due_timer_counter::timer6);
c_due_clock due_clock8(c_due_timer_counter::timer7);
c_due_clock due_clock9(c_due_timer_counter::timer8);
c_due_clock *due_clocks[9] = {
  &due_clock1, &due_clock2, &due_clock3, &due_clock4, &due_clock5, &due_clock6,
  &due_clock7, &due_clock8, &due_clock9
};
#endif // instantiate clocks_
