/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file timer.c
 * @brief Linux implementation of the timer interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "include/timer_platform.h"

bool has_timer_expired(struct Timer *timer) {
	uint64_t os_time = os_systime64();

	if (os_time >= timer->end_time )
		return 1;
	return 0;
}

void countdown_ms(struct Timer *timer, uint32_t timeout) {
		timer->end_time = timeout*1000 + os_systime64();
}

uint64_t left_ms(struct Timer *timer) {

	uint64_t result_ms;

	result_ms = (timer->end_time - os_systime64()/1000);
	return result_ms;
}

void countdown_sec(struct Timer *timer, uint32_t timeout) {
	timer->end_time = timeout*1000000 + os_systime64();
}

void init_timer(struct Timer *timer) {
	timer->end_time = 0;
}

void delay(unsigned milliseconds)
{
	uint32_t sleepTime = (milliseconds * 1000);

	os_sleep_us(sleepTime, OS_TIMEOUT_NO_WAKEUP);
}

#ifdef __cplusplus
}
#endif
