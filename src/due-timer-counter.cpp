/* due-timer-counter.cpp
 *
 * Copyright (C) 2018-2025 Bernhard Lang, www.bernhard-lang.net
 *
 * Based on code by Dominique Lovy, Université de Genève, May 2014, Nov. 2016
 * and on code from DueTimer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include "due-timer-counter.h"

#ifndef PIOE
# define PIOE NULL // not present in SAM3X8E
#endif

const c_due_timer_counter::due_timer_descr_t
c_due_timer_counter::timer_counter[] = {
  { TC0, 0, TC0_IRQn,
    { pin22,  PIOB, 26, PIO_PERIPH_B }, // TCLK0, PB26, D22
    { pin2 ,  PIOB, 25, PIO_PERIPH_B }, // TIOA0, PB25, D2
    { pin13,  PIOB, 27, PIO_PERIPH_B }},// TIOB0, PB27, D13
  { TC0, 1, TC1_IRQn,
    { pin59,  PIOA,  4, PIO_PERIPH_A }, // TCLK1, PA4, A5
    { pin61,  PIOA,  2, PIO_PERIPH_A }, // TIOA1, PA2, A7
    { pin60,  PIOA,  3, PIO_PERIPH_A }},// TIOB1, PA3, A6
  { TC0, 2, TC2_IRQn,
    { pin31,  PIOA,  7, PIO_PERIPH_A }, // TCLK2, PA7, D31
    { no_pin, PIOA,  5, PIO_PERIPH_A }, // TIOA2, PA5, --
    { pin58,  PIOA,  6, PIO_PERIPH_A }},// TIOB2, PA6, A4
  { TC1, 0, TC3_IRQn,
    { pin58,  PIOA, 22, PIO_PERIPH_B }, // TCLK3, PA22, A3
    { no_pin, PIOE,  9, PIO_PERIPH_A }, // TIOA3, PE9,  --
    { no_pin, PIOE, 10, PIO_PERIPH_A }},// TIOB3, PE10, --
  { TC1, 1, TC4_IRQn,
    { pin59,  PIOA, 23, PIO_PERIPH_B }, // TCLK4, PA23, A2
    { no_pin, PIOE, 11, PIO_PERIPH_A }, // TIOA4, PE11, --
    { no_pin, PIOE, 12, PIO_PERIPH_A }},// TIOB4, PE12, --
  { TC1, 2, TC5_IRQn,
    { pin67,  PIOB, 16, PIO_PERIPH_A }, // TCLK5, PB16, DAC1
    { no_pin, PIOE, 13, PIO_PERIPH_A }, // TIOA5, PE13, --
    { no_pin, PIOE, 14, PIO_PERIPH_A }},// TIOB5, PE14, --
  { TC2, 0, TC6_IRQn,
    { no_pin, PIOC, 27, PIO_PERIPH_B }, // TCLK6, PC27, --
    { pin5,   PIOC, 25, PIO_PERIPH_B }, // TIOA6, PC25, D5
    { pin4,   PIOC, 26, PIO_PERIPH_B }},// TIOB6, PC26, D4
  { TC2, 1, TC7_IRQn,
    { no_pin, PIOC, 30, PIO_PERIPH_B }, // TCLK7, PC30, --
    { pin3,   PIOC, 28, PIO_PERIPH_B }, // TIOA7, PC28, D3
    { pin10,  PIOC, 29, PIO_PERIPH_B }},// TIOB7, PC29, D10
  { TC2, 2, TC8_IRQn,
    { pin30,  PIOD,  9, PIO_PERIPH_B }, // TCLK8, PD9, D30
    { pin11,  PIOD,  7, PIO_PERIPH_B }, // TIOA8, PD7, D11
    { pin12,  PIOD,  8, PIO_PERIPH_B }} // TIOB8, PD8, D12
};
// see hardware/arduino/sam/variants/arduino_due_x/variant.cpp

void (*c_due_timer_counter::callbacks[])(uint32_t) = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

c_due_timer_counter *c_due_timer_counter::registered_timer_counters[] = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

void (*c_due_timer_counter::qd_callbacks[NUM_TIMER_BLOCKS])(uint32_t) = {
  NULL, NULL, NULL
};

uint32_t c_due_timer_counter::irq_masks[NUM_TIMERS];
uint32_t c_due_timer_counter::qd_irq_masks[NUM_TIMER_BLOCKS];

c_due_timer_counter::c_due_timer_counter(timer_no_t timer_no/*timer1-9*/)
  : _timer_no(timer_no), td(&timer_counter[timer_no]), used(false),
    external_trigger_interrupt_disabled(false), started(false),
    tioab_pin_used(false) {
}

