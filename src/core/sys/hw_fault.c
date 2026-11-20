#include "purrgo/hw_fault.h"

#include "purrgo/logger.h"

#include <stdarg.h>
#include <stdint.h>

/*
 * Битовые маски по узлам. latched - эпизод идёт (звук для него уже заказан), pending - звук заказан,
 * но главный цикл его ещё не сыграл. Меняются атомарно: purrgo_hw_fault() зовут и из прерываний.
 */
static volatile uint32_t s_latched;
static volatile uint32_t s_pending;

#if defined(__GNUC__) || defined(__clang__)
#  define MASK_OR(var, bit)   ((uint32_t)__atomic_fetch_or(&(var), (bit), __ATOMIC_SEQ_CST))
#  define MASK_AND(var, bit)  ((uint32_t)__atomic_fetch_and(&(var), (bit), __ATOMIC_SEQ_CST))
#  define MASK_TAKE(var)      ((uint32_t)__atomic_exchange_n(&(var), 0u, __ATOMIC_SEQ_CST))
#else
#  error "hw_fault.c needs GCC/Clang atomics"
#endif

#define HW_MSG_SIZE 224u

static const char* const k_names[PURRGO_HWF_COUNT] = {
    [PURRGO_HWF_SD]      = "SD",
    [PURRGO_HWF_GNSS]    = "GNSS",
    [PURRGO_HWF_DISPLAY] = "DISPLAY",
    [PURRGO_HWF_BATTERY] = "BATTERY",
    [PURRGO_HWF_SYSTEM]  = "SYSTEM",
};

const char* purrgo_hw_name(purrgo_hw_t hw)
{
    return ((unsigned)hw < (unsigned)PURRGO_HWF_COUNT) ? k_names[hw] : "?";
}

void purrgo_hw_fault(purrgo_hw_t hw, const char* format, ...)
{
    char msg[HW_MSG_SIZE];
    va_list ap;

    va_start(ap, format);
    (void)purrgo_vsnprintf(msg, sizeof msg, format, ap);
    va_end(ap);

    PURRGO_LOGE("HW FAULT [%s]: %s\r\n", purrgo_hw_name(hw), msg);

    if ((unsigned)hw >= (unsigned)PURRGO_HWF_COUNT) {
        return;
    }
    const uint32_t bit = 1u << (unsigned)hw;

    if ((MASK_OR(s_latched, bit) & bit) == 0u) {
        (void)MASK_OR(s_pending, bit);   /* новый эпизод: сигнал сыграет главный цикл */
    }
}

void purrgo_hw_fault_clear(purrgo_hw_t hw)
{
    if ((unsigned)hw >= (unsigned)PURRGO_HWF_COUNT) {
        return;
    }
    const uint32_t bit = 1u << (unsigned)hw;

    if ((MASK_AND(s_latched, ~bit) & bit) != 0u) {
        PURRGO_LOGI("HW OK [%s]: recovered\r\n", purrgo_hw_name(hw));
    }
}

bool purrgo_hw_fault_active(purrgo_hw_t hw)
{
    return ((unsigned)hw < (unsigned)PURRGO_HWF_COUNT) && ((s_latched & (1u << (unsigned)hw)) != 0u);
}

bool purrgo_hw_fault_take_pending(void)
{
    return MASK_TAKE(s_pending) != 0u;
}

void purrgo_hw_fault_reset(void)
{
    s_latched = 0u;
    s_pending = 0u;
}
