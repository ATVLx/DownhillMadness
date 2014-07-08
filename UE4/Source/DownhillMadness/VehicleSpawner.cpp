

#include "DownhillMadness.h"
#include "VehicleSpawner.h"


AVehicleSpawner::AVehicleSpawner(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	this->Root = PCIP.CreateDefaultSubobject<USceneComponent>(this, FName(TEXT("Root")));
	this->RootComponent = this->Root;

	this->bodyClass = nullptr;
	this->wheelClasses = TArray<FWheelClass>();
	this->wheelClasses.Empty();
	this->weightClasses = TArray<FWeightClass>();
	this->weightClasses.Empty();
	this->steeringClass = nullptr;
	this->brakeClass = nullptr;
}


// ----------------------------------------------------------------------------


void AVehicleSpawner::SerializeVehicle(AVehicleBodyBase* vehicle)
{
	if (vehicle == nullptr)
		return;

	this->bodyClass = vehicle->GetClass();
	FMatrix bodyMatrix = vehicle->Body->GetComponenTransform().ToMatrixNoScale();

	for (TArray<AVehicleWheelBase*>::TIterator wheelIter(vehicle->attachedWheels); wheelIter; ++wheelIter)
	{
		AVehicleWheelBase* currentWheel = *wheelIter;
		FMatrix wheelMatrix = currentWheel->WheelConstraint->GetComponenTransform().ToMatrixNoScale();
		FMatrix relativeMatrix = wheelMatrix * bodyMatrix.InverseSafe();

		this->wheelClasses.Add(FWheelClass(currentWheel->GetClass(), currentWheel->bIsSteerable, currentWheel->bHasBrake, relativeMatrix));
	}

	for (TArray<AVehicleWeightBase*>::TIterator weightIter(vehicle->attachedWeights); weightIter; ++weightIter)
	{
		AVehicleWeightBase* currentWeight = *weightIter;
		FMatrix weightMatrix = currentWeight->PhysicsConstraint->GetComponenTransform().ToMatrixNoScale();
		FMatrix relativeMatrix = weightMatrix * bodyMatrix.InverseSafe();

		this->weightClasses.Add(FWeightClass(currentWeight->GetClass(), relativeMatrix));
	}

	if (vehicle->attachedSteering != nullptr)
		this->steeringClass = vehicle->attachedSteering->GetClass();

	if (vehicle->attachedBrake != nullptr)
		this->brakeClass = vehicle->attachedBrake->GetClass();
}


// ----------------------------------------------------------------------------


AVehicleBodyBase* AVehicleSpawner::SpawnVehicle(FVector spawnLocation, FRotator spawnRotation)
{
	if (this->bodyClass != nullptr)
	{
		FActorSpawnParameters spawnParameters;

		AVehicleBodyBase* vehicleBody = (AVehicleBodyBase*)(this->GetWorld()->SpawnActor(this->bodyClass, &spawnLocation, &spawnRotation, spawnParameters));

		if (vehicleBody != nullptr)
		{
			FMatrix bodyMatrix = FRotationTranslationMatrix(spawnRotation, spawnLocation);

			for (TArray<FWheelClass>::TIterator wheelIter(this->wheelClasses); wheelIter; ++wheelIter)
			{
				FWheelClass currentWheel = *wheelIter;
				FMatrix wheelMatrix = currentWheel.relativeWheelMatrix * bodyMatrix;

				FVector wheelSpawnPos = wheelMatrix.GetOrigin();
				FRotator wheelSpawnRotation = wheelMatrix.Rotator();

				AVehicleWheelBase* vehicleWheel = (AVehicleWheelBase*)(this->GetWorld()->SpawnActor(currentWheel.classInstance, &wheelSpawnPos, &wheelSpawnRotation, spawnParameters));
				if (currentWheel.isSteerable != 0)
					vehicleWheel->bIsSteerable = true;
				else
					vehicleWheel->bIsSteerable = false;
				if (currentWheel.hasBrake != 0)
					vehicleWheel->bHasBrake = true;
				else
					vehicleWheel->bHasBrake = false;

				vehicleBody->AttachWheel(vehicleWheel);
			}

			for (TArray<FWeightClass>::TIterator weightIter(this->weightClasses); weightIter; ++weightIter)
			{
				FWeightClass currentWeight = *weightIter;
				FMatrix weightMatrix = currentWeight.relativeWeightMatrix * bodyMatrix;

				FVector weightSpawnPos = weightMatrix.GetOrigin();
				FRotator weightSpawnRotation = weightMatrix.Rotator();

				AVehicleWeightBase* vehicleWeight = (AVehicleWeightBase*)(this->GetWorld()->SpawnActor(currentWeight.classInstance, &weightSpawnPos, &weightSpawnRotation, spawnParameters));

				vehicleBody->AttachWeight(vehicleWeight);
			}

			if (this->steeringClass != nullptr)
			{
				AVehicleSteeringBase* vehicleSteering = (AVehicleSteeringBase*)(this->GetWorld()->SpawnActor(this->steeringClass, &spawnLocation, &spawnRotation, spawnParameters));
				vehicleBody->AttachSteering(vehicleSteering);
			}

			if (this->brakeClass != nullptr)
			{
				AVehicleBrakeBase* vehicleBrake = (AVehicleBrakeBase*)(this->GetWorld()->SpawnActor(this->brakeClass, &spawnLocation, &spawnRotation, spawnParameters));
				vehicleBody->AttachBrake(vehicleBrake);
			}

			return vehicleBody;
		}
	}

	return nullptr;
}


