// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <array>
#include <queue>
#include <numeric>
#include <string>
#include <unordered_map>
#include <ICryMannequin.h>
#include <CrySchematyc/Utils/EnumFlags.h>
#include <CrySystem/ConsoleRegistration.h>
#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>
#include <CryActionCVars.h>
#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryFlowGraph/IFlowSystem.h>

#include "StdAfx.h"
#include "GamePlugin.h"



class KeyMapper
{
public:
	// Converts an EKeyId to a user-friendly key name (e.g., eKI_W -> "W").
	static std::string KeyIdToUserFriendlyName(EKeyId keyId);

	// Converts a user-friendly key name to an EKeyId (e.g., "W" -> eKI_W).
	static EKeyId UserFriendlyNameToKeyId(const std::string& keyName);

private:
	// Initializes the mapping between EKeyId and user-friendly names.
	static void InitializeKeyMappings();

	static std::unordered_map<EKeyId, std::string> s_keyIdToNameMap;
	static std::unordered_map<std::string, EKeyId> s_nameToKeyIdMap;
};

namespace primitive
{
	struct capsule;
}

namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CCharacterControllerComponent;
	class CAdvancedAnimationComponent;
}

////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
public:

	struct SDefaultScopeSettings
	{
		string m_controllerDefinitionPath;
		string m_contextName;
		string m_fragmentName;
	};

	


	enum class EPlayerState
	{
		Walking,
		Sprinting,
		Jump,
		Idle
	};

	enum class EPlayerStance
	{
		Standing,
		Crouching
	};



private:
	static constexpr float DEFAULT_SPEED_WALKING = 2;
	static constexpr float DEFAULT_SPEED_RUNNING = 5;
	static constexpr float DEFAULT_JUMP_HEIGHT = 3;
	static constexpr float DEFAULT_ROTATION_SPEED = 0.002;
	static constexpr float DEFAULT_CAMERA_HEIGHT_STANDING = 1.7;
	static constexpr float DEFAULT_CAMERA_HEIGHT_CROUCHING = 1.0;
	static constexpr float DEFAULT_CAPSULE_HEIGHT_STANDING = 1.6;
	static constexpr float DEFAULT_CAPSULE_HEIGHT_CROUCHING = 0.75;
	static constexpr float DEFAULT_CAPSULE_HEIGHT_OFFSET = 0.2;
	static constexpr float DEFAULT_ROT_LIMIT_PITCH_MAX = -1.1;
	static constexpr float DEFAULT_ROT_LIMIT_PITCH_MIN = 1.5;
	static constexpr EPlayerState DEFAULT_PLAYER_STATE = EPlayerState::Walking;
	static constexpr EPlayerStance DEFAULT_PLAYER_STANCE = EPlayerStance::Standing;

	//Cry::DefaultComponents::CInputComponent* m_pInputComponent; // Declare the input component