int8_t c_due_timer_counter::begin(uint8_t use_tioab) {
  if (registered_timer_counters[_timer_no]) return -1;
  used = true;
  registered_timer_counters[_timer_no] = this;

  irq_masks[td->channel] = 0xFFFFFFFF; // clear all interrupts when detaching
  detach_interrupt();

  // tell Power Management Controller to disable write protection
  pmc_set_writeprotect(false);

  // enable clock for the timer
  pmc_enable_periph_clk(irq());

  // tell pio to use peripheral function for input/output pin
  if ((use_tioab & TIOA) && (td_tioa().arduino_pin_no >= 0)) {
    tioab_pin_used = TIOA;
    PIO_SetPeripheral(td_tioa().p_pio, td_tioa().pio_ab,
		      1UL << td_tioa().pio_bit);
  }

  // defined in hardware/arduino/sam/system/libsam/include/pio.h
  // instanciated in hardware/arduino/sam/system/libsam/source/pio.c

  if ((use_tioab & TIOB) && (td_tiob().arduino_pin_no >= 0)) {
    tioab_pin_used |= TIOB;
    PIO_SetPeripheral(td_tiob().p_pio, td_tiob().pio_ab,
		      1UL << td_tiob().pio_bit);
  }

  // return actual pin usage
  return tioab_pin_used;
}

void c_due_timer_counter::end(void) {
  if (used) {
    stop();
    registered_timer_counters[_timer_no] = NULL;
    used = false;
    if (tioab_pin_used & TIOA)
      PIO_SetInput(td_tioa().p_pio, td_tioa().pio_ab, 1UL << td_tioa().pio_bit);
    if (tioab_pin_used & TIOB)
      PIO_SetInput(td_tiob().p_pio, td_tioa().pio_ab, 1UL << td_tiob().pio_bit);
  }
}

c_due_timer_counter &c_due_timer_counter::trigger(void) {
  p_channel()->TC_CCR = TC_CCR_SWTRG;
  return *this;
}

c_due_timer_counter &c_due_timer_counter::sync(void) {
  tc()->TC_BCR = TC_BCR_SYNC;
  return *this;
}

void c_due_timer_counter::configure(uint32_t channel_mode_bits) {
  bool was_started = started;
  bool was_enabled = is_enabled();

  if (was_started)
    stop();
  else
    if (was_enabled)
      enable(false);

  /* set bits */
  p_channel()->TC_CMR = channel_mode_bits;

  /* clear status register */
  get_status();

  if (was_started)
    start();
  else
    if (was_enabled)
      enable(true);
}

c_due_timer_counter &c_due_timer_counter::enable(bool enable) {
  if (used) {
    if (enable) {
      NVIC_DisableIRQ(td->irq);

      // enable external event interrupt to be able to update the started flag
      if (p_channel()->TC_CMR & TC_CMR_ENETRG)
        p_channel()->TC_IER = TC_IER_ETRGS;
      //else
      //  p_channel()->TC_IDR = TC_IDR_ETRGS;

      NVIC_ClearPendingIRQ(td->irq);
      NVIC_EnableIRQ(td->irq);
      p_channel()->TC_CCR = TC_CCR_CLKEN;
    } else {
      NVIC_DisableIRQ(td->irq);
      p_channel()->TC_CCR = TC_CCR_CLKDIS;
    }
  }

  return *this;
}

bool c_due_timer_counter::is_enabled(void) const {
  return p_channel()->TC_SR & TC_SR_CLKSTA;
}

c_due_timer_counter &c_due_timer_counter::start(void) {
  if (used) {
    enable(true);
    started = true;
    external_trigger_interrupt_disabled = false;
    p_channel()->TC_CCR = TC_CCR_SWTRG;
  }

  return *this;
}

