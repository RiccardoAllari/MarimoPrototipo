#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WaterCleanerComponent.generated.h"

class AWaterCleanZone;

UCLASS(ClassGroup = (Cleaning), meta = (BlueprintSpawnableComponent))
class MARIMOPROTOTIPO_API UWaterCleanerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaterCleanerComponent();

	// Brush radius in world units
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleaning")
	float CleanRadius = 80.f;

	// Brush position relative to the robot (local space)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleaning")
	FVector BrushOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cleaning")
	bool bCleaningEnabled = true;

	// Call from the move input (Triggered). Safe to call every frame.
	UFUNCTION(BlueprintCallable, Category = "Cleaning")
	void WakeCleaner();

	// Called by zones through their overlap events
	void AddZone(AWaterCleanZone* Zone);
	void RemoveZone(AWaterCleanZone* Zone);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;

private:
	FVector GetBrushLocation() const;

	TArray<TWeakObjectPtr<AWaterCleanZone>> ActiveZones;
	FVector LastPos = FVector::ZeroVector;
};