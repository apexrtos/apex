#ifndef arm_v7m_mpu_h
#define arm_v7m_mpu_h

struct thread;

void mpu_user_thread_switch();
void mpu_thread_terminate(struct thread *);

#endif
