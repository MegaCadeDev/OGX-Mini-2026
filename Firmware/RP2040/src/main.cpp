#include <cstdint>

#include "OGXMini/OGXMini.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <cstdio>  

int main() {
    stdio_init_all();
    uart_init(uart0, 115200);  // Initialize UART0 at 115200 baud
    gpio_set_function(0, GPIO_FUNC_UART); // TX
    gpio_set_function(1, GPIO_FUNC_UART); // RX
    printf("Debug ready\n");

    flash_safe_execute_core_init(); 

    OGXMini::initialize();
    OGXMini::run();
    return 0;
}