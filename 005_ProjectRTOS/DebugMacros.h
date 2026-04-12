#ifndef DEBUG_MACROS_H
#define DEBUG_MACROS_H

// SET TO 0 TO SILENCE SYSTEM & GAIN RAM
// SET TO 1 TO SEE LOGS
#define DEBUG_LEVEL 1 

#if DEBUG_LEVEL
  #define D_PRINT(x)    Serial.print(x)
  #define D_PRINTLN(x)  Serial.println(x)
  #define D_PRINTF(x,y) Serial.print(x,y)
#else
  #define D_PRINT(x)
  #define D_PRINTLN(x)
  #define D_PRINTF(x,y)
#endif

#endif