#pragma once
#include "Platform.hpp"


void TimeServiceInit();     // Needs to be called once at startup.
void TimeServiceShutdown(); // Needs to be called at shutdown.

// Get current time ticks.
i64 TimeNow(); 

// Get microseconds from time ticks
double TimeMicroseconds(i64 time); 

// Get milliseconds from time ticks
double TimeMilliseconds(i64 time); 

// Get seconds from time ticks
double TimeSeconds(i64 time);      

// Get time difference from start to current time.
i64 TimeFrom(i64 starting_time);                 

double TimeFromMicroseconds(i64 starting_time); // Convenience method.
double TimeFromMilliseconds(i64 starting_time); // Convenience method.
double TimeFromSeconds(i64 starting_time);      // Convenience method.

double TimeDeltaSeconds(i64 starting_time, i64 ending_time);
double TimeDeltaMilliseconds(i64 starting_time, i64 ending_time);