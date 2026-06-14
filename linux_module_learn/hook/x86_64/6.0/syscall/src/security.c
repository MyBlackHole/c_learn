// SPDX-License-Identifier: GPL-2.0-only
/*
 * security.c - concurrency safety, unload safety, leak prevention
 *
 * Provides the in-flight event counter and the unloading gate so
 * that the module can be removed safely without use-after-free.
 */

#define pr_fmt(fmt) "hw: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "flipswitch.h"

extern struct hw_state hw_state;

/*
 * hw_security_event_begin - called at the start of every event record.
 * Increments the in-flight counter after checking the unloading flag.
 */
void hw_security_event_begin(void)
{
	atomic_inc(&hw_state.in_flight);
	smp_mb__after_atomic();
}

/*
 * hw_security_event_end - called after every event record completes.
 * Decrements the in-flight counter.
 */
void hw_security_event_end(void)
{
	atomic_dec(&hw_state.in_flight);
}

void hw_security_init(struct hw_state *state)
{
	atomic_set(&state->in_flight, 0);
	state->unloading = false;
}

/*
 * hw_security_wait_drain - prevent new events and wait for in-flight ones.
 *
 * Sets the unloading flag so new events are rejected (checked in
 * hw_record_event), issues a memory barrier, then busy-waits until
 * all in-flight events complete.  Must be called before restoring
 * FlipSwitch patches so that no hook handler is running while we
 * modify the dispatch code.
 */
void hw_security_wait_drain(struct hw_state *state)
{
	state->unloading = true;
	smp_mb();

	while (atomic_read(&state->in_flight) > 0) {
		/*
		 * Each event record is a few microseconds at most.
		 * A short sleep prevents busy-spinning while we wait
		 * for the last few events to drain.
		 */
		usleep_range(100, 200);
	}

	/*
	 * After all in-flight events have completed, issue an RCU
	 * grace period to ensure no stale references exist anywhere.
	 */
	synchronize_rcu();
}

void hw_security_exit(struct hw_state *state)
{
	state->unloading = false;
	atomic_set(&state->in_flight, 0);
}
