#include "spinlock.h"
#include "tas.h"

void spin_lock(volatile char* lock) {
    while(tas(lock)!=0);
}

void spin_unlock(volatile char* lock) {
    *lock = 0;
}