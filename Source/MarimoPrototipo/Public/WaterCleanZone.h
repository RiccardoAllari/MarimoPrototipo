#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterCleanZone.generated.h"

class AWaterCleanZone;
class UBoxComponent;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZoneCleaned, AWaterCleanZone*, Zone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnZoneProgress, AWaterCleanZone*, Zone, float, Progress);

UCLASS()
class MARIMOPROTOTIPO_API AWaterCleanZone : public AActor
{
    GENERATED_BODY()

public:
    AWaterCleanZone();

    // ---- Components ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cleaning")
    TObjectPtr<UBoxComponent> Bounds;               // defines the area in XY. Keep actor scale at 1 and rotation at 0.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cleaning")
    TObjectPtr<UStaticMeshComponent> WaterMesh;     // set a centered plane mesh in the Blueprint child

    // ---- Settings ----
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning")
    TObjectPtr<UTexture2D> BrushTexture;            // soft white circle WITH alpha

    UPROPERTY(EditAnywhere, Category = "Cleaning")
    int32 MaskRes = 512;                            // visual only

    UPROPERTY(EditAnywhere, Category = "Cleaning")
    int32 ProgressRes = 64;                         // gameplay progress grid

    UPROPERTY(EditAnywhere, Category = "Cleaning")
    float CleanThreshold = 0.97f;

    UPROPERTY(EditAnywhere, Category = "Cleaning")
    float SpacingFactor = 0.4f;                     // stamp spacing = brush radius * this

    UPROPERTY(EditAnywhere, Category = "Cleaning")
    float FlushInterval = 0.05f;                    // seconds between GPU draws

    // ---- Events and queries ----
    UPROPERTY(BlueprintAssignable, Category = "Cleaning")
    FOnZoneCleaned OnZoneCleaned;

    UPROPERTY(BlueprintAssignable, Category = "Cleaning")
    FOnZoneProgress OnZoneProgressChanged;

    // Name shown in the UI
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning")
    FText AreaName;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
    bool IsStarted() const { return bStarted; }

    // 0..1, reaches 1 exactly when the zone counts as cleaned
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
    float GetProgress() const { return bDone ? 1.f : FMath::Clamp(GetCleanPercent() / CleanThreshold, 0.f, 1.f); }

    // Zone surface area in world units, used to weight the level progress
    float GetAreaWeight() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
    float GetCleanPercent() const { return Total > 0 ? float(CleanedCount) / float(Total) : 1.f; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cleaning")
    bool IsCleaned() const { return bDone; }

    // ---- Used by the cleaner component ----
    void CleanSegment(const FVector& From, const FVector& To, float WorldRadius);
    void ResetStroke() { bHasLastStamp = false; }

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnBoundsBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                              UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                              bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoundsEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    void QueuePoint(const FVector2D& WorldXY);
    void FlushStamps();

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> MaskRT;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> MID;

    TArray<FVector2D> PendingUV;
    FVector2D PendingRadiusUV = FVector2D::ZeroVector;
    FVector2D LastStampXY = FVector2D::ZeroVector;
    bool bHasLastStamp = false;
    float TimeSinceFlush = 0.f;
    float LastBroadcastProgress = -1.f;
    bool bStarted = false;

    TBitArray<> CellDirty;
    TBitArray<> CellCleaned;
    int32 Total = 0;
    int32 CleanedCount = 0;
    bool bDone = false;
};