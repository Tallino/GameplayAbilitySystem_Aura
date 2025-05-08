// Copyright Tritaghiaccio Games S.r.l

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuraAffectActor.generated.h"

class UGameplayEffect;

UCLASS()
class AURA_API AAuraAffectActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AAuraAffectActor();
	
protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Applied Effects")
	TSubclassOf<UGameplayEffect> InstantGameplayEffectClass;

};
