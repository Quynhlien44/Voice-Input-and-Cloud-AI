#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main()
{
    while (1)
    {
        printf("Turn on light\n");
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        char rx_buf[128];
        if (fgets(rx_buf, sizeof(rx_buf), stdin))
        {
            printf("PC Reply: %s\n", rx_buf);
        }
    }
}