/*
 * due-counter.cpp - Arduino Due hardware counters
 *
 * Copyright (C) 2018-2022 Bernhard Lang, www.bernhard-lang.net
 * 
 * Based on code by Dominique Lovy, Université de Genève, May 2014, Nov. 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include "due-counter.h"

/******************************************************************************/
/*                           c_abstract_due_counter                           */
/******************************************************************************/


c_abstract_due_counter::c_abstract_due_counter(timer_no_t counter_no)
  : c_due_timer_counter(counter_no) {
}

c_abstract_due_counter &
c_abstract_due_counter::set_register_compare(rc_register reg_no,
					     uint32_t value) {
  rc_registers |= reg_no;
  switch (reg_no) {
  case rc_a:
    p_channel()->TC_RA = value;
    p_channel()->TC_IER = TC_IER_CPAS;
    break;

  case rc_b:
    p_channel()->TC_RB = value;
    p_channel()->TC_IER = TC_IER_CPBS;
    break;

  case rc_c:
    p_channel()->TC_RC = value;
    p_channel()->TC_IER = TC_IER_CPCS;
    break;
  }

  return *this;
}

c_abstract_due_counter &
c_abstract_due_counter::stop_register_compare(rc_register reg_no) {
  rc_registers &= ~reg_no;
  switch (reg_no) {
  case rc_a: p_channel()->TC_IDR = TC_IDR_CPAS; break;
  case rc_b: p_channel()->TC_IDR = TC_IDR_CPBS; break;
  case rc_c: p_channel()->TC_IDR = TC_IDR_CPCS; break;
  }

  return *this;
}

uint32_t c_abstract_due_counter::get_rc(rc_register reg_no) const {
  switch (reg_no) {
  case rc_a: return p_channel()->TC_RA;
  case rc_b: return p_channel()->TC_RB;
  case rc_c: return p_channel()->TC_RC;
  }
  return 0; // dummy
}

void c_abstract_due_counter::irq_handler(uint32_t status) {
  if (status & rc_registers)
    on_register_compare(status);
}

/******************************************************************************/
/*                               c_due_counter                                */
/******************************************************************************/

c_due_counter::c_due_counter(timer_no_t counter_no)
  : c_abstract_due_counter(counter_no) {
}

int8_t c_due_counter::begin(edge_t edge, uint8_t use_tioab) {
  uint32_t bits;
  int8_t result;

  if ((result = c_due_timer_counter::begin(use_tioab)) < 0) return result;

  if (td_tclk().arduino_pin_no >= 0) {
    // Tell the pripheral to use the alternate function for input pin
    bits = (1UL << td_tclk().pio_bit);
    PIO_SetPeripheral(td_tclk().p_pio, td_tclk().pio_ab, bits);
    // def. in hardware/arduino/sam/system/libsam/include/pio.h
    // inst. in hardware/arduino/sam/system/libsam/source/pio.c
  }

  // Configure the counter source and condition
  bits = 0;
  switch (channel()) {
    case 0: bits = TC_CMR_TCCLKS_XC0; break;
    case 1: bits = TC_CMR_TCCLKS_XC1; break;
    case 2: bits = TC_CMR_TCCLKS_XC2; break;
  }

  switch (edge) {
    case EDGE_NONE:    bits |= TC_CMR_ETRGEDG_NONE; break;
    case EDGE_RISING:  bits |= TC_CMR_ETRGEDG_RISING; break;
    case EDGE_FALLING: bits |= TC_CMR_ETRGEDG_FALLING; break;
    case EDGE_ALL:     bits |= TC_CMR_ETRGEDG_EDGE; break;
    default:           bits |= TC_CMR_ETRGEDG_RISING;
  }
  configure(bits);
  // in hardware/arduino/sam/system/CMSIS/Device/ATMEL/sam3xa/include
  //   -> /component/component_tc.h:

#ifdef NOT_YET
  // select the external clock signal XCx
  bits = 0x03 << (td->channel + td->channel);
  //>>> TCBMask is always zero, ok??
  tc()->TC_BMR
    = (tc()->TC_BMR & ~bits)
      | (tc()BMask << (td->channel+td->channel));
#endif

  return 0;
}

int8_t c_due_counter::begin(clock_select_t clock_select, uint8_t use_tioab) {
  int8_t result;

  if ((result = c_due_timer_counter::begin(use_tioab)) < 0) return result;
  // waveform mode UP no RC
  configure(clock_select | TC_CMR_WAVE);
  return 0;
}

/******************************************************************************/
/*                             c_due_qd_counter                               */
/******************************************************************************/

c_due_qd_counter::c_due_qd_counter(timer_block_no_t timer_block, bool _with_idx)
  : c_abstract_due_counter((timer_no_t)(3*(int)timer_block)),
    with_idx(_with_idx) {
}

