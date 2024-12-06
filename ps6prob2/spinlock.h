#ifndef SPINLOCK_H
#include "tas.h"
void spin_lock(volatile char* lock);
void spin_unlock(volatile char* lock);
#endif