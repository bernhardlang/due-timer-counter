/*
 * due-clock.h - Use Arduino Due TC timers/counters as clocks on output pins
 *
 * Copyright (C) 2018-2022 Bernhard Lang, www.bernhard-lang.net
 * 
 * Based on code by Dominique Lovy, Université de Genève, May 2014
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef __arm__
# error Oops! Trying to include due_clock on another device?
#endif

#ifndef _due_clock_h_
#define _due_clock_h_

#include "Arduino.h"
#include "due-timer-counter.h"

/*
TCCLK#  pin  TIOA  TIOB
TCCLK0, D22   D2    D13
TCCLK1, A5    A7    A6
TCCLK2, D31   --    A4
TCCLK3, A3    --    --
TCCLK4, A2    --    --
TCCLK5, DAC1  --    --
TCCLK6, --    D5    D4
TCCLK7, --    D3    D10
TCCLK8, D30   D11   D12
*/

#define NOTCOUTMSK    0xffffffff

//! Class for generating pulse sequences using the internal clock signals.
/*! 
 */
class c_due_clock: public c_due_timer_counter {
public:
  //! pulse length unit
  typedef enum {
    //! fixed unit in TC count
    none=0,
    //! percent fraction of pulse length
    percent=1,
    //! variable 1/10000 fraction of pulse length
    pertenm=2,
    //! variable per million of pulse length
    ppm=3,
    //! fixed in microseconds
    micros=4,
    //! fixed in milliseconds
    millis=5,
    //! fixed in seconds
    seconds=6,
    //! no need to explain
    undefined=9
  } pulse_unit_t;

  typedef enum { qd_forward, qd_backward } qd_direction_t;

  typedef enum {
    //! Positive pulse
    normal,
    //! Negative pulse
    inverted,
    //! Toggle at each occurrence
    toggle
  } tio_mode_t;

  //! Create a clock and ask for TIOA or TIOB or both output pins
  c_due_clock(timer_no_t timer_no);

  //! Initialise the associated counter hardware.
  /*! The peripheral clock of the associated counter is enabled.
      When specified in \a use_tioab, TIOA and TIOB are connected to the
      corresponding pins through the PIO controllers in charge of the pins.
      Note that the activation only takes place if the corresponding pin is
      routed to a connector on the arduino due board.
      By default the output is set to a 50% duty cycle pulse.
      The return value indicates the used TIOA/B signals according to the
      function argument and the availability on the arduino due.
   */
  int8_t begin(uint8_t use_tioab, double frequency=-1.0);

  //! Set the clock frequency.
  /*! The method chooses the closest possible frequency for the clock signal
      (min 7.6e-6 Hz, max 21 MHz).
      The closer the demanded frequency gets to the maser clock, the coarser
      gets the distribution of possible frequencies.
      The actual frequency can be read from get_frequency().
  */
  c_due_clock &set_frequency(double frequency);

  //! Set pulse period in microseconds.
  /*! This method behaves essentially like set_frequency(). */
  c_due_clock &set_period(long microseconds);

  //! Get the actual clock frequency.
  /*! Returns zero when the timer is not started. */
  double get_frequency(void) const;

  //! Set both TIOA and TIOB to the same output mode.
  c_due_clock &set_out(pulse_unit_t pulse_unit, uint32_t pulse_length=1,
                       tio_mode_t t=normal);

  //! Set output mode of single line. \overloaded
  c_due_clock &set_out(uint8_t tioab, pulse_unit_t pulse_unit,
                       uint32_t pulse_length=1, tio_mode_t t=normal);

  //! Set TIOA output mode.
  c_due_clock &set_out_a(pulse_unit_t pulse_unit, uint32_t pulse_length=1,
                         tio_mode_t t=normal);

  //! Set TIOB output mode.
  c_due_clock &set_out_b(pulse_unit_t pulse_unit, uint32_t pulse_length=1,
                         tio_mode_t t=normal);

  //! Return output TIOA/B mode.
  pulse_unit_t get_out(uint8_t tioab) const;

  //! Configure the counter to generate a quadrature signal on TIOA and TIOB.
  /*! The counter is set to run at twice the configured frequency.
      Each time the counter is set back to zero by an RC compare event, TIOA is
      toggled.
      TIOB is toggled on RB compare which is set to half the oscillation time.
      The \a forward controls the direction.
      When true, the signal on TIOB lags behind the signal on TIOA by a quarter
      period.
      When false, the signal on TIOB is ahead of the signal on TIOA by a quarter
      period.
      \ref set_quad may be called when the counter is already started in
      quadrature mode to invert the direction.
      The use of both TIOA and TIOB line has to be declared when calling
      \ref begin().
      Otherwise and on a timer channel where one of the TIOA/B lines is missing,
      the method does nothing.
   */
  c_due_clock &set_quad(qd_direction_t direction=qd_forward,
			double frequency=-1);

  // Return qd direction
  qd_direction_t qd_direction(void) const;

  //! Return RC value corresponding to TIOA/B mode.
  uint32_t get_length(uint8_t tioab) const;

  //! Start pulse generation.
  /*! Start the clock to generate a train of pulses.
      When \a n_pulses is zero, the timer is started in free running mode.
      With \a n_pulses > 0 the counter CPC interrupt is used for counting
      the cycles.
      Any interrupt handler previously attached to the timer-counter channel is
      detached in that mode.
   */
  void start(uint32_t n_pulses=0);
  //void start(void) override;

  //! Return the number of pulses until the timer will stop.
  /*! A zero value indicates that the counter has to be stopped manually.
      \sa start().
   */
  uint32_t get_pulse_count(void) const { return pulse_count; };

 protected:
  //! Default interrupt handler.
  /*! This handler is used to stop the counter when started with a positive
     number as an argument.
     The handler may be overloaded in derived class.
     However, for not breaking c_due_clock's mechanism, such overloaded
     handler should call this handler every time it is invoked.
     \sa start(), get_pulse_count().
   */
  void irq_handler(uint32_t);

  typedef struct {
    pulse_unit_t length_unit;
    uint32_t pulse_length;
    uint32_t channel_mode_bits;
  } out_par_t;

  typedef enum {
    //! Generate quadrature signal using both TIOA and TIOB
    quad=0x01,
    //! Quadrature signal is forward
    quad_forward=0x02,
    //! Used for grouping configuration updates together
    delayed_update=0x10
  } mode_t;

  //! Calculate RC needed register values.
  uint32_t get_pulse_len_tc(const out_par_t *out_def, uint32_t ntc);
  void update_registers(void);

 protected:
  uint32_t mck_div, rc_val, cmr_clock_bits;
  volatile uint32_t pulse_count;
  double demanded_frequency, effective_frequency;
  out_par_t outa, outb;
  uint16_t cmr_bits;
  uint8_t mode_bits;
};

//! Instantiate all available clocks.

#ifndef __no_clock_instances__
extern c_due_clock due_clock1;
extern c_due_clock due_clock2;
extern c_due_clock due_clock3;
extern c_due_clock due_clock4;
extern c_due_clock due_clock5;
extern c_due_clock due_clock6;
extern c_due_clock due_clock7;
extern c_due_clock due_clock8;
extern c_due_clock due_clock9;
extern c_due_clock *due_clocks[9];
#endif // instantiate clocks

#define  DC_FRMIN  7.629e-6    // not tested :---)))

#endif  // #ifndef _due_clock_h
