/**
  *****************************************************************************
  * @file   callout_delay.c
  * 
  *****************************************************************************
  * @attention
  *
  *
  *  Copyright (c) 2021, InnoPhase, Inc.
  *
  *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  *  POSSIBILITY OF SUCH DAMAGE.
  *
  *****************************************************************************
  */

#include <kernel/callout.h>
#include <kernel/os.h>
#include <kernel/semaphore.h>
#include "callout_delay.h"

typedef struct
{
    struct callout delay_callout;
    struct os_semaphore delay_sem;
} delay_context_t;


static void __irq delay_fn(struct callout *co)
{
    delay_context_t *delay_context = container_of(co, delay_context_t, delay_callout);

    os_sem_post(&delay_context->delay_sem);
}

void callout_delay_us(uint32_t us)
{
    delay_context_t delay_context;

    callout_init(&delay_context.delay_callout, delay_fn);
    os_sem_init(&delay_context.delay_sem, 0);
    callout_schedule(&delay_context.delay_callout, us);

    os_sem_wait(&delay_context.delay_sem);
}

void callout_delay_ms(uint32_t ms)
{
    callout_delay_us(SYSTIME_MS(ms));
}
