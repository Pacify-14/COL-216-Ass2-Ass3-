# Pipeline Simulator: Forwarding vs. Non-Forwarding

## Overview

This project implements a pipeline simulator for a processor, with two versions:  
- **`forward.cpp`**: Implements **data forwarding** to minimize stalls.  
- **`noforward.cpp`**: Implements a **non-forwarding** pipeline, which introduces stalls when data dependencies exist.  

Both implementations simulate instruction execution with different pipeline strategies.

---

## Design Decisions

### **Forwarding Implementation (`forward.cpp`)**
- Uses **data forwarding** to handle read-after-write (RAW) hazards efficiently.
- Eliminates unnecessary stalls by forwarding results as soon as they are available.
- Improves performance by reducing pipeline delays.

### **Non-Forwarding Implementation (`noforward.cpp`)**
- Does **not** implement forwarding, leading to pipeline stalls on RAW dependencies.
- Models a basic pipeline where instructions must wait for previous results.
- Simpler to implement but significantly slower due to frequent stalls.

---

## Known Issues
- **Increased stalls in `noforward.cpp`**: Since it lacks forwarding, it experiences execution delays.
- **Pipeline hazards**: While `forward.cpp` mitigates hazards using forwarding, structural dependencies might still introduce delays.
- **Edge cases**: The implementations handle standard cases well but might require adjustments for complex instruction sequences.

---

## Sources Consulted
- **ChatGPT**: Used for some theorical understanding pipeline hazards, forwarding, and stall management.
- **Computer Architecture Textbooks**: HandP_RISCV Referenced for pipeline execution strategies.
- **Online Research**: Studied processor data dependency handling techniques.

---

## How to Compile and Run

### **Compilation**
To compile both executables, run:
```sh
make
