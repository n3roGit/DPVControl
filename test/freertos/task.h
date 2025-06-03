#ifndef FREERTOS_TASK_H
#define FREERTOS_TASK_H

#include <cstdint>

typedef void* TaskHandle_t;
typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t)(xTimeInMs))
#define portMAX_DELAY ((TickType_t)0xffffffffUL)

void vTaskDelay(TickType_t xTicksToDelay);
void vTaskDelayUntil(TickType_t* pxPreviousWakeTime, TickType_t xTimeIncrement);

#endif // FREERTOS_TASK_H 