// Copyright 2017-2020 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Player.h"
#include "GamePlugin.h"

#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>

#include <CrySystem/ISystem.h>
#include <CryInput/IInput.h>

#include <CryFlowGraph/IFlowSystem.h>
#include <CryFlowGraph/IFlowBaseNode.h>

#include <CrySystem/XML/IXml.h>
#include <unordered_map>

#include <CryAudio/IAudioSystem.h>
#include <Cry3DEngine/ISurfaceType.h>
#include <Cry3DEngine/IMaterial.h>
#include <string>



namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}


CPlayerComponent::CPlayerComponent()
	:
	m_pCameraComponent(nullptr),
	m_pInputComponent(nullptr),
	m_pCharacterControllerComponent(nullptr),
	m_CurrentYaw(IDENTITY),
	m_CurrentPitch(0.f),
	m_movementDelta(ZERO),
	m_MouseDeltaRotation(ZERO),
	m_currentPlayerState(DEFAULT_PLAYER_STATE),
	m_currentPlayerStance(DEFAULT_PLAYER_STANCE),
	m_desiredPlayerStance(DEFAULT_PLAYER_STANCE),
	m_CapsuleGroundOffset(DEFAULT_CAPSULE_HEIGHT_OFFSET),
	m_CameraOffsetCrouching(Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_CROUCHING)),
	m_CapsuleHeightStanding(DEFAULT_CAPSULE_HEIGHT_STANDING),
	m_CapsuleHeightCrouching(DEFAULT_CAPSULE_HEIGHT_CROUCHING),
	m_CameraEndOffset(Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_STANDING)),
	m_CameraOffsetStanding(Vec3(0.f, 0.f, DEFAULT_CAMERA_HEIGHT_STANDING)),
	m_RotationSpeed(DEFAULT_ROTATION_SPEED),
	m_WalkSpeed(DEFAULT_SPEED_WALKING),
	m_RunSpeed(DEFAULT_SPEED_RUNNING),
	m_JumpHeight(DEFAULT_JUMP_HEIGHT),
	m_RotationLimitsMaxPitch(DEFAULT_ROT_LIMIT_PITCH_MAX),
	m_RotationLimitsMinPitch(DEFAULT_ROT_LIMIT_PITCH_MIN)
{

}

/*
	-------------------------------------------
	Footstep Sounds & Surface Types Recognition
	-------------------------------------------
*/

void CPlayerComponent::LoadSurfaceTypes()
{
	// Retrieve the assets folder name from the sys_game_folder cvar
	const ICVar* pGameFolderCVar = gEnv->pConsole->GetCVar("sys_game_folder");
	if (!pGameFolderCVar)
	{
		CryLogAlways("Failed to retrieve sys_game_folder cvar.");
		return;
	}

	const string gameFolder = pGameFolderCVar->GetString();
	if (gameFolder.empty())
	{
		CryLogAlways("sys_game_folder cvar is empty.");
		return;
	}

	// Construct the path to SurfaceTypes.xml
	const string surfaceTypesPath = gameFolder + "/libs/MaterialEffects/SurfaceTypes.xml";

	// Load the XML file
	XmlNodeRef root = gEnv->pSystem->LoadXmlFromFile(surfaceTypesPath.c_str());
	if (!root)
	{
		CryLogAlways("Failed to load SurfaceTypes.xml from path: %s", surfaceTypesPath.c_str());
		return;
	}

	// Parse the XML and populate the map
	for (int i = 0; i < root->getChildCount(); ++i)
	{
		XmlNodeRef surfaceNode = root->getChild(i);
		if (surfaceNode->isTag("SurfaceType"))
		{
			const char* surfaceName = nullptr;
			if (surfaceNode->getAttr("name", &surfaceName)) // Ensure the second argument matches the expected type
			{
				std::string surfaceNameStr = surfaceName; // Ensure surfaceNameStr is a std::string
				if (surfaceNameStr.find("mat_") == 0) {
					surfaceNameStr = surfaceNameStr.substr(4); // Remove "mat_"
				}
				m_surfaceTypes[surfaceNameStr] = "pl_footsteps/" + surfaceNameStr;
			}
		}
	}

	CryLogAlways("Loaded %d surface types.", m_surfaceTypes.size());
}



void CPlayerComponent::OnFootstepEvent(const char* eventName)
{
	// Get the player's position
	const Vec3 playerPosition = m_pEntity->GetWorldPos();

	// Perform a raycast to detect the surface below the player
	ray_hit hit;
	const int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(
		playerPosition, Vec3(0, 0, -1) * 1.0f, // Cast a ray downward
		ent_all, rayFlags, &hit, 1))
	{
		// Get the surface type from the hit
		const ISurfaceType* pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(hit.surface_idx);
		if (!pSurfaceType)
		{
			CryLogAlways("Failed to retrieve surface type from raycast hit.");
			return;
		}

		const char* surfaceTypeName = pSurfaceType->GetName();
		std::string surfaceName = surfaceTypeName;
		if (surfaceName.find("mat_") == 0)
		{
			surfaceName = surfaceName.substr(4); // Remove "mat_"
		}

		// Find the corresponding audio trigger
		auto it = m_surfaceTypes.find(surfaceName);
		if (it == m_surfaceTypes.end())
		{
			CryLogAlways("No audio trigger found for surface type: %s", surfaceName.c_str());
			return;
		}

		const std::string& audioTriggerName = it->second;

		// Play the audio trigger
		if (gEnv->pAudioSystem)
		{
			CryAudio::ControlId audioTriggerId = CryAudio::StringToId(audioTriggerName.c_str());
			if (audioTriggerId != CryAudio::InvalidControlId)
			{
				gEnv->pAudioSystem->ExecuteTrigger(audioTriggerId, CryAudio::SRequestUserData::GetEmptyObject());
			}
			else
			{
				CryLogAlways("Invalid audio trigger: %s", audioTriggerName.c_str());
			}
		}
	}
	else
	{
		CryLogAlways("No surface detected below the player.");
	}
}


/*
	-------------------------------
	Player Component Initialization
	-------------------------------
*/

