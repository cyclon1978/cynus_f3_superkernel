/*
 * Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Released under the General Public License (GPL).
 *
 * This file contains the spinlock/rwlock implementations for
 * DEBUG_SPINLOCK.
 */

#include <linux/spinlock.h>
#include <linux/nmi.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/delay.h>
#include <linux/export.h>

void __raw_spin_lock_init(raw_spinlock_t *lock, const char *name,
			  struct lock_class_key *key)
{
}

EXPORT_SYMBOL(__raw_spin_lock_init);

void __rwlock_init(rwlock_t *lock, const char *name,
		   struct lock_class_key *key)
{
}

EXPORT_SYMBOL(__rwlock_init);

void do_raw_spin_unlock(raw_spinlock_t *lock)
{
}

void do_raw_write_unlock(rwlock_t *lock)
{
}

int do_raw_write_trylock(rwlock_t *lock)
{
	return 0;
}

void do_raw_read_unlock(rwlock_t *lock)
{
}

int do_raw_read_trylock(rwlock_t *lock)
{
	return 0;
}