// ----------------------------------------------------------------------------


void AVehicleSpawner::SaveLoadData(FArchive& archive)
{
	if (archive.IsLoading())
	{
		FString className;
		archive << className;
		this->bodyClass = FObjectReaderFix::FindClass(className);
	}
	else
	{
		FString className = this->bodyClass->GetFName().ToString();
		archive << className;
	}

	int numWheels = this->wheelClasses.Num();
	archive << numWheels;
	this->wheelClasses.Reserve(numWheels);
	for (int i = 0; i < numWheels; i++)
	{
		if (i >= this->wheelClasses.Num())
			this->wheelClasses.Add(FWheelClass());
		archive << this->wheelClasses[i];
	}

	int numWeights = this->weightClasses.Num();
	archive << numWeights;
	this->weightClasses.Reserve(numWeights);
	for (int i = 0; i < numWeights; i++)
	{
		if (i >= this->weightClasses.Num())
			this->weightClasses.Add(FWeightClass());
		archive << this->weightClasses[i];
	}

	if (archive.IsLoading())
	{
		FString className;
		archive << className;
		this->steeringClass = FObjectReaderFix::FindClass(className);
	}
	else
	{
		FString className = this->steeringClass->GetFName().ToString();
		archive << className;
	}

	if (archive.IsLoading())
	{
		FString className;
		archive << className;
		this->brakeClass = FObjectReaderFix::FindClass(className);
	}
	else
	{
		FString className = this->brakeClass->GetFName().ToString();
		archive << className;
	}
}


// ----------------------------------------------------------------------------


bool AVehicleSpawner::SaveToFile(const FString& filePath)
{
	TArray<uint8> dataArray;
	UScriptStruct* dummyObject = FWheelClass::StaticStruct();
	FObjectWriterFix archive(dataArray);
	this->SaveLoadData(archive);

	if (dataArray.Num() <= 0) return false;

	if (FFileHelper::SaveArrayToFile(dataArray, *filePath))
	{
		archive.FlushCache();
		dataArray.Empty();

		return true;
	}

	archive.FlushCache();
	dataArray.Empty();

	return false;
}


// ----------------------------------------------------------------------------


bool AVehicleSpawner::LoadFromFile(const FString& filePath)
{
	TArray<uint8> binaryArray;

	if (!FFileHelper::LoadFileToArray(binaryArray, *filePath))
	{
		return false;
	}

	if (binaryArray.Num() <= 0) return false;

	FObjectReaderFix archive(binaryArray);
	archive.Seek(0);
	this->SaveLoadData(archive);

	archive.FlushCache();

	binaryArray.Empty();
	archive.Close();

	return true;
}


// ----------------------------------------------------------------------------