void CPlayerComponent::Initialize()
{
	m_pCameraComponent = m_pEntity->GetOrCreateComponent <Cry::DefaultComponents::CCameraComponent>();
	m_pInputComponent = m_pEntity->GetOrCreateComponent <Cry::DefaultComponents::CInputComponent>();
	m_pCharacterControllerComponent = m_pEntity->GetOrCreateComponent <Cry::DefaultComponents::CCharacterControllerComponent>();
	m_pAdvancedAnimationComponent = m_pEntity->GetOrCreateComponent <Cry::DefaultComponents::CAdvancedAnimationComponent>();
	m_pAdvancedAnimationComponent->SetDefaultScopeContextName("FirstPersonCharacter");
	m_pAdvancedAnimationComponent->SetMannequinAnimationDatabaseFile("Animations/Mannequin/ADB/FirstPerson.adb");
	m_pAdvancedAnimationComponent->SetControllerDefinitionFile("Animations/Mannequin/ADB/FirstPersonControllerDefinition.xml");
	m_pAdvancedAnimationComponent->SetDefaultFragmentName("Idle");
	m_pAdvancedAnimationComponent->LoadFromDisk();

	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();

	// Load surface types
	LoadSurfaceTypes();

	Reset();
}

void CPlayerComponent::RecenterCollider()
{
	static bool skip = false;
	if (skip)
	{
		skip = false;
		return;
	}

	auto PCharacterControllerComponent = m_pEntity->GetComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	if (PCharacterControllerComponent == nullptr)
	{
		return;
	}

	const auto& physParams = PCharacterControllerComponent->GetPhysicsParameters();
	float HeighOffset = physParams.m_height * 0.5f;

	if (physParams.m_bCapsule)
	{
		HeighOffset = HeighOffset * 0.5f / physParams.m_radius * 0.5f;
	}

	PCharacterControllerComponent->SetTransformMatrix(Matrix34(IDENTITY, Vec3(0.f, 0.f, 0.005f + HeighOffset)));

	skip = true;

	PCharacterControllerComponent->Physicalize();
}


void CPlayerComponent::Reset()
{
	// Reset Input
	m_movementDelta = ZERO;
	m_MouseDeltaRotation = ZERO;
	m_CurrentYaw = Quat::CreateRotationZ(m_pEntity->GetWorldRotation().GetRotZ());
	m_CurrentPitch = 0.f;

	// Reset Player State
	m_currentPlayerState = EPlayerState::Walking;

	InitializeInput();

	m_currentPlayerStance = EPlayerStance::Standing;
	m_desiredPlayerStance = m_currentPlayerStance;

	// Reset Camera Lerp
	m_CameraEndOffset = m_CameraOffsetStanding;
}

void CPlayerComponent::InitializeInput()
{
	m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value) 
		{
			m_movementDelta.y = value;
			if (activationMode == (int)eAAM_OnPress)
			{
				m_Walk = 1;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationIdle);
				m_Walk = 0;
			}
		});
	m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, eKI_W);

	m_pInputComponent->RegisterAction("player", "moveback", [this](int activationMode, float value) 
		{
			if (activationMode == (int)eAAM_OnPress)
			{
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationBack);
				m_Back = 1;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationIdle);
				m_Back = 0;
			}

			m_movementDelta.y = -value; 
		});
	m_pInputComponent->BindAction("player", "moveback", eAID_KeyboardMouse, eKI_S);

	m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value) 
		{
			m_movementDelta.x = -value; 
			if (activationMode == (int)eAAM_OnPress)
			{
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationLeft);
				m_Left = 1;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationIdle);
				m_Left = 0;
			}
		});
	m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse, eKI_A);

	m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value) 
		{
			m_movementDelta.x = value; 
			if (activationMode == (int)eAAM_OnPress)
			{
				m_pAdvancedAnimationComponent->QueueFragment(Schematyc::CSharedString(m_AnimationRight.c_str()));
				m_Right = 1;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationIdle);
				m_Right = 0;
			}
		});
	m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, eKI_D);

	m_pInputComponent->RegisterAction("Player", "yaw", [this](int activationMode, float value) {m_MouseDeltaRotation.y = -value;});
	m_pInputComponent->BindAction("Player", "yaw", eAID_KeyboardMouse, eKI_MouseY);

	m_pInputComponent->RegisterAction("Player", "pitch", [this](int activationMode, float value) {m_MouseDeltaRotation.x = -value;});
	m_pInputComponent->BindAction("Player", "pitch", eAID_KeyboardMouse, eKI_MouseX);

	m_pInputComponent->RegisterAction("player", "sprint", [this](int activationMode, float value) 
		{
			if (activationMode == (int)eAAM_OnPress)
			{
				m_currentPlayerState = EPlayerState::Sprinting;
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationRun);
				m_Run = 1;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_currentPlayerState = EPlayerState::Walking;
				m_Run = 0;
			}
		});
	m_pInputComponent->BindAction("player", "sprint", eAID_KeyboardMouse, eKI_LShift);

	m_pInputComponent->RegisterAction("player", "jump", [this](int activationMode, float value) 
		{
			if (m_pCharacterControllerComponent->IsOnGround())
			{
				m_pCharacterControllerComponent->AddVelocity(Vec3(0, 0, m_JumpHeight));
			}
			if (activationMode == (int)eAAM_OnPress)
			{
				m_currentPlayerState = EPlayerState::Jump;
				m_pAdvancedAnimationComponent->QueueFragment(Schematyc::CSharedString(m_AnimationJump.c_str()));
			}
		});
	m_pInputComponent->BindAction("player", "crouch", eAID_KeyboardMouse, eKI_Space);

	m_pInputComponent->RegisterAction("player", "crouch", [this](int activationMode, float value)
		{
			if (activationMode == (int)eAAM_OnPress)
			{
				m_desiredPlayerStance = EPlayerStance::Crouching;
				m_pAdvancedAnimationComponent->QueueFragment(m_AnimationCrouch);

				m_Crouch = 1;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_desiredPlayerStance = EPlayerStance::Standing;
				m_Crouch = 0;
			}

			/*if (m_pCharacterControllerComponent->IsOnGround())
			{
				m_pCharacterControllerComponent->AddVelocity(Vec3(0, 0, m_JumpHeight));
			}
			if (activationMode == (int)eAAM_OnPress)
			{
				m_currentPlayerState = EPlayerState::Jump;
				m_pAdvancedAnimationComponent->QueueFragment("Jump");
			} */
		});
	m_pInputComponent->BindAction("player", "crouch", eAID_KeyboardMouse, eKI_C);

}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return 
		Cry::Entity::EEvent::GameplayStarted | 
		Cry::Entity::EEvent::Update | 
		Cry::Entity::EEvent::Reset | 
		Cry::Entity::EEvent::EditorPropertyChanged | 
		Cry::Entity::EEvent::PhysicalObjectBroken;
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& eventParam)
{
	switch (eventParam.event)
	{
	case Cry::Entity::EEvent::GameplayStarted:
	{
		Reset();
	}
	break;

	case Cry::Entity::EEvent::Update:
	{
		const float frametime = eventParam.fParam[0];
		TryUpdateStance();
		UpdateMovement();
		UpdateCamera(frametime);
		UpdateRotation();
	
	}
	break;

	case Cry::Entity::EEvent::PhysicalTypeChanged:
	{
		RecenterCollider();
	}
	break;

	case Cry::Entity::EEvent::EditorPropertyChanged:
	{
		Reset();
	}
	break;

		break;
	}
}