int8_t c_due_qd_counter::begin(uint8_t filter) {
  uint32_t bits;
  int8_t result;

  // configures and enables channel 0
  if ((result = c_due_timer_counter::begin()) < 0) return result;

  // channel 1
  c_due_timer_counter((timer_no_t)(((int)timer_no()) + 1)).detach_interrupt();
  if (with_idx) // channel 2
    c_due_timer_counter((timer_no_t)(((int)timer_no()) + 2))
      .detach_interrupt();
  
  // Tell the pripheral to use the alternate function for the input pins
  bits = (1U << td_tioa().pio_bit);
  PIO_SetPeripheral(td_tioa().p_pio, td_tioa().pio_ab, bits);
  bits = (1U << td_tiob().pio_bit);
  PIO_SetPeripheral(td_tiob().p_pio, td_tiob().pio_ab, bits);

  if (with_idx) {
    if (timer_no() == 0) { // not very elegant!
      bits = (1U << 3);    // TIOB1 = PA3, A
      PIO_SetPeripheral(PIOA, PIO_PERIPH_A, bits);
    } else
      if (timer_no() == 6) {      // TIOA7 = PC28
        bits = (1U << 28);
        PIO_SetPeripheral(PIOC, PIO_PERIPH_B, bits);
      }
  }

  // def. in hardware/arduino/sam/system/libsam/include/pio.h
  // inst. in hardware/arduino/sam/system/libsam/source/pio.c

#ifdef NOT_YET
  //>>> TCBMask is always zero, ok??
  tc()->TC_BMR = td->TCBMask;
#endif

  // Configure the counter source and condition
  TC_Configure(tc(), 0, TC_CMR_TCCLKS_XC0);
  TC_Configure(tc(), 1, TC_CMR_TCCLKS_XC0);
  //TC_Configure(tc(), 2, TC_CMR_TCCLKS_XC0);

  // Enable clock for the timer (TC_Configure disables TC Clock)
  pmc_enable_periph_clk(irq());
  pmc_enable_periph_clk(irq() + 1);
  if (with_idx)
    pmc_enable_periph_clk(irq() + 2);

#ifdef NOT_YET
  // select the external clock signal XCx
  bits = 0x03 << (2 * channel());
  //>>> TCBMask is always zero, ok??
  tc()->TC_BMR
    = (tc()->TC_BMR &  ~bits)
    | (td->TCBMask << (2 * channel()));
#endif

  bits = tc()->TC_BMR & ~(TC_BMR_FILTER | TC_BMR_MAXFILT(0x3f));
  if (filter <= 0x3f) bits |= (TC_BMR_FILTER | TC_BMR_MAXFILT(filter));
  tc()->TC_BMR = bits | TC_BMR_QDEN | TC_BMR_POSEN;
  p_channel()->TC_RC = 0x08;

  offset = 0;

  return 0;
}

void c_due_qd_counter::start(void) {
  if (is_used()) {
    if (with_idx) TC_Start(tc(), 1);
    c_due_timer_counter::start();
  }
  tc()->TC_QISR; // clear status
}

void c_due_qd_counter::stop(void) {
  if (is_used()) {
    if (with_idx) TC_Stop(tc(), 1);
    c_due_timer_counter::stop();
  }
}

c_due_qd_counter &c_due_qd_counter::reset(void) {
  stop();
  delayMicroseconds(50);
  offset = 0;
  start(); // reset the counter
  return *this;
}

int32_t c_due_qd_counter::get_index_count(void) const {
  return counter_value() + offset;
}

void c_due_qd_counter::set_index_count(int32_t index) {
  int32_t real_actual = get_index_count() - offset;
  offset = index - real_actual;  
}

bool c_due_qd_counter::direction(void) const {
  return tc()->TC_QISR & TC_QISR_DIR;
}
                                         

/******************************************************************************/
/*                               instantiation                                */
/******************************************************************************/

/* Have all possible counters and qd counters instantiated in Arduino fashion.
   Withoud calling begin() there's no hardware access.
 */
c_due_counter due_counter1(c_due_timer_counter::timer0);
c_due_counter due_counter2(c_due_timer_counter::timer1);
c_due_counter due_counter3(c_due_timer_counter::timer2);
c_due_counter due_counter4(c_due_timer_counter::timer3);
c_due_counter due_counter5(c_due_timer_counter::timer4);
c_due_counter due_counter6(c_due_timer_counter::timer5);
c_due_counter due_counter7(c_due_timer_counter::timer6);
c_due_counter due_counter8(c_due_timer_counter::timer7);
c_due_counter due_counter9(c_due_timer_counter::timer8);

c_due_qd_counter due_qd_counter1(c_due_timer_counter::timer_block0);
c_due_qd_counter due_qd_counter2(c_due_timer_counter::timer_block2);

c_due_counter *due_counters[9] = {
  &due_counter1, &due_counter2, &due_counter3, &due_counter4, &due_counter5,
  &due_counter6, &due_counter7, &due_counter8, &due_counter9
};

c_due_qd_counter *due_qd_counters[2] = { &due_qd_counter1, &due_qd_counter2 };
