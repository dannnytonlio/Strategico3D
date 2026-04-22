# 🎮 Strategico 3D – Turn-Based Strategy Game (UE5.6)

#  Project Overview

**Strategico 3D** is a turn-based tactical strategy game developed in **Unreal Engine 5.6** as a project for the course:

**Course** Programmazione e Analisi degli Algoritmi (PAA)
**Academic Year:** 2025/2026
**Professor:** Ing. Giuseppe Cicala

The game features a **1 vs 1 match (Human vs AI)** on a procedurally generated grid where players must strategically place units, control towers, and defeat the opponent.

---

## 🎯 Core Features

*  Procedural grid generation (Perlin Noise)
*  Turn-based gameplay (Human vs AI)
*  AI using A* pathfinding and heuristic decision-making
*  Tower control system (win condition)
*  Unit placement phase + battle phase
*  Two unit types:

  * **Sniper** (long range)
  * **Brawler** (melee)
* Dynamic HUD (turn, HP, towers, history)
*  Move history tracking
*  Movement & attack range visualization


## 🕹️ How to Run

1. Clone the repository:

   ```bash
   git clone https://github.com/dannnytonlio/Strategico3D.git
   ```

2. Open:

   ```
   Strategico3D.uproject
   ```

3. If prompted, click **"Yes (Rebuild)"**

4. Once the map loads:
    Click **Start** to begin the game

---

## 🎮 Gameplay Rules

* Each player controls **2 units**
* The game has 2 phases:

  * **Placement phase**
  * **Battle phase**
* Players alternate turns
* Units can:

  * Move
  * Attack
* Towers provide strategic advantage

### 🏆 Victory Condition

A player wins if they control **at least 2 towers for 2 consecutive turns**

---

## 🧠 AI System

The AI includes:

* ** A* pathfinding for movement
* **Heuristic decision-making** for:

  * Target selection
  * Position evaluation
  * Risk management


## 📷 Screenshots

see file: '\images'
---

## 📊 UML Diagram
see file: '\UMLSTRATEGICO3D.txt'


## ✅ Requirements Implementation (Course Evaluation)

| #  | Requirement                       | Status               |
| -- | --------------------------------- | -------------------- |
| 1  | Project compiles, clean code, OOP | ✅ fully Implemented |
| 2  | Grid correctly displayed          | ✅ fully Implemented |
| 3  | Unit & tower placement system     | ✅ fully Implemented |
| 4  | AI using A*                       | ✅ fully Implemented |
| 5  | Turn-based system + win condition | ✅ fully Implemented |
| 6  | GUI showing game state            | ✅ fully Implemented |
| 7  | Movement range visualization      | ✅ fully Implemented |
| 8  | Counter-attack system             | ✅ fully Implemented |
| 9  | Move history log                  | ✅ fully Implemented |
| 10 | Advanced AI heuristics            | ✅ fully Implemented |

---

## ⚠️ Important Notes

* Developed strictly with:

  * Unreal Engine 5.6
  * C++ (90% for core logic)
  * Blueprints only for UI layer
  
* The following folders are excluded from Git:

  ```
  Binaries/
  Intermediate/
  Saved/
  DerivedDataCache/
  ```
* The project compiles correctly in a clean environment


## 👤 Author

**Danny joel kenfack tonlio **
Strategico3D Project – 2026