void CPlayerComponent::UpdateMovement()
{
	// Player Movement
	Vec3 velocity = Vec3(m_movementDelta.x, m_movementDelta.y, 0.0f);
	velocity.normalize();
	float playerMoveSpeed = m_currentPlayerState == EPlayerState::Sprinting ? m_RunSpeed : m_WalkSpeed;
	m_pCharacterControllerComponent->SetVelocity(m_pEntity->GetWorldRotation() * velocity * playerMoveSpeed);
}

void CPlayerComponent::UpdateRotation()
{
	m_CurrentYaw *= Quat::CreateRotationZ(m_MouseDeltaRotation.x * m_RotationSpeed);
	m_pEntity->SetRotation(m_CurrentYaw);
}

void CPlayerComponent::UpdateCamera(float frametime)
{
	m_CurrentPitch = crymath::clamp(m_CurrentPitch + m_MouseDeltaRotation.y * m_RotationSpeed, m_RotationLimitsMaxPitch, m_RotationLimitsMinPitch);

	Vec3 CurrentCameraOffset = m_pCameraComponent->GetTransformMatrix().GetTranslation();
	CurrentCameraOffset = Vec3::CreateLerp(CurrentCameraOffset,m_CameraEndOffset,10.0f*frametime);

	Matrix34 finalCamMatrix;
	finalCamMatrix.SetTranslation(m_CameraOffsetStanding);
	finalCamMatrix.SetRotation33(Matrix33::CreateRotationX(m_CurrentPitch));
	m_pCameraComponent->SetTransformMatrix(finalCamMatrix);
}

void CPlayerComponent::TryUpdateStance()
{
	if (m_desiredPlayerStance==m_currentPlayerStance)
		return;

	IPhysicalEntity* pPhysEnt = m_pEntity->GetPhysicalEntity();

	if (pPhysEnt == nullptr)
		return;

	const float radius = m_pCharacterControllerComponent->GetPhysicsParameters().m_radius * 0.5f;
	float height = 0.f;
	Vec3 camOffset = ZERO;

	switch (m_desiredPlayerStance)
	{

	/*case Cry::Entity::EEvent::PhysicalTypeChanged:
		RecenterCollider();
		break;

	case Cry::Entity::EEvent::Reset:
		Reset();
		break;*/

		case EPlayerStance::Crouching:
		{
			height = m_CapsuleHeightCrouching;
			camOffset = m_CameraOffsetCrouching;
		} break;

		case EPlayerStance::Standing:
		{
			height = m_CapsuleHeightStanding;
			camOffset = m_CameraOffsetStanding;

			primitives::capsule capsule;

			capsule.axis.Set(0, 0, 1);

			capsule.center = m_pEntity->GetWorldPos() + Vec3(0, 0, m_CapsuleGroundOffset + radius + height * 0.5f);
			capsule.r = radius;
			capsule.hh = height * 0.5f;

			if (IsCapsuleIntersectingGeometry(capsule))
			{
				return;
			}

		} break;

		

		pe_player_dimensions playerDimensions;
		pPhysEnt->GetParams(&playerDimensions);

		playerDimensions.heightCollider = m_CapsuleGroundOffset + radius + height * 0.5f;

		playerDimensions.sizeCollider = Vec3(radius, radius, height * 0.5f);

		m_CameraEndOffset = camOffset;

		m_currentPlayerStance = m_desiredPlayerStance;

		pPhysEnt->SetParams(&playerDimensions);
	}

}

bool CPlayerComponent::IsCapsuleIntersectingGeometry(const primitives::capsule& capsule) const
{
	IPhysicalEntity* pPhysEnt = m_pEntity->GetPhysicalEntity();
	if (pPhysEnt == nullptr)
	{
		return false;
	}
	//return false;

	IPhysicalWorld::SPWIParams pwiParams;
	pwiParams.itype = capsule.type;
	pwiParams.pprim = &capsule;

	pwiParams.pSkipEnts = &pPhysEnt;
	pwiParams.nSkipEnts = 1;

	intersection_params intersectionParams;
	intersectionParams.bSweepTest = false;
	pwiParams.pip = &intersectionParams;

	const int contactCount = static_cast<int>(gEnv->pPhysicalWorld->PrimitiveWorldIntersection(pwiParams));

	return contactCount > 0;
}

void CPlayerComponent::CheckAnimationState()
{
	// Check for specific animation states based on movement variables
	if (m_Run == 1)
	{
		CryLogAlways("Run");
		m_pAdvancedAnimationComponent->QueueFragment(m_AnimationRun);
	}
	else if (m_Crouch == 1)
	{
		if (m_Walk == 1 && m_Left == 1)
		{
			CryLogAlways("Crouch Walk Left");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationCrouchLeft);
		}
		else if (m_Walk == 1 && m_Right == 1)
		{
			CryLogAlways("Crouch Walk Right");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationCrouchRight);
		}
		else if (m_Back == 1)
		{
			CryLogAlways("Crouch Walk Back");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationCrouchBack);
		}
		else if (m_Walk == 1)
		{
			CryLogAlways("Crouch Walk");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationCrouchWalk);
		}
		else
		{
			CryLogAlways("Crouch");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationCrouch);
		}
	}
	else if (m_Walk == 1)
	{
		if (m_Left == 1)
		{
			CryLogAlways("Walk Left");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationWalkLeft);
		}
		else if (m_Right == 1)
		{
			CryLogAlways("Walk Right");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationWalkRight);
		}
		else if (m_Back == 1)
		{
			CryLogAlways("Walk Back");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationBack);
		}
		else
		{
			CryLogAlways("Walk");
			m_pAdvancedAnimationComponent->QueueFragment(m_AnimationWalk);
		}
	}
	else if (m_Back == 1)
	{
		CryLogAlways("Walk Back");
		m_pAdvancedAnimationComponent->QueueFragment(m_AnimationBack);
	}
	else
	{
		CryLogAlways("Idle");
		m_pAdvancedAnimationComponent->QueueFragment(m_AnimationIdle);
	}
}

