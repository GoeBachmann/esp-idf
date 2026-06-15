/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Heap Task Tracking Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_task_info.h"
#include "esp_heap_caps.h"

typedef struct {
    char * ptr;
    TaskHandle_t task;
} allocation_t;

#define NUM_ALLOCATIONS 500
static volatile allocation_t allocations[NUM_ALLOCATIONS];
static volatile bool bug_reproduced = false;
static volatile bool task_done = false;

static void realloc_then_free(void * ptr, size_t size) {
    void * new_ptr = realloc(ptr, size);
    if (new_ptr != NULL) ptr = new_ptr;
    free(ptr);
}

static void alloc_task(void *args)
{
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();

    size_t i = (size_t)args;
    allocations[i].ptr = heap_caps_malloc(10, MALLOC_CAP_DEFAULT);
    allocations[i].task = handle;

    char *task_name = pcTaskGetName(handle);
    printf("Starting task: %s %d (ptr = %p)\n", task_name, i, handle);

    for (int j = 0; j < i; j++) {
        if (allocations[j].task == handle) {
            printf("Found task with same handle allocations[%d].task == allocations[%d].task\n", i, j);

            // trigger bug
            realloc_then_free(allocations[i].ptr, 20);
            realloc_then_free(allocations[j].ptr, 20);
            // heap usage now at -10 bytes for both tasks
            allocations[i].ptr = NULL;
            allocations[j].ptr = NULL;

            // mark done
            bug_reproduced = true;
            task_done = true;

            while (1) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }

    // mark done
    task_done = true;
    vTaskDelete(NULL);
}

static void dummy_task(void *args) { while (1) taskYIELD(); }

void app_main(void)
{
    xTaskCreate(&dummy_task, "dummy_task", 3072, NULL, 0, NULL);

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        // run alloc task
        task_done = false;
        xTaskCreate(&alloc_task, "alloc_task", 3072, (void*)i, 5, NULL);
        while (!task_done) vTaskDelay(pdMS_TO_TICKS(50));

        if (bug_reproduced) break;
    }
    if (!bug_reproduced) return;

    /* print overview information of every task */
    printf("\n PRINTING OVERVIEW STATISTICS OF EACH TASK\n");
    heap_caps_print_all_task_stat_overview(stdout);

    /* print detailed statistics for every task */
    printf("\n PRINTING DETAILED STATISTICS OF EACH TASK\n");
    heap_caps_print_all_task_stat(stdout);
}
