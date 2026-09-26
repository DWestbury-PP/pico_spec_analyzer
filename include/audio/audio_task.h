/**
 * @file audio_task.h
 * @brief Core 0: DMA-driven ADC capture and per-hop spectrum analysis.
 */

#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

/** Start the ADC/DMA capture. Call once before audio_task_run(). */
void audio_task_init(void);

/** Analysis loop. Never returns. */
void audio_task_run(void);

#endif // AUDIO_TASK_H
