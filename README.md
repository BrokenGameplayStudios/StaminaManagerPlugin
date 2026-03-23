# Stamina Manager Plugin

A clean, standalone, fully replicated stamina system for Unreal Engine 5.  
Drop it on any Character and go. Designed for easy extension (Hunger, Fatigue, etc.).

## Features
- Current + Max Stamina (replicated)
- Always-active regeneration (BlueprintNativeEvent so child classes can override for Hunger/Calories)
- Consumption tax system (walking + sprinting + manual)
- Optional auto-walking cost
- Optional auto-sprint (speed + consumption)
- Replicated delegates:
  - OnStaminaChanged (for UI)
  - OnStaminaSpent (jumps, abilities, VFX)
  - OnStaminaDepleted (out of breath)
  - OnStaminaExhausted (pain/slowdown from other plugins)
  - OnStaminaFull (energized VFX)
- 100% multiplayer ready
- No dependencies

## Installation
1. Create folder `Plugins/StaminaManagerPlugin` in your project
2. Place the two files below inside:
   - `StaminaManagerComponent.h` → `Source/StaminaManagerPlugin/Public/`
   - `StaminaManagerComponent.cpp` → `Source/StaminaManagerPlugin/Private/`
3. Add the .uplugin file (or just enable the plugin in Editor)
4. Restart Editor

## Quick Start
1. Add **Stamina Manager Component** to your Character
2. (Optional) Tick **Auto Manage Walking Cost** + set rate
3. (Optional) Tick **Auto Manage Sprinting** + set Sprint Speed & rate
4. Bind inputs:
   - Sprint Pressed → `StartSprinting`
   - Sprint Released → `StopSprinting`
5. For jumps/abilities: `TrySpendStamina(20.0)` or `SpendStamina(20.0)`

## Delegates (bind in Character or Widget)
- **On Stamina Changed** → Update progress bar + text
- **On Stamina Spent** → Jump whoosh sound / particles
- **On Stamina Depleted** → Heavy breathing
- **On Stamina Exhausted** → Pain, slowdown, other plugins react
- **On Stamina Full** → Energized VFX / sound