c_due_timer_counter &c_due_timer_counter::stop(void) {
  enable(false);
  if (p_channel()->TC_IMR & TC_IDR_ETRGS) {
    external_trigger_interrupt_disabled = true;
    p_channel()->TC_IDR = TC_IDR_ETRGS;
  } else {
    external_trigger_interrupt_disabled = false;
  }
  started = false;

  return *this;
}

uint32_t c_due_timer_counter::get_status(void) const {
  return TC_GetStatus(tc(), td->channel);
}

c_due_timer_counter &c_due_timer_counter::clear(void) {
  p_channel()->TC_CV = 0;
  return *this;
}

uint32_t c_due_timer_counter::counter_value(void) const {
  return TC_ReadCV(tc(), td->channel);
}

uint32_t c_due_timer_counter::ra(void) const {
  return p_channel()->TC_RA;
}

uint32_t c_due_timer_counter::rb(void) const {
  return p_channel()->TC_RB;
}

uint32_t c_due_timer_counter::rc(void) const {
  return p_channel()->TC_RC;
}

pin_no_t c_due_timer_counter::tclk_pin_no(void) const {
  return td->tclk.arduino_pin_no;
}

c_due_timer_counter &c_due_timer_counter::set_out(uint8_t tioab, uint32_t n,
						  action_t action, bool is_rc) {
  if (tioab & TIOA) {
    TC_SetRA(tc(), td->channel, n);
    p_channel()->TC_CMR
      = (p_channel()->TC_CMR & ~TC_CMR_ACPA_Msk) | (action<<(is_rc ? 18 : 16));
  }

  if (tioab & TIOB) {
    TC_SetRB(tc(), td->channel, n);
    p_channel()->TC_CMR
      = (p_channel()->TC_CMR & ~TC_CMR_BCPB_Msk) | (action<<(is_rc ? 26 : 24));
  }

  return *this;
}

c_due_timer_counter &c_due_timer_counter::set_out_extern(uint8_t tioab,
							 action_t action) {
  if (tioab & TIOA)
    p_channel()->TC_CMR
      = (p_channel()->TC_CMR & ~TC_CMR_AEEVT_Msk) | (action<<20);

  if (tioab & TIOB)
    p_channel()->TC_CMR
      = (p_channel()->TC_CMR & ~TC_CMR_BEEVT_Msk) | (action<<28);

  return *this;
}

c_due_timer_counter &c_due_timer_counter::set_out_soft(uint8_t tioab,
						       action_t action) {
  if (tioab & TIOA)
    p_channel()->TC_CMR
      = (p_channel()->TC_CMR & ~TC_CMR_ASWTRG_Msk) | (action<<22);

  if (tioab & TIOB)
    p_channel()->TC_CMR
      = (p_channel()->TC_CMR & ~TC_CMR_BSWTRG_Msk) | (action<<30);

  return *this;
}

c_due_timer_counter &
c_due_timer_counter::select_external_event(ext_event_t event, edge_t edge) {
  p_channel()->TC_CMR
    = (p_channel()->TC_CMR & ~(TC_CMR_EEVTEDG_Msk | TC_CMR_EEVT_Msk))
    | (edge<<TC_CMR_EEVTEDG_Pos) | (event<<TC_CMR_EEVT_Pos);

  return *this;
}

c_due_timer_counter &c_due_timer_counter::enable_external_event(bool enable) {
  if (enable)
    p_channel()->TC_CMR |= TC_CMR_ENETRG;
  else
    p_channel()->TC_CMR &= ~TC_CMR_ENETRG;

  return *this;
}

bool c_due_timer_counter::external_events_enabled(void) const {
  return p_channel()->TC_CMR & TC_CMR_ENETRG;
}

c_due_timer_counter &c_due_timer_counter::burst(burst_select_t burst_select) {
  p_channel()->TC_CMR
    = (p_channel()->TC_CMR & ~TC_CMR_BURST_Msk) | (burst_select<<4);

  return *this;
}

