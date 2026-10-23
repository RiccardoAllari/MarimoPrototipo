#include "WaterCleanProgressSubsystem.h"
#include "WaterCleanZone.h"

TArray<AWaterCleanZone*> UWaterCleanProgressSubsystem::GetStartedZones() const
{
	TArray<AWaterCleanZone*> Result;
	for (const TWeakObjectPtr<AWaterCleanZone>& Weak : Zones)
	{
		if (AWaterCleanZone* Zone = Weak.Get())
		{
			if (Zone->IsStarted()) Result.Add(Zone);
		}
	}
	return Result;
}

void UWaterCleanProgressSubsystem::RegisterZone(AWaterCleanZone* Zone)
{
	if (!Zone || Zones.Contains(Zone)) return;

	Zones.Add(Zone);
	Zone->OnZoneProgressChanged.AddDynamic(this, &UWaterCleanProgressSubsystem::HandleZoneProgress);
	Recompute();
}

void UWaterCleanProgressSubsystem::UnregisterZone(AWaterCleanZone* Zone)
{
	if (!Zone) return;

	Zone->OnZoneProgressChanged.RemoveDynamic(this, &UWaterCleanProgressSubsystem::HandleZoneProgress);
	Zones.RemoveSingleSwap(Zone);
	Recompute();
}

void UWaterCleanProgressSubsystem::HandleZoneProgress(AWaterCleanZone* /*Zone*/, float /*Progress*/)
{
	Recompute();
}

int32 UWaterCleanProgressSubsystem::GetCleanedZoneCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AWaterCleanZone>& Weak : Zones)
	{
		if (const AWaterCleanZone* Zone = Weak.Get())
		{
			if (Zone->IsCleaned()) ++Count;
		}
	}
	return Count;
}

void UWaterCleanProgressSubsystem::Recompute()
{
	float WeightAll = 0.f, ProgressAll = 0.f;
	float WeightStarted = 0.f, ProgressStarted = 0.f;
	int32 Started = 0;
	bool bAllCleaned = true;

	for (const TWeakObjectPtr<AWaterCleanZone>& Weak : Zones)
	{
		if (const AWaterCleanZone* Zone = Weak.Get())
		{
			const float W = Zone->GetAreaWeight();
			const float P = Zone->GetProgress();

			WeightAll += W;
			ProgressAll += W * P;
			bAllCleaned &= Zone->IsCleaned();

			if (Zone->IsStarted())
			{
				++Started;
				WeightStarted += W;
				ProgressStarted += W * P;
			}
		}
	}

	const float NewProgress = WeightAll > 0.f ? ProgressAll / WeightAll : 0.f;
	StartedCount = Started;
	StartedProgress = WeightStarted > 0.f ? ProgressStarted / WeightStarted : 0.f;

	if (!FMath::IsNearlyEqual(NewProgress, CachedProgress, 0.0005f))
	{
		CachedProgress = NewProgress;
		OnLevelProgressChanged.Broadcast(CachedProgress);
	}

	OnStatsChanged.Broadcast();   // zones already throttle their reports

	if (!bLevelCleaned && Zones.Num() > 0 && bAllCleaned)
	{
		bLevelCleaned = true;
		OnLevelCleaned.Broadcast();
	}
}

