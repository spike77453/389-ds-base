/** BEGIN COPYRIGHT BLOCK
 * Copyright (C) 2008 Red Hat, Inc.
 * All rights reserved.
 *
 * License: GPL (version 3 or any later version).
 * See LICENSE for details. 
 * END COPYRIGHT BLOCK **/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "slap.h"

#ifndef ATOMIC_64BIT_OPERATIONS
#include <pthread.h>
#endif

#ifdef HPUX
#include <machine/sys/inline.h>
#endif

/*
 * Counter Structure
 */
typedef struct slapi_counter {
    uint64_t value;
#ifndef ATOMIC_64BIT_OPERATIONS
    pthread_mutex_t _lock;
#endif
} slapi_counter;

/*
 * slapi_counter_new()
 *
 * Allocates and initializes a new Slapi_Counter.
 */
Slapi_Counter *slapi_counter_new()
{
    Slapi_Counter *counter = NULL;

    counter = (Slapi_Counter *)slapi_ch_calloc(1, sizeof(Slapi_Counter));

    if (counter != NULL) {
        slapi_counter_init(counter);
    }

    return counter;
}

/*
 * slapi_counter_init()
 *
 * Initializes a Slapi_Counter.
 */
void slapi_counter_init(Slapi_Counter *counter)
{
    if (counter != NULL) {
        /* Set the value to 0. */
        slapi_counter_set_value(counter, 0);
#ifndef ATOMIC_64BIT_OPERATIONS
        pthread_mutex_init(&(counter->_lock), NULL);
#endif
    }
}

/*
 * slapi_counter_destroy()
 *
 * Destroy's a Slapi_Counter and sets the
 * pointer to NULL to prevent reuse.
 */
void slapi_counter_destroy(Slapi_Counter **counter)
{
    if ((counter != NULL) && (*counter != NULL)) {
#ifndef ATOMIC_64BIT_OPERATIONS
        pthread_mutex_destroy(&((*counter)->_lock));
#endif
        slapi_ch_free((void **)counter);
    }
}

/*
 * slapi_counter_increment()
 *
 * Atomically increments a Slapi_Counter.
 */
uint64_t slapi_counter_increment(Slapi_Counter *counter)
{
    return slapi_counter_add(counter, 1);
}

/*
 * slapi_counter_decrement()
 *
 * Atomically decrements a Slapi_Counter. Note
 * that this will not prevent you from wrapping
 * around 0.
 */
uint64_t slapi_counter_decrement(Slapi_Counter *counter)
{
    return slapi_counter_subtract(counter, 1);
}

/*
 * slapi_counter_add()
 *
 * Atomically add a value to a Slapi_Counter.
 */
uint64_t slapi_counter_add(Slapi_Counter *counter, uint64_t addvalue)
{
    uint64_t newvalue = 0;

    if (counter == NULL) {
        return newvalue;
    }
#ifdef ATOMIC_64BIT_OPERATIONS
    newvalue = __atomic_add_fetch_8(&(counter->value), addvalue, __ATOMIC_SEQ_CST);
#else
#ifdef HPUX
    uint64_t prev = 0;
    /* fetchadd only works with values of 1, 4, 8, and 16.  In addition, it requires
     * it's argument to be an integer constant. */
    if (addvalue == 1) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), 1, _LDHINT_NONE);
        newvalue += 1;
    } else if  (addvalue == 4) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), 4, _LDHINT_NONE);
        newvalue += 4;
    } else if (addvalue == 8) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), 8, _LDHINT_NONE);
        newvalue += 8;
    } else if (addvalue == 16) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), 16, _LDHINT_NONE);
        newvalue += 16;
    } else {
        /* For other values, we have to use cmpxchg. */
        do {
            prev = slapi_counter_get_value(counter);
            newvalue = prev + addvalue;
            /* Put prev in a register for cmpxchg to compare against */
           _Asm_mov_to_ar(_AREG_CCV, prev);
        } while (prev != _Asm_cmpxchg(_FASZ_D, _SEM_ACQ, &(counter->value), newvalue, _LDHINT_NONE));
    }
#else
    pthread_mutex_lock(&(counter->_lock));
    counter->value += addvalue;
    newvalue = counter->value;
    pthread_mutex_unlock(&(counter->_lock));
#endif
#endif

    return newvalue;
}

/*
 * slapi_counter_subtract()
 *
 * Atomically subtract a value from a Slapi_Counter.  Note
 * that this will not prevent you from wrapping around 0.
 */
