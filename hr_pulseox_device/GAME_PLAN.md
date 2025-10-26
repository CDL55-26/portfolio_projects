# 350 Project

**HbO₂ absorbs IR, Hb absorbs red**

### Calculate a double ratio of absorbance components:

R = (Ared,AC / Ared,DC) / (Air,AC / Air,DC)

Use calibration curve to determine SpO₂ calc.

---

### How do we use the proc.?

- DC is like an offset → how much absorption from veins, capillaries, skin  
- AC captures the fluctuation due to arterial (oxygenated) blood  

---

### Questions?
- Emitters to use  
- Receiver to use → Analog or digital output?  
→ More computation easier  

- Behavioral module to read the data into memory, DMA I/O  

---

### Use CPU for the computation:
- Average the data  
- Do calculation  
- Routine for counting peaks & determining HR  

---

# [Right Page]

### BH module for graphics?

---

### How else to use the processor?

- Could write a derivative routine  
  → slow, some kind of 

- Could write a simple scheduler?  
  → brutal, but cool and would mix OS + get assembly requirement

- Signal error detection?  
  → probably better as behavioral module, but would add some dimension to the project

### RR Scheduler

- Have seperate routines that do HR calc, blood oxygen calc, data reading + data output

- Have scheduler alternate between all of them. Helps keep screen update psedo-realtime