/*
	---------------
	FLOWGRAPH NODES
	---------------
*/



/*
	Input Bind Node
*/

// Static member definitions
std::unordered_map<EKeyId, std::string> KeyMapper::s_keyIdToNameMap;
std::unordered_map<std::string, EKeyId> KeyMapper::s_nameToKeyIdMap;

void KeyMapper::InitializeKeyMappings()
{
	if (!s_keyIdToNameMap.empty())
		return; // Already initialized

	// Populate the mappings
	s_keyIdToNameMap = {
		// Keyboard keys
		{eKI_Escape, "Escape"},
		{eKI_1, "1"},
		{eKI_2, "2"},
		{eKI_3, "3"},
		{eKI_4, "4"},
		{eKI_5, "5"},
		{eKI_6, "6"},
		{eKI_7, "7"},
		{eKI_8, "8"},
		{eKI_9, "9"},
		{eKI_0, "0"},
		{eKI_Minus, "Minus"},
		{eKI_Equals, "Equals"},
		{eKI_Backspace, "Backspace"},
		{eKI_Tab, "Tab"},
		{eKI_Q, "Q"},
		{eKI_W, "W"},
		{eKI_E, "E"},
		{eKI_R, "R"},
		{eKI_T, "T"},
		{eKI_Y, "Y"},
		{eKI_U, "U"},
		{eKI_I, "I"},
		{eKI_O, "O"},
		{eKI_P, "P"},
		{eKI_LBracket, "LBracket"},
		{eKI_RBracket, "RBracket"},
		{eKI_Enter, "Enter"},
		{eKI_LCtrl, "LCtrl"},
		{eKI_A, "A"},
		{eKI_S, "S"},
		{eKI_D, "D"},
		{eKI_F, "F"},
		{eKI_G, "G"},
		{eKI_H, "H"},
		{eKI_J, "J"},
		{eKI_K, "K"},
		{eKI_L, "L"},
		{eKI_Semicolon, "Semicolon"},
		{eKI_Apostrophe, "Apostrophe"},
		{eKI_Tilde, "Tilde"},
		{eKI_LShift, "LShift"},
		{eKI_Backslash, "Backslash"},
		{eKI_Z, "Z"},
		{eKI_X, "X"},
		{eKI_C, "C"},
		{eKI_V, "V"},
		{eKI_B, "B"},
		{eKI_N, "N"},
		{eKI_M, "M"},
		{eKI_Comma, "Comma"},
		{eKI_Period, "Period"},
		{eKI_Slash, "Slash"},
		{eKI_RShift, "RShift"},
		{eKI_NP_Multiply, "NPMultiply"},
		{eKI_LAlt, "LAlt"},
		{eKI_Space, "Space"},
		{eKI_CapsLock, "CapsLock"},
		{eKI_F1, "F1"},
		{eKI_F2, "F2"},
		{eKI_F3, "F3"},
		{eKI_F4, "F4"},
		{eKI_F5, "F5"},
		{eKI_F6, "F6"},
		{eKI_F7, "F7"},
		{eKI_F8, "F8"},
		{eKI_F9, "F9"},
		{eKI_F10, "F10"},
		{eKI_NumLock, "NumLock"},
		{eKI_ScrollLock, "ScrollLock"},
		{eKI_NP_7, "NP7"},
		{eKI_NP_8, "NP8"},
		{eKI_NP_9, "NP9"},
		{eKI_NP_Substract, "NPSubstract"},
		{eKI_NP_4, "NP4"},
		{eKI_NP_5, "NP5"},
		{eKI_NP_6, "NP6"},
		{eKI_NP_Add, "NPAdd"},
		{eKI_NP_1, "NP1"},
		{eKI_NP_2, "NP2"},
		{eKI_NP_3, "NP3"},
		{eKI_NP_0, "NP0"},
		{eKI_F11, "F11"},
		{eKI_F12, "F12"},
		{eKI_F13, "F13"},
		{eKI_F14, "F14"},
		{eKI_F15, "F15"},
		{eKI_Colon, "Colon"},
		{eKI_Underline, "Underline"},
		{eKI_NP_Enter, "NPEnter"},
		{eKI_RCtrl, "RCtrl"},
		{eKI_NP_Period, "NPPeriod"},
		{eKI_NP_Divide, "NPDivide"},
		{eKI_Print, "Print"},
		{eKI_RAlt, "RAlt"},
		{eKI_Pause, "Pause"},
		{eKI_Home, "Home"},
		{eKI_Up, "Up"},
		{eKI_PgUp, "PgUp"},
		{eKI_Left, "Left"},
		{eKI_Right, "Right"},
		{eKI_End, "End"},
		{eKI_Down, "Down"},
		{eKI_PgDn, "PgDn"},
		{eKI_Insert, "Insert"},
		{eKI_Delete, "Delete"},
		{eKI_LWin, "LWin"},
		{eKI_RWin, "RWin"},
		{eKI_Apps, "Apps"},
		{eKI_OEM_102, "OEM102"},

		// Mouse buttons
		{eKI_Mouse1, "Mouse1"},
		{eKI_Mouse2, "Mouse2"},
		{eKI_Mouse3, "Mouse3"},
		{eKI_Mouse4, "Mouse4"},
		{eKI_Mouse5, "Mouse5"},
		{eKI_Mouse6, "Mouse6"},
		{eKI_Mouse7, "Mouse7"},
		{eKI_Mouse8, "Mouse8"},
		{eKI_MouseWheelUp, "MouseWheelUp"},
		{eKI_MouseWheelDown, "MouseWheelDown"},
		{eKI_MouseX, "MouseX"},
		{eKI_MouseY, "MouseY"},
		{eKI_MouseZ, "MouseZ"},
		{eKI_MouseXAbsolute, "MouseXAbsolute"},
		{eKI_MouseYAbsolute, "MouseYAbsolute"},

		// Xbox controller
		{eKI_XI_DPadUp, "XI_DPadUp"},
		{eKI_XI_DPadDown, "XI_DPadDown"},
		{eKI_XI_DPadLeft, "XI_DPadLeft"},
		{eKI_XI_DPadRight, "XI_DPadRight"},
		{eKI_XI_Start, "XI_Start"},
		{eKI_XI_Back, "XI_Back"},
		{eKI_XI_ThumbL, "XI_ThumbL"},
		{eKI_XI_ThumbR, "XI_ThumbR"},
		{eKI_XI_ShoulderL, "XI_ShoulderL"},
		{eKI_XI_ShoulderR, "XI_ShoulderR"},
		{eKI_XI_A, "XI_A"},
		{eKI_XI_B, "XI_B"},
		{eKI_XI_X, "XI_X"},
		{eKI_XI_Y, "XI_Y"},
		{eKI_XI_TriggerL, "XI_TriggerL"},
		{eKI_XI_TriggerR, "XI_TriggerR"},
		// Xbox controller
		{ eKI_XI_DPadUp, "XI_DPadUp" },
		{ eKI_XI_DPadDown, "XI_DPadDown" },
		{ eKI_XI_DPadLeft, "XI_DPadLeft" },
		{ eKI_XI_DPadRight, "XI_DPadRight" },
		{ eKI_XI_Start, "XI_Start" },
		{ eKI_XI_Back, "XI_Back" },
		{ eKI_XI_ThumbL, "XI_ThumbL" },
		{ eKI_XI_ThumbR, "XI_ThumbR" },
		{ eKI_XI_ShoulderL, "XI_ShoulderL" },
		{ eKI_XI_ShoulderR, "XI_ShoulderR" },
		{ eKI_XI_A, "XI_A" },
		{ eKI_XI_B, "XI_B" },
		{ eKI_XI_X, "XI_X" },
		{ eKI_XI_Y, "XI_Y" },
		{ eKI_XI_TriggerL, "XI_TriggerL" },
		{ eKI_XI_TriggerR, "XI_TriggerR" },
		{ eKI_XI_ThumbLX, "XI_ThumbLX" },
		{ eKI_XI_ThumbLY, "XI_ThumbLY" },
		{ eKI_XI_ThumbLUp, "XI_ThumbLUp" },
		{ eKI_XI_ThumbLDown, "XI_ThumbLDown" },
		{ eKI_XI_ThumbLLeft, "XI_ThumbLLeft" },
		{ eKI_XI_ThumbLRight, "XI_ThumbLRight" },
		{ eKI_XI_ThumbRX, "XI_ThumbRX" },
		{ eKI_XI_ThumbRY, "XI_ThumbRY" },
		{ eKI_XI_ThumbRUp, "XI_ThumbRUp" },
		{ eKI_XI_ThumbRDown, "XI_ThumbRDown" },
		{ eKI_XI_ThumbRLeft, "XI_ThumbRLeft" },
		{ eKI_XI_ThumbRRight, "XI_ThumbRRight" },

		// Orbis controller
		{ eKI_Orbis_Options, "Orbis_Options" },
		{ eKI_Orbis_L3, "Orbis_L3" },
		{ eKI_Orbis_R3, "Orbis_R3" },
		{ eKI_Orbis_Up, "Orbis_Up" },
		{ eKI_Orbis_Right, "Orbis_Right" },
		{ eKI_Orbis_Down, "Orbis_Down" },
		{ eKI_Orbis_Left, "Orbis_Left" },
		{ eKI_Orbis_L2, "Orbis_L2" },  // L2 as button, for trigger use LeftTrigger.
		{ eKI_Orbis_R2, "Orbis_R2" },  // R2 as button, for trigger use RightTrigger.
		{ eKI_Orbis_L1, "Orbis_L1" },
		{ eKI_Orbis_R1, "Orbis_R1" },
		{ eKI_Orbis_Triangle, "Orbis_Triangle" },
		{ eKI_Orbis_Circle, "Orbis_Circle" },
		{ eKI_Orbis_Cross, "Orbis_Cross" },
		{ eKI_Orbis_Square, "Orbis_Square" },
		{ eKI_Orbis_StickLX, "Orbis_StickLX" },
		{ eKI_Orbis_StickLY, "Orbis_StickLY" },
		{ eKI_Orbis_StickRX, "Orbis_StickRX" },
		{ eKI_Orbis_StickRY, "Orbis_StickRY" },
		{ eKI_Orbis_RotX, "Orbis_RotX" },
		{ eKI_Orbis_RotY, "Orbis_RotY" },
		{ eKI_Orbis_RotZ, "Orbis_RotZ" },
		{ eKI_Orbis_RotX_KeyL, "Orbis_RotX_KeyL" },
		{ eKI_Orbis_RotX_KeyR, "Orbis_RotX_KeyR" },
		{ eKI_Orbis_RotZ_KeyD, "Orbis_RotZ_KeyD" },
		{ eKI_Orbis_RotZ_KeyU, "Orbis_RotZ_KeyU" },
		{ eKI_Orbis_LeftTrigger, "Orbis_LeftTrigger" },  // L2 as trigger, for button use L2.
		{ eKI_Orbis_RightTrigger, "Orbis_RightTrigger" }, // R2 as trigger, for button use R2.
		{ eKI_Orbis_Touch, "Orbis_Touch" },

		// Oculus
		{ eKI_Motion_OculusTouch_A, "OculusTouch_A" },
		{ eKI_Motion_OculusTouch_B, "OculusTouch_B" },
		{ eKI_Motion_OculusTouch_X, "OculusTouch_X" },
		{ eKI_Motion_OculusTouch_Y, "OculusTouch_Y" },
		{ eKI_Motion_OculusTouch_L3, "OculusTouch_L3" },  // Left thumb button (stick).
		{ eKI_Motion_OculusTouch_R3, "OculusTouch_R3" },  // Right thumb button (stick).
		{ eKI_Motion_OculusTouch_TriggerBtnL, "OculusTouch_TriggerBtnL" },  // Left trigger button.
		{ eKI_Motion_OculusTouch_TriggerBtnR, "OculusTouch_TriggerBtnR" },  // Right trigger button.
		{ eKI_Motion_OculusTouch_L1, "OculusTouch_L1" },  // Left index trigger.
		{ eKI_Motion_OculusTouch_R1, "OculusTouch_R1" },  // Right index trigger.
		{ eKI_Motion_OculusTouch_L2, "OculusTouch_L2" },  // Left hand trigger.
		{ eKI_Motion_OculusTouch_R2, "OculusTouch_R2" },  // Right hand trigger.
		{ eKI_Motion_OculusTouch_StickL_Y, "OculusTouch_StickL_Y" },  // Left stick vertical motion.
		{ eKI_Motion_OculusTouch_StickR_Y, "OculusTouch_StickR_Y" },  // Right stick vertical motion.
		{ eKI_Motion_OculusTouch_StickL_X, "OculusTouch_StickL_X" },  // Left stick horizontal motion.
		{ eKI_Motion_OculusTouch_StickR_X, "OculusTouch_StickR_X" },  // Right stick horizontal motion.
		{ eKI_Motion_OculusTouch_Gesture_ThumbUpL, "OculusTouch_Gesture_ThumbUpL" },  // Gesture left thumb up.
		{ eKI_Motion_OculusTouch_Gesture_ThumbUpR, "OculusTouch_Gesture_ThumbUpR" },  // Gesture right thumb up.
		{ eKI_Motion_OculusTouch_Gesture_IndexPointingL, "OculusTouch_Gesture_IndexPointingL" },  // Gesture left index pointing.
		{ eKI_Motion_OculusTouch_Gesture_IndexPointingR, "OculusTouch_Gesture_IndexPointingR" },  // Gesture right index pointing.

		{ eKI_Motion_OculusTouch_NUM_SYMBOLS, "OculusTouch_NUM_SYMBOLS" },
		{ eKI_Motion_OculusTouch_LastButtonIndex, "OculusTouch_LastButtonIndex" },
		{ eKI_Motion_OculusTouch_FirstGestureIndex, "OculusTouch_FirstGestureIndex" },
		{ eKI_Motion_OculusTouch_LastGestureIndex, "OculusTouch_LastGestureIndex" },
		{ eKI_Motion_OculusTouch_FirstTriggerIndex, "OculusTouch_FirstTriggerIndex" },
		{ eKI_Motion_OculusTouch_LastTriggerIndex, "OculusTouch_LastTriggerIndex" },

		// Eye Tracker
		{ eKI_EyeTracker_X, "EyeTracker_X" },
		{ eKI_EyeTracker_Y, "EyeTracker_Y" },

		// OpenVR
		{ eKI_Motion_OpenVR_System, "OpenVR_System" },
		{ eKI_Motion_OpenVR_ApplicationMenu, "OpenVR_ApplicationMenu" },
		{ eKI_Motion_OpenVR_Grip, "OpenVR_Grip" },
		{ eKI_Motion_OpenVR_TouchPad_X, "OpenVR_TouchPad_X" },
		{ eKI_Motion_OpenVR_TouchPad_Y, "OpenVR_TouchPad_Y" },
		{ eKI_Motion_OpenVR_Trigger, "OpenVR_Trigger" },
		{ eKI_Motion_OpenVR_TriggerBtn, "OpenVR_TriggerBtn" },
		{ eKI_Motion_OpenVR_TouchPadBtn, "OpenVR_TouchPadBtn" },

		{ eKI_Motion_OpenVR_NUM_SYMBOLS, "OpenVR_NUM_SYMBOLS" }

		// Add more mappings as needed...
		// ^^ Don't trust this comment, it's all here :-) ^^
	};

	// Reverse the mapping for user-friendly name to EKeyId
	for (const auto& pair : s_keyIdToNameMap)
	{
		s_nameToKeyIdMap[pair.second] = pair.first;
	}
}

