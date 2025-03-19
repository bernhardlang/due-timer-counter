/*
 * due-counter.h - Arduino Due hardware counters
 *
 * Copyright (C) 2018-2022 Bernhard Lang, www.bernhard-lang.net
 * 
 * Based on code by Dominique Lovy, Université de Genève, May 2014, Nov. 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef __arm__
# error Oops! Trying to include due_counter on a non-arm device?
#endif

#ifndef _due_counter_h
#define _due_counter_h

#include "Arduino.h"
#include "due-timer-counter.h"

//! Abstract counter class providing common functionality.
/*! The class provides a mechanism to call a method on register compare.
    This can for instance be used for calling a method in regular time or
    counting intervals.
 */
class c_abstract_due_counter: public c_due_timer_counter {
 public:
  //! Enumerates compare registers
  enum rc_register {
    //! RA
    rc_a=TC_SR_CPAS,
    //! RB
    rc_b=TC_SR_CPBS,
    //! RC
    rc_c=TC_SR_CPCS
  };

 protected:
  //! Instantiate an abstract due counter.
  c_abstract_due_counter(timer_no_t counter_no);

 public:
  //! Initiate a register compare.
  /*! The indicated compare register is loaded with the given value and
      the corresponding compare interrupt is activated.
      Upon an interrupt the virtual \ref method on_register_compare() is called.
      It has to be overloaded in a derived class to perform an action.
      The default implementation returns without doing anything.
   */
  c_abstract_due_counter &set_register_compare(rc_register reg_no,
					       uint32_t value);

  //! Stop a before initiated register compare.
  /*! The corresponding interrupt is disabled. */
  c_abstract_due_counter &stop_register_compare(rc_register reg_no);

  //! Return the value of the indicated compare register.
  uint32_t get_rc(rc_register reg_no) const;

 protected:
  void irq_handler(uint32_t);

  //! Called upond register compare interrupt.
  /*! The default implementation does nothing.
      Funtionality has to be implemented in a derived class which overloads
      the default method.
      The overloaded method will be called in interrupt context.
  */
  virtual void on_register_compare(uint32_t status) {};

 private:
  uint8_t rc_registers;
};

//! Counts edges on the corresponding TCLK input.
class c_due_counter: public c_abstract_due_counter {
 public:
  //! Instantiate counter on channel timer1-9.
  c_due_counter(timer_no_t counter_no);

  //! Configure the hardware to count external edges.
  int8_t begin(edge_t edge=EDGE_RISING, uint8_t use_tioab=0);

  //! Configure the hardware to count the edges of an internal clock signal.
  int8_t begin(clock_select_t clock_select, uint8_t use_tioab=0);
};

//! A class which uses the SAM3X quadrature counter features.
class c_due_qd_counter: public c_abstract_due_counter {
public:
  //! Instantiate a quadrature counter, possible blocks on due are 0 and 2 */
  c_due_qd_counter(timer_block_no_t timer_block, bool with_idx=false);

  /* filter out pulses shorter than filter * main-clock,
     filter>63: no filtering */
  int8_t begin(uint8_t filter=0xFF);

  //! Starts the QD counter if it is used and clears its status.
  c_due_qd_counter &start(void);

  //! Stops the QD counter.
  c_due_qd_counter &stop(void);

  //! Reset the counter.
  /*! The offset is set to zero. */
  c_due_qd_counter &reset(void);

  //! Returns the QD position corresponding to pulse sequences occurres since the last rest.
  int32_t get_index_count(void) const;

  //! Sets the position of the counter using a soft offset.
  void set_index_count(int32_t);

  bool direction(void) const;

 private:
  const due_timer_descr_t *ctrd;
  bool with_idx;
  int32_t offset;
};


extern c_due_counter due_counter1;
extern c_due_counter due_counter2;
extern c_due_counter due_counter3;
extern c_due_counter due_counter4;
extern c_due_counter due_counter5;
extern c_due_counter due_counter6;
extern c_due_counter due_counter7;
extern c_due_counter due_counter8;
extern c_due_counter due_counter9;
extern c_due_counter *due_counters[9];
extern c_due_qd_counter due_qd_counter1;
extern c_due_qd_counter due_qd_counter2;
extern c_due_qd_counter *due_qd_counters[2];

#endif // _due_counter_h
