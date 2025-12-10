#include <Arduino.h>

void printHeapStats() {
  Serial.printf("\n[HEAP]\n");
  Serial.printf("Heap total      : %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap libre      : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Heap min libre  : %u bytes\n", ESP.getMinFreeHeap());
  Serial.printf("Heap max alloc  : %u bytes\n", ESP.getMaxAllocHeap());

#ifdef BOARD_HAS_PSRAM
  Serial.printf("\n[PSRAM]\n");
  Serial.printf("PSRAM total     : %u bytes\n", ESP.getPsramSize());
  Serial.printf("PSRAM libre     : %u bytes\n", ESP.getFreePsram());
#endif
}