std::string KeyMapper::KeyIdToUserFriendlyName(EKeyId keyId)
{
	InitializeKeyMappings();

	// Check if the keyId exists in the map
	auto it = s_keyIdToNameMap.find(keyId);
	if (it != s_keyIdToNameMap.end())
	{
		return it->second; // Return the user-friendly name
	}

	// Fallback: Generate a user-friendly name if not found
	// Assuming EKeyId values are enums prefixed with eKI_
	std::string fallbackName = "Unknown";
	if (keyId >= eKI_A && keyId <= eKI_Z) // Handle alphabetic keys
	{
		fallbackName = std::string(1, 'A' + (keyId - eKI_A));
	}
	else if (keyId >= eKI_0 && keyId <= eKI_9) // Handle numeric keys
	{
		fallbackName = std::to_string(keyId - eKI_0);
	}
	else
	{
		fallbackName = "eKI_" + std::to_string(static_cast<int>(keyId));
	}

	return fallbackName;
}

EKeyId KeyMapper::UserFriendlyNameToKeyId(const std::string& keyName)
{
	InitializeKeyMappings();

	auto it = s_nameToKeyIdMap.find(keyName);
	if (it != s_nameToKeyIdMap.end())
	{
		return it->second;
	}

	return eKI_Unknown; // Default for unmapped keys
}


