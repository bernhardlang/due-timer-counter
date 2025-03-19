/*
 * due-capturer.h - Use Arduino Due TC timers/counters for time-tagging events
 *
 * Copyright (C) 2019-2022 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef __arm__
# error Oops! Trying to include due_capturer on another device?
#endif

#ifndef _due_capturer_h_
#define _due_capturer_h_

#include "Arduino.h"
#include "due-timer-counter.h"
#include "ring-buffer.h"

#ifdef DEBUG_DUE_TIMER_COUNTER
class c_command_parser;
#endif

//! Class for time-tagging external events
/*! This class implements the caputure mode of the SAM3X's timer counter.
    A sequence of time tags is generated for events on the timer's TIOA input.
    Five instances are accessible as global variables due_capturer1-5,
    corresponding to those five channels with a TIOAn line being routed to a
    pin on the Arduino Due board.

    capturer      | timer channel | arduino input pin
    --------------|:-------------:|:-----------------:
    due_capturer1 | timer 0       | D2
    due_capturer2 | timer 1       | A7
    due_capturer3 | timer 6       | D5
    due_capturer4 | timer 7       | D3
    due_capturer5 | timer 8       | D11

    Positive edges on TIOAn are captured in the RA register, negative edges in
    the RB register.
    Capturing is enabled always on both RA and RB so that consecutive events
    can be captured without retriggering the timer inbetween.
    The method prepare allows to asign a ringbuffer to RA or RB or both in
    which the events will be stored.
 */
class c_due_capturer: public c_due_timer_counter {
 public:
  typedef enum { rc_a=1, rc_b=2 } rc_select_t;

  typedef struct {
    uint32_t time;
    uint8_t id;
  } capture_t;

  //! Create a capturer instance.
  c_due_capturer(timer_no_t timer_no);

  //! Initialise the associated counter hardware.
  int8_t begin(clock_select_t clock_select, bool with_trigger=false);

  //! Prepares capturing to a buffer or via derived class.
  /*! At present two methods are provided for capturing an input signal.
      When calling this method with \a buffer being not NULL, the time tags
      will be written to that buffer.
      \a r_ab selects which register, RA or RB, will be read.
      When passing a NULL argument to \ref prepare, the virtual method
      \ref on_capture_a or \ref on_capture_b will be called upon a capture
      interrupt.
      The default implementation of these methods is to return immediately.
      Funtionality has to be provided in a derived class.
   */
  c_due_capturer &prepare(rc_select_t r_ab,
			  t_ring_buffer<uint32_t> *buffer);

  c_due_capturer &prepare(uint8_t id, rc_select_t r_ab,
                          t_ring_buffer<capture_t> *buffer);

  //! Start capturing.
  c_due_capturer &start(uint32_t max=0, uint32_t first=TC_CMR_LDRA_RISING,
			uint32_t second=TC_CMR_LDRB_FALLING);

#ifdef DEBUG_DUE_TIMER_COUNTER
  void debug(c_command_parser *p);
#endif

 protected:
  void irq_handler(uint32_t);
  //! Event handler which does nothing in the default implementation.
  virtual void on_capture_a(void) {};
  //! Event handler which does nothing in the default implementation.
  virtual void on_capture_b(void) {};

  //uint32_t cmr_bits;
  t_ring_buffer<uint32_t> *buffer_a, *buffer_b;
  t_ring_buffer<capture_t> *id_buffer_a, *id_buffer_b;
  uint32_t capture_counter, capture_max;
  uint8_t id_a, id_b;
};

//! Instantiate all available clocks.

extern c_due_capturer due_capturer1;
extern c_due_capturer due_capturer2;
extern c_due_capturer due_capturer3;
extern c_due_capturer due_capturer4;
extern c_due_capturer due_capturer5;
extern c_due_capturer *due_capturers[5];

#endif  // #ifndef _due_capturer_h
