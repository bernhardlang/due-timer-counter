/* due-timer-counter.h
 *
 * Copyright (C) 2018-2024 Bernhard Lang, www.bernhard-lang.net
 *
 * Based on code by Dominique Lovy, Université de Genève, May 2014, Nov. 2016
 * and on code from DueTimer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef __arm__
# error Oops! Trying to include due_clock on another device?
#endif

#ifndef _due_timer_counter_h_
#define _due_timer_counter_h_

class c_command_parser;

#include "Arduino.h"
#include "due-pio.h"

#define NUM_TIMER_BLOCKS 3
#define NUM_TIMERS       9

#define qd_mask                                                \
  ( TC_BMR_QDEN | TC_BMR_POSEN | TC_BMR_EDGPHA | TC_BMR_FILTER \
    | TC_BMR_MAXFILT(0x0f) )
// EDGEPHA = 1 -> 4 cnts, EDGEPHA = 0 -> 2 cnts !!!!  (doc. error)

//! Basic timer and counter functions of the SAM3X in the Arduino Due.
/*! The SAM3X8 contains nine timer-counter units which are grouped together
    in three timer blocks.
    Each block goups together three related counters.
    The Atmel manual is unfortunately not very clear about the hardware
    organsation.
    Each timer channel can be connected to
      - one of five internal clocks
        - TIMER_CLOCK1 = master clock / 2
        - TIMER_CLOCK2 = master clock / 8
        - TIMER_CLOCK3 = master clock / 32
        - TIMER_CLOCK4 = master clock / 128
        - TIMER_CLOCK5 = slow clock
      - to one of the three external clock signals XC0-2, which are conected to
        TCKL0-2, TCLK3-5 respectively TCLK6-8 for the three counter blocks
      - to the TIOA0-2 signals of the counter block throuth XC0 for the first,
        XC1 for the second and XC2 for the thrid channel of a block. Accessible
        are the TIOA lines of the other two counters in the same block.

    c_due_timer_counter provides a table which describes the routing of the
    signals to the SAM3X, respectively Arduino Due pins.
    It contains basic functionality for starting, stopping and configuring the
    counters.
    For interrupt handling two mechanisms are provided.
    A callback function may be attached in the Arduino fashion.
    When no such function is attached, a virtual handler method is called which
    can be overridden in derived classes.

    c_due_timer_counter is the base of different derived classes using the
    timer-counter hardware.
    - c_due_counter and c_due_qd_counter count edges on external clock signals.
    - c_due_clock produces pulse sequences based in internal clock signals.
    - c_due_delay implements event-triggeres single-shot timers.
 */
class c_due_timer_counter {
 public:
  typedef enum {
    timer0=0, timer1=1, timer2=2, timer3=3, timer4=4, timer5=5, timer6=6,
    timer7=7, timer8=8
  } timer_no_t;

  typedef enum { timer_block0=0, timer_block1=1, timer_block2=2 }
  timer_block_no_t;

  //! Description of a counter channel and links to Arduino pin numbering.
  typedef struct {
    Tc *tc;               // timer block pointer TC0 | TC1 | TC2
    uint8_t channel;      // timer channel  0 | 1 | 2
    IRQn_Type irq;        // IRQ type
    c_due_pio::pin_descriptor_t tclk;
    c_due_pio::pin_descriptor_t tioa;
    c_due_pio::pin_descriptor_t tiob;
  } due_timer_descr_t ;

  //! Bits of the TC status register.
  /*! Bits 0-7 are reset upon read acces to the status register.
      They report thus events which occured since the last reading of the
      register. */
  typedef enum {
    //! Bit 0 • COVFS: Counter Overflow Status
    COVFS=(1<<0),
    //! Bit 1 • LOVRS: Load Overrun Status, RA or RB overwritten befor having been read
    LOVRS=(1<<1),
    //! Bit 2 • RA Compare Status. RA compare occured
    CPAS=(1<<2),
    //! Bit 3 • RB Compare Status. RB compare occured
    CPBS=(1<<3),
    //! Bit 4 • RC Compare Status. RC compare occured
    CPCS=(1<<4),
    //! Bit 5 • LDRAS: RA loaded
    LDRAS=(1<<5),
    //! Bit 6 • LDRBS: RB loaded
    LDRBS=(1<<6),
    //! Bit 7 • ETRGS: external trigger occured
    ETRGS=(1<<7),
    //! Bit 16 • CLKSTA: Clock Enabling Status, 0/1: clock disabled/enabled
    CLKSTA=(1<<16),
    //! Bit 17 • MTIOA: TIOA Mirror, actual state of the TIOA signal
    MTIOA=(1<<17),
    //! Bit 18 • MTIOB: TIOB Mirror, actual state of the TIOB signal
    MTIOB=(1<<18)
  } state_bits_t;

  typedef enum { TIOA=0x01, TIOB=0x02 } tioab_t;

