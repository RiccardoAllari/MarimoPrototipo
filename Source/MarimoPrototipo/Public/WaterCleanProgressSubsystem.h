#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WaterCleanProgressSubsystem.generated.h"

class AWaterCleanZone;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelProgress, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelCleaned);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCleaningStatsChanged);

UCLASS()
class MARIMOPROTOTIPO_API UWaterCleanProgressSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Fires whenever the overall progress changes (0..1)
	UPROPERTY(BlueprintAssignable, Category = "Cleaning")
	FOnLevelProgress OnLevelProgressChanged;

	// Fires once, when every zone is clean
	UPROPERTY(BlueprintAssignable, Category = "Cleaning")
	FOnLevelCleaned OnLevelCleaned;

	// Overall progress 0..1, weighted by zone area
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
	float GetLevelProgress() const { return CachedProgress; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
	int32 GetZoneCount() const { return Zones.Num(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
	int32 GetCleanedZoneCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
	bool IsLevelCleaned() const { return bLevelCleaned; }

	// Fires whenever any number below may have changed. Read the getters afterwards.
	UPROPERTY(BlueprintAssignable, Category = "Cleaning")
	FOnCleaningStatsChanged OnStatsChanged;

	// Areas where the robot has cleaned at least something
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
	int32 GetStartedZoneCount() const { return StartedCount; }

	// Combined progress (0..1) of the started areas only, weighted by area size
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
	float GetStartedAreasProgress() const { return StartedProgress; }

	// The started areas, to build a per-area list. Use Get Progress and Area Name on each.
	UFUNCTION(BlueprintCallable, Category = "Cleaning")
	TArray<AWaterCleanZone*> GetStartedZones() const;

	void RegisterZone(AWaterCleanZone* Zone);
	void UnregisterZone(AWaterCleanZone* Zone);

private:
	UFUNCTION()
	void HandleZoneProgress(AWaterCleanZone* Zone, float Progress);

	void Recompute();

	TArray<TWeakObjectPtr<AWaterCleanZone>> Zones;
	float CachedProgress = 0.f;
	bool bLevelCleaned = false;
	int32 StartedCount = 0;
	float StartedProgress = 0.f;
};