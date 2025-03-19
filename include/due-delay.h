/*
 * due-delay.h
 *
 * Copyright (C) 2018-2022 Bernhard Lang, www.bernhard-lang.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef __arm__
# error Oops! Trying to include due_counter on a non-arm device?
#endif

#ifndef _due_delay_h_
#define _due_delay_h_

#ifdef _due_clock_h_
# error due-clock.h must be included after due-delay.h
#endif

#include "due-clock.h"

//! Use the SAM3X timers for generating triggerable delays.
/*! The delay can be triggered either by software or by an external trigger
    signal on the channel's TIOB line or on one of the timer block's
    TCLK lines.
 */
class c_due_delay: public c_due_clock {
 public:
  typedef enum { irq_rca=0, irq_rcb=1, irq_rcc=2 } irq_select_t;

  //! Initialize internal structure.
  c_due_delay(timer_no_t counter_no);

  //! Claim the hardware and select the internal clock.
  int8_t begin(uint8_t tioab, ext_event_t trigger_select, edge_t edge);

  //! Program delay and pulse length.
  /*! RA and RB are used to generate the first edge on TIOA and TIOB,
      respectively.
      RC is used for the second edge.
      When TIOA and TIOB are used at the same time, the second edge is shared.
      Note that when programming TIOA and TIOB separately, the second edge will
      be for both TIOA and TIOB the one last programmed.
      The first gets overwritten.
  */
  c_due_delay &set_pulse(uint8_t tioab, double pulse_start, double pulse_length);
  c_due_delay &set_start_stop(uint8_t tioab, double pulse_start,
			      double pulse_end);

  //! Program a delay length.
  /*! Only the first edge of the pulse is programmed.
      \sa set_pulse().
   */
  c_due_delay &set_delay(uint8_t tioab, double pulse_start);

  c_due_delay &set_action(uint8_t tioab, trigger_t trigger, action_t action);

  //! Returns the delay in seconds programmed on the given TIOA/B line.
  double delay(tioab_t) const;

  //! Returns the pulse duration in seconds on the given TIOA/B line.
  double pulse_length(tioab_t) const;

  //! Returns the selection of the external trigger.
  /*! \sa begin(). */

  ext_event_t trigger_select(void) const { return external_trigger_select; };

  //! Attaches a callback function to the register compare interrupt.
  /*! The function is called upon the rising or falling edge of either TIOA
      or TIOB.
      This permits to perform actions following one or several of those edges.
   */
  c_due_delay &attach_pulse_interrupt(irq_select_t,
                                      void (*isr)(uint32_t status));

  c_due_clock &set_frequency(double frequency);
  c_due_delay &set_retrigger(bool);
  c_due_delay &start(void);
  c_due_delay &stop(void);

#ifdef DEBUG_DUE_TIMER_COUNTER
  uint32_t debug_counter(void) const { return irq_counter; };
  uint32_t debug_status(void) const { return irq_status; };
  void debug(c_command_parser *);
#endif

 private:
  void irq_handler(uint32_t);
  uint8_t isr_bit(irq_select_t irq) const;
  friend int8_t debug(uint8_t, char **argv, c_command_parser *p);

  static const uint32_t delay_scaling;
  uint32_t tioa_bits, tiob_bits;
  ext_event_t external_trigger_select;
  bool retrigger;
  double inv_mck_div;
#ifdef DEBUG_DUE_TIMER_COUNTER
  uint32_t irq_counter, irq_status;
#endif
};

extern c_due_delay due_delay1;
extern c_due_delay due_delay2;
extern c_due_delay due_delay3;
extern c_due_delay due_delay4;
extern c_due_delay due_delay5;
extern c_due_delay due_delay6;
extern c_due_delay due_delay7;
extern c_due_delay due_delay8;
extern c_due_delay due_delay9;
extern c_due_delay *due_delays[9];

// dirty hack!!
#define c_due_clock1 c_due_delay1
#define c_due_clock2 c_due_delay2
#define c_due_clock3 c_due_delay3
#define c_due_clock4 c_due_delay4
#define c_due_clock5 c_due_delay5
#define c_due_clock6 c_due_delay6
#define c_due_clock7 c_due_delay7
#define c_due_clock8 c_due_delay8
#define c_due_clock9 c_due_delay9
#define c_due_clocks c_due_delays
#define __no_clock_instances__


#endif // _due_delay_h