c_due_timer_counter &c_due_timer_counter::stop_on_rc(bool stop) {
  if (stop)
    p_channel()->TC_CMR |= TC_CMR_CPCSTOP;
  else
    p_channel()->TC_CMR &= ~TC_CMR_CPCSTOP;

  return *this;
}

c_due_timer_counter &c_due_timer_counter::disable_on_rc(bool disable) {
  if (disable)
    p_channel()->TC_CMR |= TC_CMR_CPCDIS;
  else
    p_channel()->TC_CMR &= ~TC_CMR_CPCDIS;

  return *this;
}

c_due_timer_counter &
c_due_timer_counter::attach_interrupt(uint32_t bits,
                                      void (*isr)(uint32_t)) {
  if (used) {
    if (callbacks[_timer_no])
      detach_interrupt();
    callbacks[_timer_no] = isr;
    irq_masks[_timer_no] = bits;
    p_channel()->TC_IER = bits;
  }
  
  return *this;
}

c_due_timer_counter &c_due_timer_counter::detach_interrupt(void) {
  if (used) {
    stop(); // Stop the currently running timer
    p_channel()->TC_IDR = irq_masks[td->channel];
    callbacks[_timer_no] = NULL;
    irq_masks[_timer_no] = 0;
  }

  return *this;
}

c_due_timer_counter &
c_due_timer_counter::attach_qd_interrupt(uint32_t bits,
                                         void (*isr)(uint32_t)) {
  if (used) {
    uint8_t block = _timer_no / 3;
    qd_callbacks[block] = isr;
    qd_irq_masks[block] = bits;
    tc()->TC_QIER = bits;
  }

  return *this;
}

c_due_timer_counter &c_due_timer_counter::detach_qd_interrupt(void) {
  if (used) {
    uint8_t block = _timer_no / 3;
    stop(); // Stop the currently running timer
    qd_callbacks[block] = NULL;
    tc()->TC_QIDR = qd_irq_masks[block];
  }

  return *this;
}

uint8_t c_due_timer_counter::pins_available(void) const {
  return (td_tclk().arduino_pin_no >= 0 ? 0x01 : 0)
    | (td_tioa().arduino_pin_no >= 0 ? 0x02 : 0)
    | (td_tiob().arduino_pin_no >= 0 ? 0x04 : 0);
}

TcChannel *c_due_timer_counter::p_channel(void) const {
  return tc()->TC_CHANNEL + td->channel;
}

c_due_timer_counter &c_due_timer_counter::xc_select(ext_clock_sel_t clock_sel) {
  uint8_t channel_in_block = _timer_no % 3;

  switch (channel_in_block) {
  case 0:
    tc()->TC_BMR = (tc()->TC_BMR & ~TC_BMR_TC0XC0S_Msk) | clock_sel;
    break;

  case 1:
    tc()->TC_BMR = (tc()->TC_BMR & ~TC_BMR_TC1XC1S_Msk)
      | (clock_sel << TC_BMR_TC1XC1S_Pos);
    break;

  case 2:
    tc()->TC_BMR = (tc()->TC_BMR & ~TC_BMR_TC2XC2S_Msk)
      | (clock_sel << TC_BMR_TC1XC1S_Pos);
    break;
  }

  return *this;
}


/******************************************************************************/
/*                              interrupt handlers                            */
/******************************************************************************/

void c_due_timer_counter::irq_handler(uint32_t status) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  irq_counter2[_timer_no]++;
#endif
  // keep track of external trigger events
  if (status & TC_SR_ETRGS)
    started = true;

  if (status & irq_masks[_timer_no]) {
    // handle interrupts with registered callbacks
    if (callbacks[_timer_no])
      callbacks[_timer_no](status);
  }

  if (status & qd_irq_masks[td->channel])
    qd_callbacks[td->channel](status);
  else
    qd_irq_handler(status);
}

void TC0_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[0]++;
#endif
  uint32_t status = TC0->TC_CHANNEL[0].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[0])
    c_due_timer_counter::registered_timer_counters[0]->irq_handler(status);
}

void TC1_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[1]++;
#endif
  uint32_t status = TC0->TC_CHANNEL[1].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[1])
    c_due_timer_counter::registered_timer_counters[1]->irq_handler(status);
}

