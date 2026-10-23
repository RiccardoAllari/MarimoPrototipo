#include "WaterCleanerComponent.h"
#include "WaterCleanZone.h"

UWaterCleanerComponent::UWaterCleanerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // asleep until woken
	PrimaryComponentTick.TickGroup = TG_PostPhysics;      // after the movement component
	PrimaryComponentTick.TickInterval = 0.03f;            // ~30 Hz while moving
}

FVector UWaterCleanerComponent::GetBrushLocation() const
{
	return GetOwner()->GetActorTransform().TransformPosition(BrushOffset);
}

void UWaterCleanerComponent::WakeCleaner()
{
	if (IsComponentTickEnabled()) return;

	LastPos = GetBrushLocation();
	SetComponentTickEnabled(true);
}

void UWaterCleanerComponent::AddZone(AWaterCleanZone* Zone)
{
	if (!Zone) return;
	Zone->ResetStroke();               // no long stamp line from a previous visit
	ActiveZones.AddUnique(Zone);
}

void UWaterCleanerComponent::RemoveZone(AWaterCleanZone* Zone)
{
	ActiveZones.RemoveSingleSwap(Zone);
}

void UWaterCleanerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
										   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const FVector NowPos = GetBrushLocation();
	
	
	const bool bStill = NowPos.Equals(LastPos, 0.1f)
					 && GetOwner()->GetVelocity().SizeSquared2D() < 1.f;

	if (bStill)
	{
		SetComponentTickEnabled(false);   // fully stopped (glide included): sleep
		return;
	}

	if (bCleaningEnabled)
	{
		for (int32 i = ActiveZones.Num() - 1; i >= 0; --i)
		{
			if (AWaterCleanZone* Zone = ActiveZones[i].Get())
			{
				Zone->CleanSegment(LastPos, NowPos, CleanRadius);
			}
			else
			{
				ActiveZones.RemoveAtSwap(i);
			}
		}
	}
	LastPos = NowPos;
}