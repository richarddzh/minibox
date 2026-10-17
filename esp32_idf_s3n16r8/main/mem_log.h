#pragma once
// 一行内存快照, 方便事后回看是否可以调整 arena / DMA / TLS 缓冲。
//
// 输出格式 (单行 INFO 日志):
//   [mem] <tag> heap=NNN min=NNN int=NNN dma=NNN dma_max=NNN
//
// 字段说明:
//   heap     当前空闲堆 (DRAM 可分配总量)
//   min      启动到现在为止历史最低空闲堆 (掉到多少就别再放大 buffer)
//   int      MALLOC_CAP_INTERNAL 当前空闲 (排除 PSRAM)
//   dma      MALLOC_CAP_DMA      当前空闲 (Wi-Fi/I2S DMA 共享池)
//   dma_max  MALLOC_CAP_DMA      最大连续可分配块 (碎片化指标)
//
// 调用频率应保持低: 启动 + 每个长事件前后, 不要塞进音频热循环。
void mem_log(const char *tag);