CFlowNode_ChangeInputBinding::CFlowNode_ChangeInputBinding(SActivationInfo* pActInfo, CPlayerComponent* pPlayerComponent)
	: m_pPlayerComponent(pPlayerComponent)
{
	if (!m_pPlayerComponent && pActInfo && pActInfo->pEntity)
	{
		m_pPlayerComponent = pActInfo->pEntity->GetComponent<CPlayerComponent>();
		CryLogAlways("[CFlowNode_ChangeInputBinding] Retrieved Player Component dynamically.");
	}
}

void CFlowNode_ChangeInputBinding::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->AddObject(this, sizeof(*this));
}

void CFlowNode_ChangeInputBinding::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPorts[] = {
		InputPortConfig<string>("ActionName", _HELP("Name of the action to rebind")),
		InputPortConfig<string>("NewKey", _HELP("New key to bind to the action")),
		InputPortConfig_Void("Trigger", _HELP("Trigger to apply the new binding")),
		{ 0 }
	};

	static const SOutputPortConfig outputPorts[] = {
		OutputPortConfig_Void("OnSuccess", _HELP("Triggered when the binding is successfully changed")),
		OutputPortConfig_Void("OnFailure", _HELP("Triggered when the binding fails")),
		{ 0 }
	};

	config.sDescription = _HELP("FlowGraph node to change key input bindings dynamically");
	config.pInputPorts = inputPorts;
	config.pOutputPorts = outputPorts;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_ChangeInputBinding::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, 2)) // Trigger input
	{
		const string& actionName = GetPortString(pActInfo, 0);
		const string& newKey = GetPortString(pActInfo, 1);

	if (RebindAction(actionName, newKey))
		{
			ActivateOutput(pActInfo, 0, true); // OnSuccess
		}
		else
		{
			ActivateOutput(pActInfo, 1, true); // OnFailure
		}
	}
	
}