bool AVehicleSpawner::LoadFromArray(const TArray<uint8>& inArray)
{
	TArray<uint8> binaryArray(inArray);

	if (binaryArray.Num() <= 0) return false;

	FObjectReaderFix archive(binaryArray);
	archive.Seek(0);
	this->SaveLoadData(archive);

	archive.FlushCache();

	binaryArray.Empty();
	archive.Close();

	return true;
}


// ----------------------------------------------------------------------------


void AVehicleSpawner::LoadStaticVehicle(uint8 index)
{
	TArray<uint8> vehicleData;
	vehicleData.Empty();
	uint8* fillData = nullptr;

	int32 numElements = 0;

	switch (index)
	{
	case 0:
		numElements = 663;
		fillData = new uint8[numElements]{ 0x12, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x42, 0x6F, 0x64, 0x79, 0x5F, 0x43, 0x00, 0x04, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x0B, 0x11, 0x8D, 0xB6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x11, 0x8D, 0x36, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x42, 0x64, 0x00, 0xF0, 0x41, 0x00, 0x44, 0xA7, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x1A, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x0B, 0x11, 0x8D, 0xB6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x11, 0x8D, 0x36, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC2, 0x92, 0xFF, 0xEF, 0x41, 0x00, 0x44, 0xA7, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x1A, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x16, 0x22, 0xCA, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x22, 0xCA, 0xB6, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x42, 0x9C, 0xFF, 0xEF, 0xC1, 0x00, 0x44, 0xA7, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x1A, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x16, 0x22, 0xCA, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x22, 0xCA, 0xB6, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC2, 0x6E, 0x00, 0xF0, 0xC1, 0x00, 0x44, 0xA7, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x57, 0x65, 0x69, 0x67, 0x68, 0x74, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x0B, 0x11, 0x8D, 0xB6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x11, 0x8D, 0x36, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9A, 0x11, 0xC2, 0xA0, 0x00, 0x20, 0xC1, 0x00, 0x00, 0xCC, 0xBD, 0x00, 0x00, 0x80, 0x3F, 0x14, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x57, 0x65, 0x69, 0x67, 0x68, 0x74, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x0B, 0x11, 0x8D, 0xB6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x11, 0x8D, 0x36, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9A, 0x11, 0xC2, 0x60, 0xFF, 0x1F, 0x41, 0x00, 0x00, 0xCC, 0xBD, 0x00, 0x00, 0x80, 0x3F, 0x16, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x53, 0x74, 0x65, 0x65, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x43, 0x00, 0x13, 0x00, 0x00, 0x00, 0x54, 0x65, 0x73, 0x74, 0x56, 0x65, 0x68, 0x69, 0x63, 0x6C, 0x65, 0x42, 0x72, 0x61, 0x6B, 0x65, 0x5F, 0x43, 0x00 };

		vehicleData.Append(fillData, numElements);

		delete[] fillData;

		this->LoadFromArray(vehicleData);

		vehicleData.Empty();

		break;

	case 1:
		numElements = 456;
		fillData = new uint8[numElements]{ 0x11, 0x00, 0x00, 0x00, 0x44, 0x72, 0x61, 0x67, 0x6F, 0x6E, 0x42, 0x6F, 0x61, 0x74, 0x42, 0x6F, 0x64, 0x79, 0x5F, 0x43, 0x00, 0x04, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xC1, 0xB8, 0x18, 0xF3, 0xC1, 0x40, 0xC8, 0x8C, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8C, 0x42, 0xB8, 0x18, 0xF3, 0xC1, 0x40, 0xC8, 0x8C, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xC1, 0xB8, 0x18, 0xF3, 0x41, 0x40, 0xC8, 0x8C, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8C, 0x42, 0xB8, 0x18, 0xF3, 0x41, 0x40, 0xC8, 0x8C, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x42, 0x6F, 0x61, 0x74, 0x53, 0x74, 0x65, 0x65, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x43, 0x00, 0x10, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74, 0x42, 0x72, 0x61, 0x6B, 0x65, 0x73, 0x5F, 0x43, 0x00 };

		vehicleData.Append(fillData, numElements);

		delete[] fillData;

		this->LoadFromArray(vehicleData);

		vehicleData.Empty();

		break;

	case 2:
		numElements = 442;
		fillData = new uint8[numElements]{ 0x0B, 0x00, 0x00, 0x00, 0x46, 0x69, 0x73, 0x68, 0x42, 0x6F, 0x64, 0x79, 0x5F, 0x43, 0x00, 0x04, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0xC2, 0x0C, 0x59, 0xEF, 0xC1, 0x80, 0x3D, 0x73, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x42, 0x0C, 0x59, 0xEF, 0xC1, 0x80, 0x3D, 0x73, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0xC2, 0x0C, 0x59, 0xEF, 0x41, 0x80, 0x3D, 0x73, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x42, 0x0C, 0x59, 0xEF, 0x41, 0x80, 0x3D, 0x73, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x42, 0x6F, 0x61, 0x74, 0x53, 0x74, 0x65, 0x65, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x43, 0x00, 0x10, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74, 0x42, 0x72, 0x61, 0x6B, 0x65, 0x73, 0x5F, 0x43, 0x00 };

		vehicleData.Append(fillData, numElements);

		delete[] fillData;

		this->LoadFromArray(vehicleData);

		vehicleData.Empty();

		break;

	case 3:
		numElements = 452;
		fillData = new uint8[numElements]{ 0x0D, 0x00, 0x00, 0x00, 0x48, 0x6F, 0x74, 0x52, 0x6F, 0x64, 0x42, 0x6F, 0x64, 0x79, 0x5F, 0x43, 0x00, 0x04, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x48, 0x6F, 0x74, 0x52, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xC1, 0xD0, 0x19, 0x19, 0xC2, 0x20, 0x7F, 0x13, 0xC2, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x48, 0x6F, 0x74, 0x52, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB6, 0x42, 0xD0, 0x19, 0x19, 0xC2, 0x20, 0x7F, 0x13, 0xC2, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x48, 0x6F, 0x74, 0x52, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xC1, 0xD0, 0x19, 0x19, 0x42, 0x20, 0x7F, 0x13, 0xC2, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x48, 0x6F, 0x74, 0x52, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB6, 0x42, 0xD0, 0x19, 0x19, 0x42, 0x20, 0x7F, 0x13, 0xC2, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x42, 0x6F, 0x61, 0x74, 0x53, 0x74, 0x65, 0x65, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x43, 0x00, 0x10, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74, 0x42, 0x72, 0x61, 0x6B, 0x65, 0x73, 0x5F, 0x43, 0x00 };

		vehicleData.Append(fillData, numElements);

		delete[] fillData;

		this->LoadFromArray(vehicleData);

		vehicleData.Empty();

		break;

	case 4:
		numElements = 613;
		fillData = new uint8[numElements]{ 0X0F, 0x00, 0x00, 0x00, 0x4D, 0x6A, 0x6F, 0x65, 0x6C, 0x6E, 0x69, 0x72, 0x42, 0x6F, 0x64, 0x79, 0x5F, 0x43, 0x00, 0x04, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C, 0xC2, 0xE0, 0x4B, 0xB1, 0xC2, 0x80, 0x00, 0x20, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x94, 0x42, 0xE0, 0x4B, 0xB1, 0xC2, 0x80, 0x00, 0x20, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C, 0xC2, 0xE0, 0x4B, 0xB1, 0x42, 0x80, 0x00, 0x20, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x15, 0x00, 0x00, 0x00, 0x53, 0x68, 0x69, 0x65, 0x6C, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x94, 0x42, 0xE0, 0x4B, 0xB1, 0x42, 0x80, 0x00, 0x20, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x42, 0x6F, 0x61, 0x74, 0x53, 0x74, 0x65, 0x65, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x43, 0x00, 0x10, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74, 0x42, 0x72, 0x61, 0x6B, 0x65, 0x73, 0x5F, 0x43, 0x00 };

		vehicleData.Append(fillData, numElements);

		delete[] fillData;

		this->LoadFromArray(vehicleData);

		vehicleData.Empty();

		break;

	case 5:
		numElements = 663;
		fillData = new uint8[numElements]{ 0x0E, 0x00, 0x00, 0x00, 0x57, 0x61, 0x73, 0x68, 0x54, 0x75, 0x62, 0x42, 0x6F, 0x64, 0x79, 0x5F, 0x43, 0x00, 0x04, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0xC2, 0x20, 0x71, 0x1D, 0xC2, 0x00, 0x9E, 0xA1, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8A, 0x42, 0x20, 0x71, 0x1D, 0xC2, 0x00, 0x9E, 0xA1, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0xC2, 0x10, 0x71, 0x1D, 0x42, 0x00, 0x9E, 0xA1, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x13, 0x00, 0x00, 0x00, 0x57, 0x6F, 0x6F, 0x64, 0x43, 0x61, 0x70, 0x73, 0x75, 0x6C, 0x65, 0x57, 0x68, 0x65, 0x65, 0x6C, 0x5F, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x5A, 0x88, 0x68, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x88, 0x68, 0x35, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8A, 0x42, 0x10, 0x71, 0x1D, 0x42, 0x00, 0x9E, 0xA1, 0xC1, 0x00, 0x00, 0x80, 0x3F, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x4D, 0x61, 0x74, 0x74, 0x6F, 0x63, 0x6B, 0x57, 0x65, 0x69, 0x67, 0x68, 0x74, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x80, 0x33, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x41, 0x20, 0x71, 0x19, 0xC2, 0x00, 0x00, 0x68, 0x3E, 0x00, 0x00, 0x80, 0x3F, 0x10, 0x00, 0x00, 0x00, 0x4D, 0x61, 0x74, 0x74, 0x6F, 0x63, 0x6B, 0x57, 0x65, 0x69, 0x67, 0x68, 0x74, 0x5F, 0x43, 0x00, 0x00, 0x00, 0x80, 0x33, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x2E, 0xBD, 0xBB, 0x33, 0xFE, 0xFF, 0x7F, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xBF, 0x2E, 0xBD, 0xBB, 0xB3, 0x00, 0x00, 0x80, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x41, 0x10, 0x71, 0x19, 0x42, 0x00, 0x00, 0x68, 0x3E, 0x00, 0x00, 0x80, 0x3F, 0x0F, 0x00, 0x00, 0x00, 0x42, 0x6F, 0x61, 0x74, 0x53, 0x74, 0x65, 0x65, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x43, 0x00, 0x10, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74, 0x42, 0x72, 0x61, 0x6B, 0x65, 0x73, 0x5F, 0x43, 0x00 };

		vehicleData.Append(fillData, numElements);

		delete[] fillData;

		this->LoadFromArray(vehicleData);

		vehicleData.Empty();

		break;
	}
}


