// Copyright 2020-2021 Sheffer Online Services.
// MIT License. See LICENSE for details.

#include "RyRuntimeObjectHelpers.h"
#include "UObject/ObjectRedirector.h"
#include "RyRuntimeModule.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

// Async asset loading extension
#include "Engine/StreamableManager.h"
#include "LatentActions.h"

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool URyRuntimeObjectHelpers::IsLiveSoftObjectReference(const TSoftObjectPtr<UObject>& SoftObjectReference)
{
    if(SoftObjectReference.IsNull())
        return false;

    TPersistentObjectPtr<FSoftObjectPath> persistObjPtr(SoftObjectReference.ToSoftObjectPath());
    return persistObjPtr.Get(false) != nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UPackage* URyRuntimeObjectHelpers::FindOrLoadPackage(const FString& PackageName)
{
    UPackage* Pkg = FindPackage(nullptr, *PackageName);
    if(!Pkg)
    {
        Pkg = LoadPackage(nullptr, *PackageName, LOAD_NoWarn | LOAD_Quiet);
    }
    return Pkg;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UObject* URyRuntimeObjectHelpers::LoadObjectFromPackage(UPackage* package, const FString& objectName)
{
    if(!package)
    {
        return nullptr;
    }

    return ::LoadObject<UObject>(package, *objectName);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UPackage* URyRuntimeObjectHelpers::GetPackageOfObject(UObject* object)
{
    if(!object)
        return nullptr;

    return object->GetOutermost();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void URyRuntimeObjectHelpers::GetObjectsInPackage(UPackage* package, TArray<UObject*>& ObjectsOut)
{
    if(!package)
    {
        return;
    }

    if(!package->IsFullyLoaded())
    {
        package->FullyLoad();
    }

    for(TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
    {
        UObject* Object = *ObjIt;
        if(Object->IsIn(package))
        {
            ObjectsOut.Add(Object);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UObject* URyRuntimeObjectHelpers::LoadObject(const FString& fullObjectPath)
{
    UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *fullObjectPath, nullptr, LOAD_None, nullptr, true, nullptr);

#if WITH_EDITOR
    // Look at core redirects if we didn't find the object
    if(!LoadedObject)
    {
        FSoftObjectPath FixupObjectPath = fullObjectPath;
        if(FixupObjectPath.FixupCoreRedirects())
        {
            LoadedObject = ::LoadObject<UObject>(nullptr, *FixupObjectPath.ToString());
        }
    }
#endif

    while(UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadedObject))
    {
        LoadedObject = Redirector->DestinationObject;
    }

    return LoadedObject;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
struct FLoadAssetPriorityActionBase : FPendingLatentAction
{
	// @TODO: it would be good to have static/global manager? 
	FSoftObjectPath SoftObjectPath;
	FStreamableManager StreamableManager;
	TSharedPtr<FStreamableHandle> Handle;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	virtual void OnLoaded() PURE_VIRTUAL(FLoadAssetPriorityActionBase::OnLoaded, );

	FLoadAssetPriorityActionBase(const FSoftObjectPath& InSoftObjectPath, const int32 Priority, const FLatentActionInfo& InLatentInfo)
		: SoftObjectPath(InSoftObjectPath)
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
	{
		Handle = StreamableManager.RequestAsyncLoad(SoftObjectPath, FStreamableDelegate(), Priority);
	}

	virtual ~FLoadAssetPriorityActionBase()
	{
		if (Handle.IsValid())
		{
			Handle->ReleaseHandle();
		}
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		const bool bLoaded = !Handle.IsValid() || Handle->HasLoadCompleted() || Handle->WasCanceled();
		if (bLoaded)
		{
			OnLoaded();
		}
		Response.FinishAndTriggerIf(bLoaded, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return FString::Printf(TEXT("Load Asset Priority Action Base: %s"), *SoftObjectPath.ToString());
	}
#endif
};

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void URyRuntimeObjectHelpers::LoadAssetPriority(UObject* WorldContextObject, TSoftObjectPtr<UObject> Asset, const int32 Priority,
                                                FOnAssetLoaded OnLoaded, FLatentActionInfo LatentInfo)
{
	struct FLoadAssetAction : FLoadAssetPriorityActionBase
	{
		FOnAssetLoaded OnLoadedCallback;

		FLoadAssetAction(const FSoftObjectPath& InSoftObjectPath, const int32 Priority, FOnAssetLoaded InOnLoadedCallback, const FLatentActionInfo& InLatentInfo)
			: FLoadAssetPriorityActionBase(InSoftObjectPath, Priority, InLatentInfo)
			, OnLoadedCallback(InOnLoadedCallback)
		{}

		virtual void OnLoaded() override
		{
			UObject* LoadedObject = SoftObjectPath.ResolveObject();
			OnLoadedCallback.ExecuteIfBound(LoadedObject);
		}
	};

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();

		// We always spawn a new load even if this node already queued one, the outside node handles this case
		FLoadAssetAction* NewAction = new FLoadAssetAction(Asset.ToSoftObjectPath(), Priority, OnLoaded, LatentInfo);
		LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
struct FLoadPackagePriorityActionBase : FPendingLatentAction
{
    FString PackagePath;
    FName ExecutionFunction;
    int32 OutputLink;
    FWeakObjectPtr CallbackTarget;
    EAsyncLoadingResult::Type Result;
    UPackage* LoadedPackage;

    int32 LoadRequest;
    FLoadPackageAsyncDelegate LoadCB;

    virtual void OnLoaded() PURE_VIRTUAL(FLoadPackagePriorityActionBase::OnLoaded, );

    FLoadPackagePriorityActionBase(const FString& packagePath, const int32 priority, const bool blockOnLoad, const FLatentActionInfo& inLatentInfo)
        : PackagePath(packagePath)
        , ExecutionFunction(inLatentInfo.ExecutionFunction)
        , OutputLink(inLatentInfo.Linkage)
        , CallbackTarget(inLatentInfo.CallbackTarget)
        , Result(EAsyncLoadingResult::Failed)
        , LoadedPackage(nullptr)
    {
        LoadCB.BindRaw(this, &FLoadPackagePriorityActionBase::OnPackageLoadCompleteCB);
        LoadRequest = LoadPackageAsync(PackagePath, nullptr, nullptr, LoadCB, PKG_None, INDEX_NONE, priority);
        if(LoadRequest != INDEX_NONE)
        {
            if(blockOnLoad)
            {
                FlushAsyncLoading(LoadRequest);
            }
        }
    }

    virtual ~FLoadPackagePriorityActionBase()
    {
    }

    void OnPackageLoadCompleteCB(const FName& packagePath, UPackage* loadedPackage, EAsyncLoadingResult::Type result)
    {
        Result = result;
        LoadedPackage = loadedPackage;
    }

    virtual void UpdateOperation(FLatentResponse& Response) override
    {
        const bool bLoaded = LoadRequest == INDEX_NONE;
        if (bLoaded)
        {
            OnLoaded();
        }
        Response.FinishAndTriggerIf(bLoaded, ExecutionFunction, OutputLink, CallbackTarget);
    }

#if WITH_EDITOR
    virtual FString GetDescription() const override
    {
        return FString::Printf(TEXT("Load Package Priority Action Base: %s"), *PackagePath);
    }
#endif
};

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void URyRuntimeObjectHelpers::LoadPackagePriority(UObject* WorldContextObject, const FString& PackagePath, const int32 Priority,
                                                  const bool BlockOnLoad, FOnPackageLoaded OnLoaded, FLatentActionInfo LatentInfo)
{
    struct FLoadPackageAction : FLoadPackagePriorityActionBase
    {
        FOnPackageLoaded OnLoadedCallback;

        FLoadPackageAction(const FString& packagePath, const int32 priority, const bool blockOnLoad, FOnPackageLoaded onPackageLoaded, const FLatentActionInfo& inLatentInfo)
            : FLoadPackagePriorityActionBase(packagePath, priority, blockOnLoad, inLatentInfo)
            , OnLoadedCallback(onPackageLoaded)
        {}

        virtual void OnLoaded() override
        {
            OnLoadedCallback.ExecuteIfBound(LoadedPackage, static_cast<ERyAsyncLoadingResult>(Result));
        }
    };

    if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
    {
        FLatentActionManager& LatentManager = World->GetLatentActionManager();

        // We always spawn a new load even if this node already queued one, the outside node handles this case
        FLoadPackageAction* NewAction = new FLoadPackageAction(PackagePath, Priority, BlockOnLoad, OnLoaded, LatentInfo);
        LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UClass* URyRuntimeObjectHelpers::GetParentClass(UClass* Class)
{
    if(!Class)
        return nullptr;

    return Class->GetSuperClass();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void URyRuntimeObjectHelpers::GetClassHierarchy(UClass* Class, TArray<UClass*>& ClassHierarchy, const bool includeSelf)
{
    UClass* NextClass = Class;
    if(NextClass && includeSelf)
    {
        ClassHierarchy.Add(NextClass);
    }
    while(NextClass && NextClass->GetSuperClass())
    {
        ClassHierarchy.Add(NextClass->GetSuperClass());
        NextClass = NextClass->GetSuperClass();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UObject* URyRuntimeObjectHelpers::GetClassDefaultObject(TSubclassOf<UObject> theClass)
{
#if RY_INCLUDE_DANGEROUS_FUNCTIONS
    if(!theClass)
    {
        return nullptr;
    }

    return theClass->GetDefaultObject();
#else
    return nullptr;
#endif
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool URyRuntimeObjectHelpers::SetObjectPropertyValue(UObject* object, const FName PropertyName, const FString& Value, const bool PrintWarnings)
{
#if RY_INCLUDE_DANGEROUS_FUNCTIONS
    if(!object)
    {
        return false;
    }

#if ENGINE_MINOR_VERSION < 25
    UProperty *FoundProperty = object->GetClass()->FindPropertyByName(PropertyName);
#else
    FProperty *FoundProperty = object->GetClass()->FindPropertyByName(PropertyName);
#endif
    if(FoundProperty)
    {
        void *PropertyPtr = FoundProperty->ContainerPtrToValuePtr<void>(object);
        check(PropertyPtr);
#if ENGINE_MINOR_VERSION < 25
        if(UNumericProperty *pIntProp = Cast<UNumericProperty>(FoundProperty))
#else
        if(FNumericProperty *pIntProp = CastField<FNumericProperty>(FoundProperty))
#endif
        {
            if(Value.IsNumeric())
            {
                pIntProp->SetNumericPropertyValueFromString(PropertyPtr, *Value);
                return true;
            }
            else
            {
                if(PrintWarnings)
                {
                    UE_LOG(LogRyRuntime, Warning, TEXT("SetObjectPropertyValue: Property named '%s' is numeric but the Value string is not"), *PropertyName.ToString());
                }
                return false;
            }
        }
#if ENGINE_MINOR_VERSION < 25
        else if(UBoolProperty *pBoolProp = Cast<UBoolProperty>(FoundProperty))
#else
        else if(FBoolProperty *pBoolProp = CastField<FBoolProperty>(FoundProperty))
#endif
        {
            pBoolProp->SetPropertyValue(PropertyPtr, FCString::ToBool(*Value));
            return true;
        }
#if ENGINE_MINOR_VERSION < 25
        else if(UStructProperty* StructProperty = Cast<UStructProperty>(FoundProperty))
#else
        else if(FStructProperty* StructProperty = CastField<FStructProperty>(FoundProperty))
#endif
        {
            FName StructType = StructProperty->Struct->GetFName();
            if(StructType == NAME_LinearColor)
            {
                FLinearColor *pCol = (FLinearColor*)PropertyPtr;
                return pCol->InitFromString(Value);
            }
            else if(StructType == NAME_Color)
            {
                FColor *pCol = (FColor*)PropertyPtr;
                return pCol->InitFromString(Value);
            }
            else if(StructType == NAME_Vector)
            {
                FVector *pVec = (FVector*)PropertyPtr;
                return pVec->InitFromString(Value);
            }
            else if(StructType == NAME_Rotator)
            {
                FRotator *pRot = (FRotator*)PropertyPtr;
                return pRot->InitFromString(Value);
            }
            else if(StructType == NAME_Transform)
            {
                FTransform *pTrans = (FTransform*)PropertyPtr;
                return pTrans->InitFromString(Value);
            }
        }

        if(PrintWarnings)
        {
            UE_LOG(LogRyRuntime, Warning, TEXT("SetObjectPropertyValue: Unsupported property named '%s'"), *PropertyName.ToString());
        }
    }
    else if(PrintWarnings)
    {
        UE_LOG(LogRyRuntime, Warning, TEXT("SetObjectPropertyValue: Unable to find property in object named '%s'"), *PropertyName.ToString());
    }
#endif // RY_INCLUDE_DANGEROUS_FUNCTIONS

    return false;
}
