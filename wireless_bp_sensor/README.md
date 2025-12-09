# ECE 459 - Design Project: Wireless BP sensor

## Abstract
The objective of this project is to develop a complete system for wireless blood pressure measurements.
### Background
Blood pressure cuffs are wildly accessible and easy to use. They work by constricting blood flow in the arm until no sound of fluid flow
can be heard. 

Then, pressure is relaxed which allows turbulent blood flow through the cuffed region. This is noisy and can be heard clearly. 
Pressure continues to be decreased until laminar blood flow, which is silent.

Noting when blood flow stops and when it becomes laminar gives measurements for diastolic and systolic pressure.

### Motivation
While this type of blood pressure measurment is fine for physicals or general purpose medicine, it lacks the precision neccesary for post-surgical data collection. 

When a surgeon implants, for example, a stent into a blood vessel, he is primarily concerned with the blood pressure and flow rate in the affected region.

Currently, in order to monitor this pressure, a patient must have radioactive dye injected into the affected region. They then must undergo
an MRI so that Doctors can observe the flow of the dye.

This is both expensive and invasive. Instead a wireless device could be used to quickly and locally take blood pressure measurments, greatly
improving a patients quality of life.

### Setup

The technology that makes this wireless approach takes advantage of near-field magnetic coupling.

A tiny MEMS LC tank is implanted into the affected region during a surgery. This device has a capacitor made out of photoresist connected to an inductor coil. The capacitor expands and compresses with blood pressure, changing the resonant frequency of the tank.

Using an external transciever, we drive a coil antenna with a set of known frequencies and measure the difference between incident and reflected waves.

This coil antenna sits near the inductor coil of the LC tank, which causes them to magnetically couple, acting as a transformer system.

When far from the tank's resonance, our waveform generator sees a mostly reactive load and as such, the VSWR is very high. However, when our signal matches the resonant frequency of the tank, the impedance of the tank becomes mostly real and resistive. This allows energy to travel into the LC circuit, reducing reflection. 

This sharp dip can be measured, which allows us to identify the resonant frequency of the tank. We can create a known curve that maps frequency -> capacitance -> pressure.

This allows us to wirelessly record the pressure seen by the device in a highly localized region

## Hardware Architecture
The hardware aspect of this project can be split into two logical groups
1. The MCU driven transciever
2. The LC tank based sensor

We will discuss each of these in some depth

### Transciever
What we are refering to as a transciever is really just a simplistic **VNA** (Vector Network Analyzer) that soley measures reflection.

It consists of:
1. An MCU (we have versions for both the nrf52 and esp32)
2. A waveform generator (ideally a PLL synthesizer, we use the adf4351)
3. A bi-directional bridge + log power detector
4. A coil antenna 


## Software Architecture 

## Testing and Validation