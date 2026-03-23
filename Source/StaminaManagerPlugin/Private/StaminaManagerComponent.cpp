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
    UpdateGroundState();               
    UpdateAutoMovementConsumption();
    UpdateStamina(DeltaTime);
}

void UStaminaManagerComponent::UpdateGroundState()
{
    if (!CachedMovementComponent || !GetOwner() || !GetOwner()->HasAuthority()) return;

    const bool bCurrentlyOnGround = CachedMovementComponent->IsMovingOnGround();

    if (bCurrentlyOnGround && !bWasOnGroundLastTick)
    {
        AirJumpsUsed = 0;  
    }

    if (bCurrentlyOnGround)
    {
        LastGroundTime = GetWorld()->GetTimeSeconds();
    }

    bWasOnGroundLastTick = bCurrentlyOnGround;
}

bool UStaminaManagerComponent::CanPerformGroundJump() const
{
    if (!CachedMovementComponent) return false;

    const bool bOnGround = CachedMovementComponent->IsMovingOnGround();
    const float CurrentTime = GetWorld()->GetTimeSeconds();

    return bOnGround || (CurrentTime - LastGroundTime <= CoyoteTime);
}

// ===================================================================
// Jump Functions (Ground-only + Coyote + Optional Air Jumps + Weak fallback)
// ===================================================================

void UStaminaManagerComponent::StartJumping()
{
    ACharacter* Char = Cast<ACharacter>(GetOwner());
    if (!Char) return;

    if (!bAutoManageJumping)
    {
        Char->Jump();
        return;
    }

    if (CanPerformGroundJump())
    {
        // Ground jump (normal or weak fallback)
        const bool bHasEnoughForNormal = HasEnoughStamina(JumpStaminaCost);
        const float JumpPower = bHasEnoughForNormal ? NormalJumpZVelocity : ExhaustedJumpZVelocity;

        Char->LaunchCharacter(FVector(0.0f, 0.0f, JumpPower), false, true);
        Server_StartJumping();  // server handles cost + authoritative launch + notification
    }
    else if (MaxAirJumps > 0 && AirJumpsUsed < MaxAirJumps)
    {
        // Air jump
        Char->LaunchCharacter(FVector(0.0f, 0.0f, AirJumpZVelocity), false, true);
        Server_StartJumping();  // server handles cost + notification
    }
}

void UStaminaManagerComponent::StopJumping()
{
    if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
    {
        Char->StopJumping();
    }
}

void UStaminaManagerComponent::PerformJump()
{
    StartJumping();
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        StopJumping();
    });
}

bool UStaminaManagerComponent::Server_StartJumping_Validate() { return true; }

void UStaminaManagerComponent::Server_StartJumping_Implementation()
{
    ACharacter* Char = Cast<ACharacter>(GetOwner());
    if (!Char || !CachedMovementComponent) return;

    const bool bGroundJump = CanPerformGroundJump();

    if (bGroundJump)
    {
        const bool bHasEnoughForNormal = HasEnoughStamina(JumpStaminaCost);
        const float JumpPower = bHasEnoughForNormal ? NormalJumpZVelocity : ExhaustedJumpZVelocity;

        if (bHasEnoughForNormal && JumpStaminaCost > 0.0f)
        {
            SpendStamina(JumpStaminaCost);
        }

        Char->LaunchCharacter(FVector(0.0f, 0.0f, JumpPower), false, true);
        Multicast_JumpPerformed(bHasEnoughForNormal ? EStaminaJumpType::Normal : EStaminaJumpType::Exhausted);
    }
    else if (MaxAirJumps > 0 && AirJumpsUsed < MaxAirJumps)
    {
        AirJumpsUsed++;
        if (AirJumpStaminaCost > 0.0f)
        {
            SpendStamina(AirJumpStaminaCost);
        }

        Char->LaunchCharacter(FVector(0.0f, 0.0f, AirJumpZVelocity), false, true);
        Multicast_JumpPerformed(EStaminaJumpType::Air);
    }
}

void UStaminaManagerComponent::UpdateAutoMovementConsumption()
{
    if (!bAutoManageWalkingCost || !CachedMovementComponent || !(GetOwner() && GetOwner()->HasAuthority())) return;

    bool bIsMoving = CachedMovementComponent->Velocity.Size2D() > 10.0f && CachedMovementComponent->IsMovingOnGround();

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

    if (CanRegenerate())
    {
        CurrentStamina += RegenRate * DeltaTime;
        bChanged = true;
    }

    if (CurrentConsumptionRate > 0.0f)
    {
        CurrentStamina -= CurrentConsumptionRate * DeltaTime;
        bChanged = true;
    }

    CurrentStamina = FMath::Clamp(CurrentStamina, 0.0f, MaxStamina);

    if (bChanged)
    {
        BroadcastStaminaChange();

        if (GetOwner() && GetOwner()->HasAuthority())
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

void UStaminaManagerComponent::Multicast_JumpPerformed_Implementation(EStaminaJumpType JumpType)
{
    OnJumpPerformed.Broadcast(JumpType);
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
// Core Functions 
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
            if (CurrentStamina != Old && GetOwner() && GetOwner()->HasAuthority())
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

        if (GetOwner() && GetOwner()->HasAuthority())
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

        if (GetOwner() && GetOwner()->HasAuthority())
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

bool UStaminaManagerComponent::Server_SetSprinting_Validate(bool bNewSprinting) { return true; }

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