  //! internal timer-counter clock sources
  typedef enum {
    //! MCK/2
    CLOCK_SEL_CLOCK1=TC_CMR_TCCLKS_TIMER_CLOCK1,
    //! MCK/8
    CLOCK_SEL_CLOCK2=TC_CMR_TCCLKS_TIMER_CLOCK2,
    //! MCK/32
    CLOCK_SEL_CLOCK3=TC_CMR_TCCLKS_TIMER_CLOCK3,
    //! MCK/128
    CLOCK_SEL_CLOCK4=TC_CMR_TCCLKS_TIMER_CLOCK4,
    //! slow clock
    CLOCK_SEL_CLOCK5=TC_CMR_TCCLKS_TIMER_CLOCK5,
    //! XC0
    CLOCK_SEL_XC0=TC_CMR_TCCLKS_XC0,
    //! XC1
    CLOCK_SEL_XC1=TC_CMR_TCCLKS_XC1,
    //! XC2
    CLOCK_SEL_XC2=TC_CMR_TCCLKS_XC2
  } clock_select_t;

  typedef enum {
    //! no trigger
    EDGE_NONE=0,
    //! trigger on rising edges.
    EDGE_RISING=1,
    //! trigger falling edges.
    EDGE_FALLING=2,
    //! trigger on all edges.
    EDGE_ALL=3
  } edge_t;

  //! Selection of and external timer clock signal.
  typedef enum {
    TCLK=0,
    TIOA01=2,
    TIOA02=3,
    TIOA10=2,
    TIOA12=3,
    TIOA20=2,
    TIOA21=3,
  } ext_clock_sel_t;

  typedef enum {
    EXT_TIOB=0,
    EXT_XC0=1,
    EXT_XC1=2,
    EXT_XC2=3
  } ext_event_t;

  typedef enum {
    BURST_NONE=0,
    BURST_XC0=1,
    BURST_XC1=2,
    BURST_XC2=3
  } burst_select_t;

  typedef enum {
    ACTION_NONE=0,
    ACTION_SET=1,
    ACTION_CLEAR=2,
    ACTION_TOGGLE=3
  } action_t;

  typedef enum { TRIGGER_AB, TRIGGER_C, TRIGGER_EXT } trigger_t;

  friend class c_due_timer_block;

  //! Instantiates the given timer-counter.
  /*! Internal variables are initialised.
      No hardware is accessed yet.
   */
  c_due_timer_counter(timer_no_t counter_no);

  //! Claim and initialise the hardware.
  /*! The peripheral clock of the channel is activated and the attached
      interrupt handler, if there is any, is removed.
      The timer is stopped if it was running.
      A pointer in a global table is set to the actual instance so that
      the static primary interrupt handler may call overloaded handler methods.
      This table serves also as a locking mechanism.
      Only one object derived from c_due_timer_counter per channel may be
      active at a time.

      The argument may be set to one of the TIOA/B lines or both ored together.
      The corresponding output pin as programmed to be controlled by the counter.
   */
  int8_t begin(uint8_t use_tioab=0);

  //! Release the counter.
  /*! The instance is deregistered from the global counter pointer table.
      Note that no hardware interaction is performed.
      The timer remains in the same state.
      Especially, if it is running, it continues to run.
   */
  void end(void);

  //! Start the timer channel as previously configured.
  /*! All pending interrupts are cleared before.
      The method returns false if the timer-counter unit has not been registered
      calling begin() or when the timer-counter has already been started.
  */
  c_due_timer_counter &start(void);

  //! Stop the timer channel.
  /*! Interrupts are disabled.
      The method does nothing if the timer-counter unit has not been registered
      calling begin().
  */
  c_due_timer_counter &stop(void);

  //! Trigger the timer to start counting from zero.
  c_due_timer_counter &trigger(void);

  //! Set timer register to zero.
  c_due_timer_counter &clear(void);
  
  //! Synchronise channels.
  /* Assert the sync signal which generates a software trigger simultaneously
     on all three timer channels of the block the timer channel is residing in.
  */
  c_due_timer_counter &sync(void);

  //! Enable the timer without starting it.
  c_due_timer_counter &enable(bool);

  //! Return the content of the timer channel status register.
  uint32_t get_status(void) const;

  //! Register an interrupt handler.
  /*! isr is registered as an interrupt handler and the interrupts given in bits
      are activated.
      The interrupt handler is called upon an interrupt request with at least
      one of the given bits being active.
      If an interrupt handler has been attached previously it is detached
      first.
      The method returns false if the timer-counter unit has not been registered
      calling begin().
   */
  c_due_timer_counter &attach_interrupt(uint32_t bits,
					void (*isr)(uint32_t tc_status_reg));

  //! Deregister interrupt handler.
  /*! An interrupt handler attached before is deregistered and the corresponding
      interrupts are disabled.
      The method returns false if the timer-counter unit has not been registered
      calling begin().
  */
  c_due_timer_counter &detach_interrupt(void);

  //! Attach an interrupt handler for the quadrature counter.
  /*! isr is registered as an interrupt handler and the interrupts given in bits
      are activated.
      The interrupt handler is called upon an interrupt request with at least
      one of the given bits being active.
      If an interrupt handler has been attached previously it is detached
      first.
      The method returns false if the timer-counter unit has not been registered
      calling begin().
   */
  c_due_timer_counter &attach_qd_interrupt(uint32_t bits,
					   void (*isr)(uint32_t qd_status_reg));

