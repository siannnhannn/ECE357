#include "tas.h"

//loop until we can acquire the lock
void spin_lock(volatile char* lock) {
    while(tasl(lock)!=0);
    lock
}

void spin_unlock(volatile char* lock) {
    *lock = 0;
}