bool CFlowNode_ChangeInputBinding::RebindAction(const string& actionName, const string& newKey)
{
	if (!m_pPlayerComponent)
	{
		CryLogAlways("[RebindAction] Player component is null. Attempting to retrieve dynamically.");
		if (gEnv && gEnv->pEntitySystem)
		{
			IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName("Player");
			if (pEntity)
			{
				m_pPlayerComponent = pEntity->GetComponent<CPlayerComponent>();
				if (m_pPlayerComponent)
				{
					CryLogAlways("[RebindAction] Successfully retrieved Player Component.");
				}
				else
				{
					CryLogAlways("[RebindAction] Failed to retrieve Player Component.");
					return false;
				}
			}
			else
			{
				CryLogAlways("[RebindAction] Failed: Could not find entity named 'Player'.");
				return false;
			}
		}
	}

	if (!m_pPlayerComponent)
	{
		CryLogAlways("[RebindAction] Failed: Player component is still null.");
		return false;
	}

	if (actionName.empty())
	{
		CryLogAlways("[RebindAction] Failed: Action name is empty.");
		return false;
	}

	if (newKey.empty())
	{
		CryLogAlways("[RebindAction] Failed: New key is empty.");
		return false;
	}

	// Retrieve the input symbol for the given key name
	const SInputSymbol* pInputSymbol = gEnv->pInput->GetSymbolByName(newKey.c_str());
	if (!pInputSymbol)
	{
		CryLogAlways("[RebindAction] Failed: Key '%s' not found in input system.", newKey.c_str());
		return false;
	}

	// Extract the EKeyId from the input symbol
	EKeyId keyId = pInputSymbol->keyId;

	// Convert the EKeyId to a user-friendly name
	std::string keyName = KeyMapper::KeyIdToUserFriendlyName(keyId);

	CryLogAlways("[RebindAction] Key '%s' resolved to EKeyId '%s'.", newKey.c_str(), keyName.c_str());

	// Access the input component from the player component
	if (m_pPlayerComponent->m_pInputComponent)
	{
		// Store the existing callback for the action
		auto defaultCallback = [this, actionName](int activationMode, float value)
			{
				if (actionName == "moveforward")
				{
					m_pPlayerComponent->m_movementDelta.y = value;
					if (activationMode == (int)eAAM_OnPress)
					{
						m_pPlayerComponent->m_Walk = 1;
					}
					else if (activationMode == eAAM_OnRelease)
					{
						m_pPlayerComponent->m_Walk = 0;
					}
				}
				else if (actionName == "moveback")
				{
					m_pPlayerComponent->m_movementDelta.y = -value;
				}
				else if (actionName == "moveleft")
				{
					m_pPlayerComponent->m_movementDelta.x = -value;
					if (activationMode == (int)eAAM_OnPress)
					{
						m_pPlayerComponent->m_Left = 1;
					}
					else if (activationMode == eAAM_OnRelease)
					{
						m_pPlayerComponent->m_Left = 0;
					}
				}
				else if (actionName == "moveright")
				{
					m_pPlayerComponent->m_movementDelta.x = value;
					if (activationMode == (int)eAAM_OnPress)
					{
						m_pPlayerComponent->m_Right = 1;
					}
					else if (activationMode == eAAM_OnRelease)
					{
						m_pPlayerComponent->m_Right = 0;
					}
				}
				else if (actionName == "yaw")
				{
					m_pPlayerComponent->m_MouseDeltaRotation.y = -value;
				}
				else if (actionName == "pitch")
				{
					m_pPlayerComponent->m_MouseDeltaRotation.x = -value;
				}
				else if (actionName == "sprint")
				{
					if (activationMode == (int)eAAM_OnPress)
					{
						m_pPlayerComponent->m_currentPlayerState = CPlayerComponent::EPlayerState::Sprinting;
						m_pPlayerComponent->m_Run = 1;
					}
					else if (activationMode == eAAM_OnRelease)
					{
						m_pPlayerComponent->m_currentPlayerState = CPlayerComponent::EPlayerState::Walking;
						m_pPlayerComponent->m_Run = 0;
					}
				}
				else if (actionName == "jump")
				{
					if (m_pPlayerComponent->m_pCharacterControllerComponent->IsOnGround())
					{
						m_pPlayerComponent->m_pCharacterControllerComponent->AddVelocity(Vec3(0, 0, m_pPlayerComponent->m_JumpHeight));
					}
				}
				else if (actionName == "crouch")
				{
					if (activationMode == (int)eAAM_OnPress)
					{
						m_pPlayerComponent->m_desiredPlayerStance = CPlayerComponent::EPlayerStance::Crouching;
						m_pPlayerComponent->m_Crouch = 1;
					}
					else if (activationMode == (int)eAAM_OnRelease)
					{
						m_pPlayerComponent->m_desiredPlayerStance = CPlayerComponent::EPlayerStance::Standing;
						m_pPlayerComponent->m_Crouch = 0;
					}
				}
			};


		// Create a combined callback
		auto combinedCallback = [defaultCallback, actionName](int activationMode, float value)
			{
				// Call the default callback
				defaultCallback(activationMode, value);

				// Call the flowgraph node's behavior (if any)
				CryLogAlways("[RebindAction] Flowgraph node triggered for action '%s'.", actionName.c_str());
			};

		// Register the combined callback
		m_pPlayerComponent->m_pInputComponent->RegisterAction("player", actionName.c_str(), combinedCallback);

		// Bind the action to the new key
		m_pPlayerComponent->m_pInputComponent->BindAction(
			"player",
			actionName.c_str(),
			eAID_KeyboardMouse,
			keyId,
			true,  // Bind on press
			true,  // Bind on release
			true   // Bind on hold
		);

		CryLogAlways("[RebindAction] Successfully bound action '%s' to key '%s'.", actionName.c_str(), newKey.c_str());
		return true;
	}

	CryLogAlways("[RebindAction] Failed: Input component is null.");
	return false;
}




