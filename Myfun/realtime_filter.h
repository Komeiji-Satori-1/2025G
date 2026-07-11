#ifndef __REALTIME_FILTER_H__
#define __REALTIME_FILTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void RealtimeFilter_Init(void);
uint8_t RealtimeFilter_Start(void);
void RealtimeFilter_Stop(void);
uint8_t RealtimeFilter_IsRunning(void);
void RealtimeFilter_ProcessHalf(uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif /* __REALTIME_FILTER_H__ */
