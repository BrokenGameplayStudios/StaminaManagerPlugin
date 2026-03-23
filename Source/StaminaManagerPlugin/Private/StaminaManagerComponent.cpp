// StaminaManagerComponent.cpp

#include "StaminaManagerComponent.h"
#include "GameFramework/Character.h"

UStaminaManagerComponent::UStaminaManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
    CurrentStamina = MaxStamina;
}

void UStaminaManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentStamina = MaxStamina;
    BroadcastStaminaChange();

    // Safely cache movement component (only works on ACharacter owners)
    if (ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner()))
    {
        CachedMovementComponent = CharacterOwner->GetCharacterMovement();
        if (CachedMovementComponent)
        {
            OriginalMaxWalkSpeed = CachedMovementComponent->MaxWalkSpeed;
        }
    }
}

void UStaminaManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UStaminaManagerComponent, MaxStamina);
    DOREPLIFETIME(UStaminaManagerComponent, CurrentStamina);
    DOREPLIFETIME(UStaminaManagerComponent, CurrentConsumptionRate);
    DOREPLIFETIME(UStaminaManagerComponent, bIsSprinting);
}

void UStaminaManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateAutoMovementConsumption();
    UpdateStamina(DeltaTime);
}

void UStaminaManagerComponent::UpdateAutoMovementConsumption()
{
    if (!bAutoManageWalkingCost || !CachedMovementComponent || !GetOwner()->HasAuthority()) return;

    bool bIsMoving = CachedMovementComponent->Velocity.Size2D() > 10.0f &&
        CachedMovementComponent->IsMovingOnGround();

    float DesiredRate = 0.0f;
    if (bIsSprinting)
    {
        DesiredRate = SprintConsumptionRate;
    }
    else if (bIsMoving)
    {
        DesiredRate = WalkingConsumptionRate;
    }

    if (CurrentConsumptionRate != DesiredRate)
    {
        SetConsumptionRate(DesiredRate);
    }
}

void UStaminaManagerComponent::UpdateStamina(float DeltaTime)
{
    const float PreviousStamina = CurrentStamina;
    bool bChanged = false;

    // Regeneration (always attempts if allowed)
    if (CanRegenerate())
    {
        CurrentStamina += RegenRate * DeltaTime;
        bChanged = true;
    }

    // Consumption tax (applied on top of regeneration)
    if (CurrentConsumptionRate > 0.0f)
    {
        CurrentStamina -= CurrentConsumptionRate * DeltaTime;
        bChanged = true;
    }

    // Clamp to valid range
    CurrentStamina = FMath::Clamp(CurrentStamina, 0.0f, MaxStamina);

    if (bChanged)
    {
        BroadcastStaminaChange();

        if (GetOwner()->HasAuthority())
        {
            CheckForDepletion(PreviousStamina);
            CheckForStaminaFull(PreviousStamina);
            HandleExhaustion();
        }
    }
}

void UStaminaManagerComponent::CheckForDepletion(float PreviousStamina)
{
    if (PreviousStamina > 0.0f && CurrentStamina <= 0.0f)
    {
        Multicast_StaminaDepleted();
    }
}

void UStaminaManagerComponent::CheckForStaminaFull(float PreviousStamina)
{
    if (PreviousStamina < MaxStamina && CurrentStamina >= MaxStamina)
    {
        Multicast_StaminaFull();
    }
}

void UStaminaManagerComponent::HandleExhaustion()
{
    if (CurrentStamina <= 0.0f && CurrentConsumptionRate > 0.0f)
    {
        Multicast_StaminaExhausted();
    }
}

void UStaminaManagerComponent::BroadcastStaminaChange()
{
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}

void UStaminaManagerComponent::Multicast_StaminaSpent_Implementation(float Amount)
{
    OnStaminaSpent.Broadcast(Amount);
}

void UStaminaManagerComponent::Multicast_StaminaDepleted_Implementation()
{
    OnStaminaDepleted.Broadcast();
    AutoStopSprintIfNeeded();
}

void UStaminaManagerComponent::Multicast_StaminaExhausted_Implementation()
{
    OnStaminaExhausted.Broadcast();
}

