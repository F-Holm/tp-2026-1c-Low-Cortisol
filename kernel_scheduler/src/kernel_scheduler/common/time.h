#pragma once

// Milliseconds since the epoch.
unsigned long millis(void);

// Absolute difference between two millis() readings.
unsigned long time_diff(unsigned long time_1, unsigned long time_2);
