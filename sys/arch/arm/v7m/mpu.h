#pragma once

struct thread;

void mpu_user_thread_switch();
void mpu_thread_terminate(struct thread *);