void UStaminaManagerComponent::Multicast_StaminaFull_Implementation()
{
    OnStaminaFull.Broadcast();
}

void UStaminaManagerComponent::AutoStopSprintIfNeeded()
{
    if (bAutoManageSprinting && bIsSprinting)
    {
        StopSprinting();
    }
}

void UStaminaManagerComponent::OnRep_CurrentStamina()
{
    BroadcastStaminaChange();
}

// ===================================================================
// Stamina Core Functions
// ===================================================================

void UStaminaManagerComponent::SetMaxStamina(float NewMaxStamina, bool bClampCurrentStamina)
{
    if (NewMaxStamina != MaxStamina)
    {
        MaxStamina = FMath::Max(0.0f, NewMaxStamina);
        if (bClampCurrentStamina)
        {
            const float Old = CurrentStamina;
            CurrentStamina = FMath::Min(CurrentStamina, MaxStamina);
            if (CurrentStamina != Old && GetOwner()->HasAuthority())
            {
                CheckForDepletion(Old);
            }
        }
        BroadcastStaminaChange();
    }
}

void UStaminaManagerComponent::SetCurrentStamina(float NewCurrentStamina)
{
    const float OldStamina = CurrentStamina;
    const float Clamped = FMath::Clamp(NewCurrentStamina, 0.0f, MaxStamina);

    if (Clamped != CurrentStamina)
    {
        CurrentStamina = Clamped;
        BroadcastStaminaChange();

        if (GetOwner()->HasAuthority())
        {
            CheckForDepletion(OldStamina);
        }
    }
}

bool UStaminaManagerComponent::HasEnoughStamina(float Amount) const
{
    return CurrentStamina >= Amount;
}

void UStaminaManagerComponent::SpendStamina(float Amount)
{
    if (Amount <= 0.0f) return;

    const float OldStamina = CurrentStamina;
    CurrentStamina = FMath::Max(0.0f, CurrentStamina - Amount);

    if (CurrentStamina != OldStamina)
    {
        BroadcastStaminaChange();

        if (GetOwner()->HasAuthority())
        {
            Multicast_StaminaSpent(Amount);
            CheckForDepletion(OldStamina);
        }
    }
}

bool UStaminaManagerComponent::TrySpendStamina(float Amount)
{
    if (HasEnoughStamina(Amount))
    {
        SpendStamina(Amount);
        return true;
    }
    return false;
}

// ===================================================================
// Consumption & Regeneration
// ===================================================================

void UStaminaManagerComponent::StartConsumption(float RatePerSecond)
{
    CurrentConsumptionRate = FMath::Max(0.0f, RatePerSecond);
}

void UStaminaManagerComponent::StopConsumption()
{
    CurrentConsumptionRate = 0.0f;
}

void UStaminaManagerComponent::SetConsumptionRate(float NewRate)
{
    CurrentConsumptionRate = FMath::Max(0.0f, NewRate);
}

void UStaminaManagerComponent::SetCanRegenerate(bool bNewCanRegenerate)
{
    bAllowRegeneration = bNewCanRegenerate;
}

bool UStaminaManagerComponent::CanRegenerate_Implementation() const
{
    return bAllowRegeneration && CurrentStamina < MaxStamina;
}

float UStaminaManagerComponent::GetStaminaPercentage() const
{
    return MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f;
}

// ===================================================================
// Sprint Functions
// ===================================================================

void UStaminaManagerComponent::StartSprinting()
{
    if (bAutoManageSprinting)
    {
        Server_SetSprinting(true);
    }
}

void UStaminaManagerComponent::StopSprinting()
{
    if (bAutoManageSprinting)
    {
        Server_SetSprinting(false);
    }
}

bool UStaminaManagerComponent::Server_SetSprinting_Validate(bool bNewSprinting)
{
    return true;
}

void UStaminaManagerComponent::Server_SetSprinting_Implementation(bool bNewSprinting)
{
    if (bIsSprinting == bNewSprinting || !CachedMovementComponent) return;

    bIsSprinting = bNewSprinting;

    if (bIsSprinting)
    {
        CachedMovementComponent->MaxWalkSpeed = SprintSpeed;
    }
    else
    {
        CachedMovementComponent->MaxWalkSpeed = OriginalMaxWalkSpeed;
    }
}