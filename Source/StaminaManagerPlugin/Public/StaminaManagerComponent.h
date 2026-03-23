// StaminaManagerComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "StaminaManagerComponent.generated.h"

/**
 * Stamina Manager Component
 *
 * A fully replicated, plug-and-play stamina system designed for easy extension.
 * Features:
 *  - Always-active regeneration (overridable for Hunger/Calorie plugins)
 *  - Consumption tax system (walking + sprinting + manual abilities)
 *  - Optional automatic sprint and walking cost management
 *  - Multiple replicated delegates for UI, VFX, audio, and other plugins
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, CurrentStamina, float, MaxStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaSpent, float, AmountSpent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaDepleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaExhausted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaFull);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class STAMINAMANAGERPLUGIN_API UStaminaManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStaminaManagerComponent();

    // ===================================================================
    // Core Stamina Settings
    // ===================================================================
    /** Maximum stamina value. Replicated. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", Replicated, meta = (ClampMin = "0"))
    float MaxStamina = 100.0f;

    /** Current stamina value. Replicated with OnRep. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_CurrentStamina)
    float CurrentStamina;

    // ===================================================================
    // Regeneration
    // ===================================================================
    /** Stamina points regenerated per second when CanRegenerate() returns true. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0"))
    float RegenRate = 15.0f;

    // ===================================================================
    // Consumption
    // ===================================================================
    /** Current total consumption rate per second (auto walking + sprint + manual). Replicated. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina|Consumption", Replicated)
    float CurrentConsumptionRate = 0.0f;

    // ===================================================================
    // Sprint Management (Optional - auto handles speed & consumption)
    // ===================================================================
    /** If true, this component automatically controls sprinting speed and consumption. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Sprint", DisplayName = "Auto Manage Sprinting")
    bool bAutoManageSprinting = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Sprint", meta = (EditCondition = "bAutoManageSprinting", ClampMin = "0"))
    float SprintSpeed = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Sprint", meta = (EditCondition = "bAutoManageSprinting", ClampMin = "0"))
    float SprintConsumptionRate = 25.0f;

    /** Replicated sprint state (use in Animation Blueprint). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina|Sprint", Replicated)
    bool bIsSprinting = false;

    // ===================================================================
    // Walking Management (Optional - auto walking tax)
    // ===================================================================
    /** If true, this component automatically applies a consumption cost while walking. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", DisplayName = "Auto Manage Walking Cost")
    bool bAutoManageWalkingCost = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (EditCondition = "bAutoManageWalkingCost", ClampMin = "0"))
    float WalkingConsumptionRate = 5.0f;

    // ===================================================================
    // Delegates / Events
    // ===================================================================
    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaChanged OnStaminaChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaSpent OnStaminaSpent;

    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaDepleted OnStaminaDepleted;

    /** Fires every frame on all clients when stamina = 0 AND consumption > 0 (perfect for pain, slowdown, or other plugin reactions). */
    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaExhausted OnStaminaExhausted;

    /** Fires on all clients when stamina reaches MaxStamina from below (perfect for "energized" VFX, sounds, UI flash, etc.). */
    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaFull OnStaminaFull;

    // ===================================================================
    // Public Functions
    // ===================================================================
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void SetMaxStamina(float NewMaxStamina, bool bClampCurrentStamina = true);

    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void SetCurrentStamina(float NewCurrentStamina);

    UFUNCTION(BlueprintPure, Category = "Stamina")
    bool HasEnoughStamina(float Amount) const;

    UFUNCTION(BlueprintCallable, Category = "Stamina")
    bool TrySpendStamina(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void SpendStamina(float Amount);

    // Manual Consumption
    UFUNCTION(BlueprintCallable, Category = "Stamina|Consumption")
    void StartConsumption(float RatePerSecond);

    UFUNCTION(BlueprintCallable, Category = "Stamina|Consumption")
    void StopConsumption();

    UFUNCTION(BlueprintCallable, Category = "Stamina|Consumption")
    void SetConsumptionRate(float NewRate);

    // Regeneration Control
    UFUNCTION(BlueprintCallable, Category = "Stamina|Regeneration")
    void SetCanRegenerate(bool bNewCanRegenerate);

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stamina|Regeneration")
    bool CanRegenerate() const;
    virtual bool CanRegenerate_Implementation() const;

    UFUNCTION(BlueprintPure, Category = "Stamina")
    float GetStaminaPercentage() const;

    // Sprint Control
    UFUNCTION(BlueprintCallable, Category = "Stamina|Sprint")
    void StartSprinting();

    UFUNCTION(BlueprintCallable, Category = "Stamina|Sprint")
    void StopSprinting();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    bool bAllowRegeneration = true;
    UCharacterMovementComponent* CachedMovementComponent = nullptr;
    float OriginalMaxWalkSpeed = 600.0f;

    UFUNCTION()
    void OnRep_CurrentStamina();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StaminaSpent(float Amount);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StaminaDepleted();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StaminaExhausted();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StaminaFull();

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetSprinting(bool bNewSprinting);

    void UpdateStamina(float DeltaTime);
    void BroadcastStaminaChange();
    void CheckForDepletion(float PreviousStamina);
    void CheckForStaminaFull(float PreviousStamina);
    void AutoStopSprintIfNeeded();
    void UpdateAutoMovementConsumption();
    void HandleExhaustion();
};