  //! Deregister quadrature counter interrupt handler.
  /*! An interrupt handler attached before is deregistered and the corresponding
      interrupts are disabled.
      The method returns false if the timer-counter unit has not been registered
      calling begin().
   */
  c_due_timer_counter &detach_qd_interrupt(void);

  bool is_enabled(void) const;

  bool is_started(void) const { return started; };

  //! Returns true after the begin method has been invoked.
  bool is_used(void) const { return used; }; 

  //! Return the actual counter value.
  uint32_t counter_value(void) const;

  //! Return the arduino pin number of the TCLK pin belonging to the channel.
  /*! no_pin is returned in case the line is not routed to an arduino pin. */
  pin_no_t tclk_pin_no(void) const;

  //! Program the function of the TIOA/B lines.
  /*! The method provides direct access to the channel mode register TC_CMRn.
      tioab tells the line to be programmed.
      Both can be programmed at the same time.
      The action argument ACTION_SET, ACTION_CLEAR or ACTION_TOGGLE for
      set/clear/toggle tioa/b line on compare match.
      By default the rc value is wrtten to RA respectively RB.
      On is_rc true the rc value is written to the RC register.
   */
  c_due_timer_counter &set_out(uint8_t tioab, uint32_t rc, action_t action,
			       bool is_rc=false);

  //! Select action on tioa/b upon external event.
  c_due_timer_counter &set_out_extern(uint8_t tioab, action_t action);

  //! Select action on tioa/b upon software trigger.
  c_due_timer_counter &set_out_soft(uint8_t tioab, action_t action);

  //! Select source of external event.
  c_due_timer_counter &select_external_event(ext_event_t, edge_t);

  //! Enable external events.
  c_due_timer_counter &enable_external_event(bool);

  bool external_events_enabled(void) const;
  
  //! Select burst signal.
  c_due_timer_counter &burst(burst_select_t);

  //! Stop counter on rc compare.
  c_due_timer_counter &stop_on_rc(bool);

  //! Disable counter on rc compare.
  c_due_timer_counter &disable_on_rc(bool);

  //! Select the source of the external clock.
  /*! The external clock XCn of channel n (counted from 0 to 2 within the block)
      can be connected to the TCLK line corresponding to the timer channel or
      to the TIOA lines of the other two channels within the same block.
   */
  c_due_timer_counter &xc_select(ext_clock_sel_t clock_sel);

#ifdef DEBUG_DUE_TIMER_COUNTER
  void debug(c_command_parser *);
  uint32_t debug_counter1(void) const;
  uint32_t debug_counter2(void) const;
#endif

 protected:
  friend void TC0_Handler(void);
  friend void TC1_Handler(void);
  friend void TC2_Handler(void);
  friend void TC3_Handler(void);
  friend void TC4_Handler(void);
  friend void TC5_Handler(void);
  friend void TC6_Handler(void);
  friend void TC7_Handler(void);
  friend void TC8_Handler(void);

  //! Configure the timer channel.
  /*! If the cannel is running it is stopped before and restarted after
    configuring. */
  void configure(uint32_t channel_mode_bits);

  virtual void irq_handler(uint32_t);
  virtual void qd_irq_handler(uint32_t) {};

  uint32_t irq(void) const { return td->irq; };
  timer_no_t timer_no(void) const { return _timer_no; };
  uint8_t pins_available(void) const;
  Tc *tc(void) const { return td->tc; };
  TcChannel *p_channel(void) const;
  uint8_t channel(void) const { return td->channel; };
  const c_due_pio::pin_descriptor_t &td_tclk(void) const { return td->tclk; };
  const c_due_pio::pin_descriptor_t &td_tioa(void) const { return td->tioa; };
  const c_due_pio::pin_descriptor_t &td_tiob(void) const { return td->tiob; };

 protected:
  static const due_timer_descr_t timer_counter[NUM_TIMERS];
  static void (*callbacks[NUM_TIMERS])(uint32_t tc_status_reg);
  static uint32_t irq_masks[NUM_TIMERS];
  static c_due_timer_counter *registered_timer_counters[NUM_TIMERS];
  static void (*qd_callbacks[NUM_TIMER_BLOCKS])(uint32_t tc_status_reg);
  static uint32_t qd_irq_masks[NUM_TIMER_BLOCKS];
  timer_no_t _timer_no;
  const due_timer_descr_t *td;
  bool used, external_trigger_interrupt_disabled;
  volatile bool started;
  uint8_t tioab_pin_used;
#ifdef DEBUG_DUE_TIMER_COUNTER
  static uint32_t irq_counter1[NUM_TIMERS], irq_counter2[NUM_TIMERS];
#endif
};

#define set_reg(reg, mask, val) ((reg) = ((reg) & ~(mask)) | (val & (mask)))

#endif // _due_timer_counter_h_