uint64_t slapi_counter_subtract(Slapi_Counter *counter, uint64_t subvalue)
{
    uint64_t newvalue = 0;

    if (counter == NULL) {
        return newvalue;
    }

#ifdef ATOMIC_64BIT_OPERATIONS
    newvalue = __atomic_sub_fetch_8(&(counter->value), subvalue, __ATOMIC_SEQ_CST);
#else
#ifdef HPUX
    uint64_t prev = 0;
    /* fetchadd only works with values of -1, -4, -8, and -16.  In addition, it requires
     * it's argument to be an integer constant. */
    if (subvalue == 1) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), -1, _LDHINT_NONE);
        newvalue -= 1;
    } else if  (subvalue == 4) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), -4, _LDHINT_NONE);
        newvalue -= 4;
    } else if (subvalue == 8) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), -8, _LDHINT_NONE);
        newvalue -= 8;
    } else if (subvalue == 16) {
        newvalue = _Asm_fetchadd(_FASZ_D, _SEM_ACQ, &(counter->value), -16, _LDHINT_NONE);
        newvalue -= 16;
    } else {
        /* For other values, we have to use cmpxchg. */
        do {
            prev = slapi_counter_get_value(counter);
            newvalue = prev - subvalue;
            /* Put prev in a register for cmpxchg to compare against */
           _Asm_mov_to_ar(_AREG_CCV, prev);
        } while (prev != _Asm_cmpxchg(_FASZ_D, _SEM_ACQ, &(counter->value), newvalue, _LDHINT_NONE));
    }
#else
    pthread_mutex_lock(&(counter->_lock));
    counter->value -= subvalue;
    newvalue = counter->value;
    pthread_mutex_unlock(&(counter->_lock));
#endif
#endif

    return newvalue;
}

/*
 * slapi_counter_set_value()
 *
 * Atomically sets the value of a Slapi_Counter.
 */
uint64_t slapi_counter_set_value(Slapi_Counter *counter, uint64_t newvalue)
{
    uint64_t value = 0;

    if (counter == NULL) {
        return value;
    }

#ifdef ATOMIC_64BIT_OPERATIONS
    __atomic_store_8(&(counter->value), newvalue, __ATOMIC_SEQ_CST);
#else /* HPUX */
#ifdef HPUX
    do {
        value = counter->value;
        /* Put value in a register for cmpxchg to compare against */
        _Asm_mov_to_ar(_AREG_CCV, value);
    } while (value != _Asm_cmpxchg(_FASZ_D, _SEM_ACQ, &(counter->value), newvalue, _LDHINT_NONE));
#else
    pthread_mutex_lock(&(counter->_lock));
    counter->value = newvalue;
    pthread_mutex_unlock(&(counter->_lock));
#endif
#endif
    return newvalue;
}

/*
 * slapi_counter_get_value()
 *
 * Returns the value of a Slapi_Counter.
 */
uint64_t slapi_counter_get_value(Slapi_Counter *counter)
{
    uint64_t value = 0;

    if (counter == NULL) {
        return value;
    }

#ifdef ATOMIC_64BIT_OPERATIONS
    value = __atomic_load_8(&(counter->value), __ATOMIC_SEQ_CST);
#else  /* HPUX */
#ifdef HPUX
    do {
        value = counter->value;
        /* Put value in a register for cmpxchg to compare against */
        _Asm_mov_to_ar(_AREG_CCV, value);
    } while (value != _Asm_cmpxchg(_FASZ_D, _SEM_ACQ, &(counter->value), value, _LDHINT_NONE));
#else
    pthread_mutex_lock(&(counter->_lock));
    value = counter->value;
    pthread_mutex_unlock(&(counter->_lock));
#endif
#endif

    return value;
}

/*
 * atomic increment functions (64bit)
 */
uint64_t
slapi_atomic_incr_64(uint64_t *ptr, int memorder)
{
#ifdef ATOMIC_64BIT_OPERATIONS
    return __atomic_add_fetch_8(ptr, 1, memorder);
#else
    PRInt32 *pr_ptr = (PRInt32 *)ptr;
    return PR_AtomicIncrement(pr_ptr);
#endif
}

/*
 * atomic decrement functions (64bit)
 */

uint64_t
slapi_atomic_decr_64(uint64_t *ptr, int memorder)
{
#ifdef ATOMIC_64BIT_OPERATIONS
    return __atomic_sub_fetch_8(ptr, 1, memorder);
#else
    PRInt32 *pr_ptr = (PRInt32 *)ptr;
    return PR_AtomicDecrement(pr_ptr);
#endif
}


