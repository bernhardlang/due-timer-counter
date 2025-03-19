/*
 * due-capturer.cpp - Use Arduino Due TC timers/counters for time-tagging events
 *
 * Copyright (C) 2019-2022 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include "due-capturer.h"

c_due_capturer::c_due_capturer(timer_no_t timer_no)
  : c_due_timer_counter(timer_no) {
}

int8_t c_due_capturer::begin(clock_select_t clock_select, bool with_trigger) {
  int8_t result;

  if (with_trigger)
    result = c_due_timer_counter::begin(TIOA | TIOB);
  else
    result = c_due_timer_counter::begin(TIOA);
  if (result < 0)
    return result;

  //cmr_bits = (p_channel()->TC_CMR &~ (TC_CMR_TCCLKS_Msk)) | clock_select;
  p_channel()->TC_CMR
    = (p_channel()->TC_CMR & ~(TC_CMR_TCCLKS_Msk)) | clock_select;

  return 0;
}

c_due_capturer &c_due_capturer::prepare(rc_select_t rc,
                                        t_ring_buffer<uint32_t> *buffer) {
  if (rc == rc_a)
    buffer_a = buffer;
  else
    buffer_b = buffer;

  return *this;
}

c_due_capturer &c_due_capturer::prepare(uint8_t id, rc_select_t rc,
                                        t_ring_buffer<capture_t> *buffer) {
  if (rc == rc_a) {
    id_buffer_a = buffer;
    id_a = id;
  } else {
    id_buffer_b = buffer;
    id_b = id;
  }

  return *this;
}

c_due_capturer &c_due_capturer::start(uint32_t max, uint32_t first,
				      uint32_t second) {
  p_channel()->TC_CMR
    = (p_channel()->TC_CMR & ~(TC_CMR_LDRA_Msk | TC_CMR_LDRB_Msk))
    | first | second;

  p_channel()->TC_SR; // clear status register
  p_channel()->TC_IER = TC_IER_LDRAS | TC_IER_LDRBS;//irq_bits;

  capture_max = max;
  capture_counter = 0;

  c_due_timer_counter::start();

  return *this;
}

uint32_t irq_status = 0;

void c_due_capturer::irq_handler(uint32_t ir_status) {
  if (capture_max && (capture_counter == capture_max)) {
    if (is_started())
      stop();
    return;
  }

  irq_status = ir_status;
  if (ir_status & TC_SR_LDRAS) {
    if (buffer_a) buffer_a->write(p_channel()->TC_RA);
    else
      if (id_buffer_a) {
        capture_t data = { p_channel()->TC_RA, id_a };
        id_buffer_a->write(data);
      } else
        on_capture_a();
    capture_counter++;
  }

  if (ir_status & TC_SR_LDRBS) {
    if (buffer_b) buffer_b->write(p_channel()->TC_RB);
    else
      if (id_buffer_b) {
        capture_t data = { p_channel()->TC_RB, id_b };
        id_buffer_b->write(data);
      } else
        on_capture_b();
  }
}

//! Instantiate all available capturers.

c_due_capturer due_capturer1(c_due_timer_counter::timer0);
c_due_capturer due_capturer2(c_due_timer_counter::timer1);
c_due_capturer due_capturer3(c_due_timer_counter::timer6);
c_due_capturer due_capturer4(c_due_timer_counter::timer7);
c_due_capturer due_capturer5(c_due_timer_counter::timer8);
c_due_capturer *due_capturers[5] = {
  &due_capturer1, &due_capturer2, &due_capturer3, &due_capturer4, &due_capturer5
};
  
#ifdef DEBUG_DUE_TIMER_COUNTER
#include "command-parser.h"

void c_due_capturer::debug(c_command_parser *p) {
  c_due_timer_counter::debug(p);
  p->respond("%d (%d)", capture_counter, capture_max);
}
#endif
