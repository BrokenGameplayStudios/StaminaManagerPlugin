#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "StaminaManagerComponent.generated.h"

/** Jump type returned by the new OnJumpPerformed delegate */
UENUM(BlueprintType)
enum class EStaminaJumpType : uint8
{
    Normal,
    Exhausted,
    Air
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, CurrentStamina, float, MaxStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStaminaSpent, float, AmountSpent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaDepleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaExhausted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaFull);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJumpPerformed, EStaminaJumpType, JumpType);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class STAMINAMANAGERPLUGIN_API UStaminaManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStaminaManagerComponent();

    // ===================================================================
    // Core Stamina Settings
    // ===================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", Replicated, meta = (ClampMin = "0"))
    float MaxStamina = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_CurrentStamina)
    float CurrentStamina;

    // ===================================================================
    // Regeneration
    // ===================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0"))
    float RegenRate = 15.0f;

    // ===================================================================
    // Consumption
    // ===================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina|Consumption", Replicated)
    float CurrentConsumptionRate = 0.0f;

    // ===================================================================
    // Sprint Management (Optional)
    // ===================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Sprint", DisplayName = "Auto Manage Sprinting")
    bool bAutoManageSprinting = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Sprint", meta = (EditCondition = "bAutoManageSprinting", ClampMin = "0"))
    float SprintSpeed = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Sprint", meta = (EditCondition = "bAutoManageSprinting", ClampMin = "0"))
    float SprintConsumptionRate = 35.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina|Sprint", Replicated)
    bool bIsSprinting = false;

    // ===================================================================
    // Walking Management (Optional)
    // ===================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", DisplayName = "Auto Manage Walking Cost")
    bool bAutoManageWalkingCost = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Movement", meta = (EditCondition = "bAutoManageWalkingCost", ClampMin = "0"))
    float WalkingConsumptionRate = 10.0f;

    // ===================================================================
    // Jump Management (Ground only + Coyote + Optional Air Jumps)
    // ===================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", DisplayName = "Auto Manage Jumping")
    bool bAutoManageJumping = false;

    /** Coyote Time: small window after leaving ground where you can still ground jump */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", meta = (EditCondition = "bAutoManageJumping", ClampMin = "0"))
    float CoyoteTime = 0.2f;

    /** Normal ground jump Z velocity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", meta = (EditCondition = "bAutoManageJumping", ClampMin = "0"))
    float NormalJumpZVelocity = 500.0f;

    /** Exhausted/weak ground jump Z velocity (used when stamina too low for normal cost) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", meta = (EditCondition = "bAutoManageJumping", ClampMin = "0"))
    float ExhaustedJumpZVelocity = 350.0f;

    /** Stamina cost for normal ground jump (0 = free). If not enough stamina → weak free jump. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", meta = (EditCondition = "bAutoManageJumping", ClampMin = "0"))
    float JumpStaminaCost = 50.0f;

    /** Maximum number of air jumps allowed (0 = disabled) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", meta = (EditCondition = "bAutoManageJumping", ClampMin = "0"))
    int32 MaxAirJumps = 0;

    /** Air jump Z velocity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", meta = (EditCondition = "bAutoManageJumping", ClampMin = "0"))
    float AirJumpZVelocity = 350.0f;

    /** Stamina cost for each air jump */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Jump", meta = (EditCondition = "bAutoManageJumping", ClampMin = "0"))
    float AirJumpStaminaCost = 20.0f;

    // ===================================================================
    // Delegates / Events
    // ===================================================================
    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaChanged OnStaminaChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaSpent OnStaminaSpent;

    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaDepleted OnStaminaDepleted;

    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaExhausted OnStaminaExhausted;

    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnStaminaFull OnStaminaFull;

    /** Fires on all clients when a jump happens (Normal / Exhausted / Air) */
    UPROPERTY(BlueprintAssignable, Category = "Stamina|Events")
    FOnJumpPerformed OnJumpPerformed;

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

    UFUNCTION(BlueprintCallable, Category = "Stamina|Consumption")
    void StartConsumption(float RatePerSecond);

    UFUNCTION(BlueprintCallable, Category = "Stamina|Consumption")
    void StopConsumption();

    UFUNCTION(BlueprintCallable, Category = "Stamina|Consumption")
    void SetConsumptionRate(float NewRate);

    UFUNCTION(BlueprintCallable, Category = "Stamina|Regeneration")
    void SetCanRegenerate(bool bNewCanRegenerate);

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stamina|Regeneration")
    bool CanRegenerate() const;
    virtual bool CanRegenerate_Implementation() const;

    UFUNCTION(BlueprintPure, Category = "Stamina")
    float GetStaminaPercentage() const;

    // Sprint
    UFUNCTION(BlueprintCallable, Category = "Stamina|Sprint")
    void StartSprinting();

    UFUNCTION(BlueprintCallable, Category = "Stamina|Sprint")
    void StopSprinting();

    // Jump (Mario-style hold for variable height)
    UFUNCTION(BlueprintCallable, Category = "Stamina|Jump")
    void StartJumping();

    UFUNCTION(BlueprintCallable, Category = "Stamina|Jump")
    void StopJumping();

    /** Convenience for simple tap jumps */
    UFUNCTION(BlueprintCallable, Category = "Stamina|Jump")
    void PerformJump();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    bool bAllowRegeneration = true;
    UCharacterMovementComponent* CachedMovementComponent = nullptr;
    float OriginalMaxWalkSpeed = 600.0f;

    // Jump tracking
    float LastGroundTime = 0.0f;
    int32 AirJumpsUsed = 0;
    bool bWasOnGroundLastTick = true;

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

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_JumpPerformed(EStaminaJumpType JumpType);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_StartJumping();

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetSprinting(bool bNewSprinting);

    void UpdateStamina(float DeltaTime);
    void BroadcastStaminaChange();
    void CheckForDepletion(float PreviousStamina);
    void CheckForStaminaFull(float PreviousStamina);
    void AutoStopSprintIfNeeded();
    void UpdateAutoMovementConsumption();
    void HandleExhaustion();
    void UpdateGroundState();           
    bool CanPerformGroundJump() const;  
};