REGISTER_FLOW_NODE("Player Component:Change Input Bind", CFlowNode_ChangeInputBinding);



/*
	Trigger Custom Animation Node
*/


CFlowNode_TriggerCustomAnimation::CFlowNode_TriggerCustomAnimation(SActivationInfo* pActInfo)
	: m_pPlayerComponent(nullptr)
{
	if (pActInfo)
	{
		if (pActInfo->pEntity)
		{
			m_pPlayerComponent = pActInfo->pEntity->GetComponent<CPlayerComponent>();
			if (m_pPlayerComponent)
			{
				CryLogAlways("[CFlowNode_TriggerCustomAnimation] Successfully retrieved Player Component.");
			}
			else
			{
				CryLogAlways("[CFlowNode_TriggerCustomAnimation] Failed to retrieve Player Component from entity.");
			}
		}
		else
		{
			CryLogAlways("[CFlowNode_TriggerCustomAnimation] pEntity is null in Activation info.");
		}
	}
	else
	{
		CryLogAlways("[CFlowNode_TriggerCustomAnimation] Activation info is null.");
	}
}

void CFlowNode_TriggerCustomAnimation::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPorts[] = {
		InputPortConfig<string>("AnimationName", _HELP("Name of the animation fragment to trigger")),
		InputPortConfig<bool>("MotionDriven", false, _HELP("Set to true if the animation is motion-driven")),
		InputPortConfig_Void("Trigger", _HELP("Trigger to play the animation")),
		{ 0 }
	};

	static const SOutputPortConfig outputPorts[] = {
		OutputPortConfig_Void("OnSuccess", _HELP("Triggered when the animation is successfully played")),
		OutputPortConfig_Void("OnFailure", _HELP("Triggered if the animation fails to play")),
		{ 0 }
	};

	config.sDescription = _HELP("FlowGraph node to trigger a custom animation");
	config.pInputPorts = inputPorts;
	config.pOutputPorts = outputPorts;
	config.SetCategory(EFLN_APPROVED);
}


void CFlowNode_TriggerCustomAnimation::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, 2)) // Trigger input
	{
		CryLogAlways("[CFlowNode_TriggerCustomAnimation] Trigger input activated.");

		if (!pActInfo)
		{
			CryLogAlways("[CFlowNode_TriggerCustomAnimation] Activation info is null.");
			ActivateOutput(pActInfo, 1, true); // OnFailure
			return;
		}

		if (!pActInfo->pEntity)
		{
			CryLogAlways("[CFlowNode_TriggerCustomAnimation] pEntity is null in Activation info. Attempting to retrieve dynamically.");

			// Attempt to retrieve the entity dynamically by name
			IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName("Player");
			if (pEntity)
			{
				CryLogAlways("[CFlowNode_TriggerCustomAnimation] Successfully retrieved entity dynamically.");
				pActInfo->pEntity = pEntity; // Update pEntity in Activation info
			}
			else
			{
				CryLogAlways("[CFlowNode_TriggerCustomAnimation] Failed to retrieve entity dynamically.");
				ActivateOutput(pActInfo, 1, true); // OnFailure
				return;
			}
		}

		if (!m_pPlayerComponent)
		{
			CryLogAlways("[CFlowNode_TriggerCustomAnimation] Player component is null. Attempting to retrieve dynamically.");
			m_pPlayerComponent = pActInfo->pEntity->GetComponent<CPlayerComponent>();
			if (m_pPlayerComponent)
			{
				CryLogAlways("[CFlowNode_TriggerCustomAnimation] Successfully retrieved Player Component dynamically.");
			}
			else
			{
				CryLogAlways("[CFlowNode_TriggerCustomAnimation] Failed to retrieve Player Component dynamically.");
				ActivateOutput(pActInfo, 1, true); // OnFailure
				return;
			}
		}

		if (!m_pPlayerComponent->m_pAdvancedAnimationComponent)
		{
			CryLogAlways("[CFlowNode_TriggerCustomAnimation] AdvancedAnimationComponent is null.");
			ActivateOutput(pActInfo, 1, true); // OnFailure
			return;
		}

		const string& animationName = GetPortString(pActInfo, 0);
		const bool motionDriven = GetPortBool(pActInfo, 1);

		CryLogAlways("[CFlowNode_TriggerCustomAnimation] Animation name: '%s', Motion-driven: %s",
			animationName.c_str(), motionDriven ? "true" : "false");

		if (animationName.empty())
		{
			CryLogAlways("[CFlowNode_TriggerCustomAnimation] Animation name is empty.");
			ActivateOutput(pActInfo, 1, true); // OnFailure
			return;
		}

		m_pPlayerComponent->m_pAdvancedAnimationComponent->SetAnimationDrivenMotion(motionDriven);
		CryLogAlways("[CFlowNode_TriggerCustomAnimation] Motion-driven flag set to: %s", motionDriven ? "true" : "false");

		m_pPlayerComponent->m_pAdvancedAnimationComponent->QueueFragment(Schematyc::CSharedString(animationName.c_str()));
		CryLogAlways("[CFlowNode_TriggerCustomAnimation] Queued animation fragment: '%s'", animationName.c_str());

		ActivateOutput(pActInfo, 0, true); // OnSuccess
		CryLogAlways("[CFlowNode_TriggerCustomAnimation] Animation triggered successfully.");
	}
}



void CFlowNode_TriggerCustomAnimation::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->AddObject(this, sizeof(*this));
}

// Register the FlowGraph node
REGISTER_FLOW_NODE("Player Component:Play Custom Animation", CFlowNode_TriggerCustomAnimation);

