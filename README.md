# 🏥 Patient Bed Allocation System

A robust and realistic **C language** project that simulates a hospital's **bed allocation system**, efficiently managing ICU and General Ward beds using **Linux system programming**, **multithreading**, **semaphores**, and **priority queues**. This simulation handles real-time patient admissions, emergency prioritization, and discharge mechanisms with proper synchronization and terminal interaction.

---

## 📌 Project Overview

This console-based system simulates the functioning of a hospital's patient bed allocation. It prioritizes patients based on their condition (emergency or regular), severity score, and bed availability (ICU or General Ward). It uses **POSIX threads**, **semaphores**, and **mutexes** to manage concurrent activities and ensure data integrity.

---

## 🚀 Features

- ✅ Multithreaded admission and discharge system
- ⚠️ Priority Queue: Emergency patients are prioritized
- 🛏️ Dynamic allocation of ICU and General beds
- 📡 Real-time terminal status monitoring (every 4 seconds)
- 📁 Logging of all major events in `hospital.log`
- 🔐 Thread-safe implementation using mutexes and semaphores
- 📦 Graceful shutdown via `Ctrl+C` (SIGINT handler)
- 🎨 Colorful and structured console output using ANSI escape codes

---

## 🛠️ Technologies Used

- **C Language**
- **POSIX Threads (pthreads)**
- **Semaphores (`<semaphore.h>`)**
- **Mutexes (`<pthread.h>`)**
- **Signal Handling (`<signal.h>`)**
- **File I/O for Logging (`<stdio.h>`)**
- **Terminal Color Codes for CLI Beautification**

---

## 🧠 How It Works

1. **Patients** arrive (simulated or via user input) with:
   - Name
   - Type: Regular / Emergency
   - Severity score
   - Bed preference: ICU / General

2. **Priority Queue** stores incoming patients and automatically prioritizes emergency cases.

3. **Threads** simulate:
   - Patient admission
   - Periodic discharge
   - Real-time bed status monitoring
   - Simulated ward/ICU allocation using semaphores

4. **Logger** writes all events (check-ins, admissions, discharges, bed status) to a log file `hospital.log`.

5. **Graceful Exit**: On `Ctrl+C`, the system completes its operations and terminates safely.

---

## 🖼️ Sample Console Output

```bash
[STATUS] Beds Occupied: 2/5
Patients in Queue: 4

Admitted: Bob (EMERGENCY)
[DISCHARGE] Discharged a patient.
[ICU ALLOCATED] Patient 101 (Severity: 9)

[INFO] Shutting down hospital system...
System shutdown complete.