void TC2_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[2]++;
#endif
  uint32_t status = TC0->TC_CHANNEL[2].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[2])
    c_due_timer_counter::registered_timer_counters[2]->irq_handler(status);
}

void TC3_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[3]++;
#endif
  uint32_t status = TC1->TC_CHANNEL[0].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[3])
    c_due_timer_counter::registered_timer_counters[3]->irq_handler(status);
}

void TC4_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[4]++;
#endif
  uint32_t status = TC1->TC_CHANNEL[1].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[4])
    c_due_timer_counter::registered_timer_counters[4]->irq_handler(status);
}

void TC5_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[5]++;
#endif
  uint32_t status = TC1->TC_CHANNEL[2].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[5])
    c_due_timer_counter::registered_timer_counters[5]->irq_handler(status);
}

void TC6_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[6]++;
#endif
  uint32_t status = TC2->TC_CHANNEL[0].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[6])
    c_due_timer_counter::registered_timer_counters[6]->irq_handler(status);
}

void TC7_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[7]++;
#endif
  uint32_t status = TC2->TC_CHANNEL[1].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[7])
    c_due_timer_counter::registered_timer_counters[7]->irq_handler(status);
}

void TC8_Handler(void) {
#ifdef DEBUG_DUE_TIMER_COUNTER
  c_due_timer_counter::irq_counter1[8]++;
#endif
  uint32_t status = TC2->TC_CHANNEL[2].TC_SR;
  if (c_due_timer_counter::registered_timer_counters[8])
    c_due_timer_counter::registered_timer_counters[8]->irq_handler(status);
}


/******************************************************************************/
/*                                  debugging                                 */
/******************************************************************************/

#ifdef DEBUG_DUE_TIMER_COUNTER
#include "command-parser.h"

uint32_t c_due_timer_counter::irq_counter1[NUM_TIMERS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0
};

uint32_t c_due_timer_counter::irq_counter2[NUM_TIMERS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0
};

void debug_xc_select(uint8_t no, uint8_t val, c_command_parser *p) {
  switch (val) {
  case 0: p->respond("  XC%d <- TCLK", no);    break;
  case 2:
    if (no)
      p->respond("  XC%d <- TIOA0", no);
    else
      p->respond("  XC%d <- TIOA1", no);
    break;
  case 3:
    if (no < 2)
      p->respond("  XC%d <- TIOA2", no);
    else
      p->respond("  XC%d <- TIOA1", no);
    break;
  }
}

void debug_rc(uint32_t rc, const char *name, c_command_parser *p) {
  switch (rc) {
  case 1: p->respond("  %s: set", name);    break;
  case 2: p->respond("  %s: clear", name);  break;
  case 3: p->respond("  %s: toggle", name); break;
  }
}

