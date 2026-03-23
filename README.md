# Stamina Manager Plugin

A clean, fully replicated stamina system for Unreal Engine 5.  
Drop the `UStaminaManagerComponent` on any Character for instant stamina, regeneration, consumption, sprinting, and advanced jumping.

## What It Does
- Current + Max Stamina (fully replicated)
- Always-active regeneration (`CanRegenerate` is Blueprint-overridable)
- Consumption tax (walking + sprinting + manual)
- Optional auto-sprint (speed + cost)
- Advanced jumping:
  - Coyote Time
  - Weak/exhausted jumps when low on stamina
  - Optional air jumps (double/triple jump support)
  - Variable height (hold button = higher jump)

## Main Functions
- `StartJumping` / `StopJumping` (bind to input)
- `StartSprinting` / `StopSprinting`
- `SpendStamina` / `TrySpendStamina`
- `SetCanRegenerate`

## Delegates (bind anywhere)
- `OnStaminaChanged` → UI progress bar + text
- `OnStaminaSpent` → Jump/ability VFX & sounds
- `OnStaminaDepleted` / `OnStaminaExhausted` → Exhaustion effects
- `OnStaminaFull`
- `OnJumpPerformed` → Different feedback for Normal / Exhausted / Air jumps

## Example in This Repo
Contains a Blueprint child component (`StaminaManagerCompOverride`) that overrides `CanRegenerate` so stamina **does not regenerate while falling**. Use it as a template for Hunger, Fatigue, or movement-based rules.

100% multiplayer ready • No dependencies