public:

	CPlayerComponent();
	virtual ~CPlayerComponent() override {};


	virtual void Initialize() override;
	
	void CheckAnimationState();

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;

	float m_movementSpeed;


	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetEditorCategory("Player");
		desc.SetLabel("Player Controller");
		desc.SetDescription("Creates a player controller");

		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
		desc.AddMember(&CPlayerComponent::m_WalkSpeed, 'pws', "playerwalkspeed", "Player Walk Speed", "Sets the Player Walk Speed", DEFAULT_SPEED_WALKING);
		desc.AddMember(&CPlayerComponent::m_RunSpeed, 'prs', "playerrunspeed", "Player Run Speed", "Sets the Player Run Speed", DEFAULT_SPEED_RUNNING);
		desc.AddMember(&CPlayerComponent::m_JumpHeight, 'pjh', "playejumpheight", "Player Jump Height", "Sets the Player Jump Height", DEFAULT_JUMP_HEIGHT);
		desc.AddMember(&CPlayerComponent::m_RotationSpeed, 'pros', "playerrotationspeed", "Player Rotation Speed", "Sets the Player Rotation Speed", DEFAULT_ROTATION_SPEED);
		desc.AddMember(&CPlayerComponent::m_CameraOffsetStanding, 'cos', "cameraoffsetstanding", "Camera Offset Standing", "Sets up Camera Offset While Standing", Vec3(0.f,0.f, DEFAULT_CAMERA_HEIGHT_STANDING));
		desc.AddMember(&CPlayerComponent::m_CameraOffsetCrouching, 'camc', "cameraoffsetcrouching", "Camera Offset Crouching", "Sets up Camera Offset While Crouching", Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_CROUCHING));
		desc.AddMember(&CPlayerComponent::m_CapsuleHeightStanding, 'caps', "capsuleheightstanding", "Capsule Height Standing", "Sets up Capsule Height While Standing", DEFAULT_CAPSULE_HEIGHT_STANDING);
		desc.AddMember(&CPlayerComponent::m_CapsuleHeightCrouching, 'capc', "capsuleheightcrouching", "Capsule Height Crouching", "Sets up Capsule Height While Crouching", DEFAULT_CAPSULE_HEIGHT_CROUCHING);
		desc.AddMember(&CPlayerComponent::m_CapsuleGroundOffset, 'capo', "capsulegroundoffset", "Capsule Ground Offset", "Sets up Capsule Ground Offset", DEFAULT_CAPSULE_HEIGHT_OFFSET);
		desc.AddMember(&CPlayerComponent::m_RotationLimitsMaxPitch, 'cpm', "camerapitchmax", "Camera Pitch Max", "Maximum Rotation Value for Camera Pitch", DEFAULT_ROT_LIMIT_PITCH_MAX);
		desc.AddMember(&CPlayerComponent::m_RotationLimitsMinPitch, 'cpmi', "camerapitchmin", "Camera Pitch Min", "Minimum Rotation Value for Camera Pitch", DEFAULT_ROT_LIMIT_PITCH_MIN);
		desc.AddMember(&CPlayerComponent::m_AnimationIdle, 'ani', "animationidle", "Idle Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationWalk, 'anw', "animationwalk", "Walk Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationIdle, 'ani', "animationidle", "Idle Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationRun, 'anr', "animationrun", "Run Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationJump, 'anj', "animationjump", "Jump Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationLeft, 'anl', "animationleft", "Left Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationRight, 'anr', "animationright", "Right Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		// Added members for the requested variables
		desc.AddMember(&CPlayerComponent::m_AnimationCrouch, 'anc', "animationcrouch", "Crouch Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationCrouchIdle, 'anci', "animationcrouchidle", "Crouch Idle Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationCroucToStand, 'anct', "animationcrouchtostand", "Crouch to Stand Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationStandToCrouch, 'anst', "animationstandtocrouch", "Stand to Crouch Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationWalkLeft, 'anwl', "animationwalkleft", "Walk Left Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationWalkRight, 'anwr', "animationwalkright", "Walk Right Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationRunLeft, 'anrl', "animationrunleft", "Run Left Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationRunRight, 'anrr', "animationrunright", "Run Right Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationCrouchLeft, 'ancl', "animationcrouchleft", "Crouch Left Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationCrouchRight, 'ancr', "animationcrouchright", "Crouch Right Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationCrouchWalk, 'ancw', "animationcrouchwalk", "Crouch Walk Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
		desc.AddMember(&CPlayerComponent::m_AnimationCrouchBack, 'ancb', "animationcrouchback", "Crouch Back Animation", "Set Up the Animation from Mannequin", Schematyc::CSharedString());
	}

	
protected:
	void InitializeInput();
	void Reset();

	void UpdateMovement();
	void UpdateRotation();
	void UpdateCamera(float frametime);
	void RecenterCollider();
	

	void TryUpdateStance();
	bool IsCapsuleIntersectingGeometry(const primitives::capsule& capsule) const;

public:
	// Coponent Reference

	// Camera
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent;

	// Input
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;

	// Physics (Character Controller)
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterControllerComponent;

	// Mesh&Animation (Advanced Animation Component)
	Cry::DefaultComponents::CAdvancedAnimationComponent* m_pAdvancedAnimationComponent;



	// Runtime Variable
	Quat m_CurrentYaw;
	float m_CurrentPitch;
	Vec2 m_movementDelta;
	Vec2 m_MouseDeltaRotation;
	EPlayerState m_currentPlayerState;
	EPlayerStance m_currentPlayerStance;
	EPlayerStance m_desiredPlayerStance;
	Vec3 m_CameraEndOffset;

	// Component Properties
	Vec3 m_CameraOffsetStanding;
	float m_RotationSpeed;
	float m_RotationLimitsMinPitch;
	float m_RotationLimitsMaxPitch;
	float m_RunSpeed;
	float m_WalkSpeed;
	float m_JumpHeight;
	Vec3 m_CameraOffsetCrouching;
	float m_CapsuleHeightStanding;
	float m_CapsuleHeightCrouching;
	float m_CapsuleGroundOffset;
	
	// Animation State
	float m_Walk = 0;
	float m_Left = 0;
	float m_Right = 0;
	float m_Run = 0;
	float m_Crouch = 0;
	float m_Back = 0;

	// Animation Names
	Schematyc::CSharedString m_AnimationIdle;
	Schematyc::CSharedString m_AnimationWalk;
	Schematyc::CSharedString m_AnimationBack;
	Schematyc::CSharedString m_AnimationRun;
	Schematyc::CSharedString m_AnimationJump;
	Schematyc::CSharedString m_AnimationLeft;
	Schematyc::CSharedString m_AnimationRight;
	Schematyc::CSharedString m_AnimationCrouch;
	Schematyc::CSharedString m_AnimationCrouchIdle;
	Schematyc::CSharedString m_AnimationCroucToStand;
	Schematyc::CSharedString m_AnimationStandToCrouch;
	Schematyc::CSharedString m_AnimationWalkLeft;
	Schematyc::CSharedString m_AnimationWalkRight;
	Schematyc::CSharedString m_AnimationRunLeft;
	Schematyc::CSharedString m_AnimationRunRight;
	Schematyc::CSharedString m_AnimationCrouchLeft;
	Schematyc::CSharedString m_AnimationCrouchRight;
	Schematyc::CSharedString m_AnimationCrouchWalk;
	Schematyc::CSharedString m_AnimationCrouchBack;

	private:
		// Map to store surface types and their corresponding audio triggers
		std::unordered_map<std::string, std::string> m_surfaceTypes;

		// Private methods
		void LoadSurfaceTypes();
		void OnFootstepEvent(const char* eventName);
};


/*
	------------------------
	Flow Graph Nodes Classes
	------------------------
*/

class CFlowNode_ChangeInputBinding : public CFlowBaseNode<eNCT_Singleton>
{
public:


	CFlowNode_ChangeInputBinding(SActivationInfo* pActInfo, CPlayerComponent* pPlayerComponent = nullptr);

	virtual void GetConfiguration(SFlowNodeConfig& config) override;
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;
	virtual void GetMemoryUsage(ICrySizer* sizer) const override;

private:
	bool RebindAction(const string& actionName, const string& newKey);

	CPlayerComponent* m_pPlayerComponent; // Pointer to the player component
};

class CFlowNode_TriggerCustomAnimation : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_TriggerCustomAnimation(SActivationInfo* pActInfo);

	// FlowGraph node configuration
	virtual void GetConfiguration(SFlowNodeConfig& config) override;

	// Event processing
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;

	// Memory usage reporting
	virtual void GetMemoryUsage(ICrySizer* sizer) const override;

private:
	CPlayerComponent* m_pPlayerComponent; // Reference to the player component
};