// ----------------------------------------------------------------------------


UClass* FObjectReaderFix::FindClass(const FString& className)
{
	TObjectIterator<UClass> allClasses = TObjectIterator<UClass>();

	while (allClasses)
	{
		if (allClasses->GetFName().ToString() == className)
			return *allClasses;
		++allClasses;
	}

	return nullptr;
}


// ----------------------------------------------------------------------------


FWheelClass::FWheelClass()
{
	this->classInstance = nullptr;
	this->isSteerable = 0;
	this->hasBrake = 0;
	this->relativeWheelMatrix = FMatrix::Identity;
}


// ----------------------------------------------------------------------------


FWeightClass::FWeightClass()
{
	this->classInstance = nullptr;
	this->relativeWeightMatrix = FMatrix::Identity;
}


// ----------------------------------------------------------------------------


FWheelClass::FWheelClass(UClass* classInstance, bool bIsSteerable, bool bHasBrake, const FMatrix& relativeWheelMatrix)
{
	this->classInstance = classInstance;
	if (bIsSteerable)
		this->isSteerable = 1;
	else
		this->isSteerable = 0;
	if (bHasBrake)
		this->hasBrake = 1;
	else
		this->hasBrake = 0;
	this->relativeWheelMatrix = relativeWheelMatrix;
}


// ----------------------------------------------------------------------------


FWeightClass::FWeightClass(UClass* classInstance, const FMatrix& relativeWeightMatrix)
{
	this->classInstance = classInstance;
	this->relativeWeightMatrix = relativeWeightMatrix;
}


