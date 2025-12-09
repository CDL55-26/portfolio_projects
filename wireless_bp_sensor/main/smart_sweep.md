# smart sweep ideas

## callibration

not trivial for a callibration

We'd probably want to sweep a huge range and have some algo for detecting every peak
Then change capactiance, sweep again, and mark all the peaks that changed.
Repeat until some level of certainty that we found the dip corresponding to the sensor

## no callibration

skipping callibration would be easier.

If we know the range will be 50 - 150 MHz, for example
 - first sweep is full range
 - next sweep, change parameters so that start stop is ± 20 MHz of peak.
 - do same process for the rest of recording


## distance between peaks for systole vs diastole 

This should also be pretty easy
 - keep 4 pointers to max, min, prev, current frequency
 - max and min are global / persistent. take the difference between the two: ∆pressure -> maybe log in DB 

- need to figure out when to latch max and min
    - general idea is whenever current frequency flips direction. 
        - could do like current_freq - prev_freq. If this ever changes signs (+ -> - or - -> +), latch max or min