void debug_cmr(uint32_t reg_val, c_command_parser *p) {
  uint32_t val;

  switch (reg_val & 0x07) {
  case 0: p->respond("  clock <- CLOCK1"); break;
  case 1: p->respond("  clock <- CLOCK2"); break;
  case 2: p->respond("  clock <- CLOCK3"); break;
  case 3: p->respond("  clock <- CLOCK4"); break;
  case 4: p->respond("  clock <- CLOCK5"); break;
  case 5: p->respond("  clock <- XC0");    break;
  case 6: p->respond("  clock <- XC1");    break;
  case 7: p->respond("  clock <- XC2");    break;
  }

  if (reg_val & TC_CMR_CLKI) p->respond("  inverted clock");

  val = (reg_val >> 4) & 0x03;
  if (val) p->respond("  burst XC%d", val - 1);

  if (reg_val & TC_CMR_WAVE) {
    p->respond("  wave generating mode");
    if (reg_val & TC_CMR_CPCSTOP) p->respond("  stop on CPC");

    if (reg_val & TC_CMR_CPCDIS) p->respond("  disable on CPC");

    switch ((reg_val >> 8) & 0x03) {
    case 1: p->respond("  external event on rising edge");  break;
    case 2: p->respond("  external event on falling edge"); break;
    case 3: p->respond("  external event on any edge");     break;
    }

    val = (reg_val >> 10) & 0x03;
    switch (val) {
    case 0: p->respond("  external event on TIOB"); break;
    default: p->respond("  external event on XC%d", val - 1);
    }

    if (reg_val & TC_CMR_ENETRG) p->respond("  trigger on external event");

    switch ((reg_val >> 13) & 0x03) {
    case 0: p->respond("  up");         break;
    case 1: p->respond("  up-down");    break;
    case 2: p->respond("  up-rc");      break;
    case 3: p->respond("  up-down-rc"); break;
    }

    debug_rc((reg_val >> 16) & 0x03, "ACPA", p);
    debug_rc((reg_val >> 18) & 0x03, "ACPC", p);
    debug_rc((reg_val >> 20) & 0x03, "AEEVT", p);
    debug_rc((reg_val >> 22) & 0x03, "ASWTRIG", p);
    debug_rc((reg_val >> 24) & 0x03, "BCPB", p);
    debug_rc((reg_val >> 26) & 0x03, "BCPC", p);
    debug_rc((reg_val >> 28) & 0x03, "BEEVT", p);
    debug_rc((reg_val >> 30) & 0x03, "BSWTRIG", p);
  } else {
    p->respond("  input capture mode");

    switch (reg_val & TC_CMR_ETRGEDG_Msk) {
    case TC_CMR_ETRGEDG_RISING:
      p->respond("external trigger on rising edge");
      break;
    case TC_CMR_ETRGEDG_FALLING:
      p->respond("external trigger on falling edge");
      break;
    case TC_CMR_ETRGEDG_EDGE:
      p->respond("external trigger on both edges");
      break;
    }
    if (reg_val & TC_CMR_ETRGEDG_Msk) {
      if (reg_val & TC_CMR_ABETRG) p->respond("of TIOA");
      else p->respond("of TIOB");
    }

    if (reg_val & TC_CMR_CPCTRG) p->respond("reset on CPC");

    switch (reg_val & TC_CMR_LDRA_Msk) {
    case TC_CMR_LDRA_RISING:   p->respond("RA on rising edge"); break;
    case TC_CMR_LDRA_FALLING:  p->respond("RA on falling edge"); break;
    case TC_CMR_LDRA_EDGE:     p->respond("RA on both edges"); break;
    }

    switch (reg_val & TC_CMR_LDRB_Msk) {
    case TC_CMR_LDRB_RISING:   p->respond("RB on rising edge"); break;
    case TC_CMR_LDRB_FALLING:  p->respond("RB on falling edge"); break;
    case TC_CMR_LDRB_EDGE:     p->respond("RB on both edges"); break;
    }
  }
}

void c_due_timer_counter::debug(c_command_parser *p) {
  p->respond("=== Timer block %d, channel %d ===", _timer_no / 3, channel());
  if (is_enabled()) {
    if (is_started()) p->respond("started");
    else p->respond("enabled");
  } else
    p->respond("stopped");

  p->respond("TC_BMR: 0x%08x", tc()->TC_BMR);
  debug_xc_select(0, tc()->TC_BMR & 0x03, p);
  debug_xc_select(1, (tc()->TC_BMR >> 2) & 0x03, p);
  debug_xc_select(2, (tc()->TC_BMR >> 4) & 0x03, p);
  p->respond("TC_CMR: 0x%08x", p_channel()->TC_CMR);
  debug_cmr(p_channel()->TC_CMR, p);
  p->respond("TC_RA:  0x%08x", p_channel()->TC_RA);
  p->respond("TC_RB:  0x%08x", p_channel()->TC_RB);
  p->respond("TC_RC:  0x%08x", p_channel()->TC_RC);
  p->respond("TC_SR:  0x%08x", p_channel()->TC_SR);
  p->respond("TC_IMR: 0x%08x", p_channel()->TC_IMR);
  p->respond("TC_CV:  0x%08x", p_channel()->TC_CV);
  p->respond("icnt: %d, %d", irq_counter1[_timer_no], irq_counter2[_timer_no]);
}

uint32_t c_due_timer_counter::debug_counter1(void) const {
  return irq_counter1[_timer_no];
}

uint32_t c_due_timer_counter::debug_counter2(void) const {
  return irq_counter2[_timer_no];
}

#endif // DEBUG_DUE_TIMER_COUNTER
