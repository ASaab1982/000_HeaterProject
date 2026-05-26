#ifndef HEATER_MODEL_H
#define HEATER_MODEL_H

// [HEATER MODEL] Call updateHeaterModel() periodically to simulate boiler water temperature.
// It tracks elapsed time internally using millis() — no argument needed.
void updateHeaterModel();

#endif
