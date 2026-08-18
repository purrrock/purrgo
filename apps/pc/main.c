#include <stdio.h>

#include "purrgo/navigation.h"

int main(void)
{
    purrgo_nav_state_t state;
    purrgo_nav_init(&state);

    printf("PurrGo PC application skeleton\n");
    return state.valid ? 0 : 0;
}
