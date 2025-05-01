#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Utils/EnumFlags.h>
#include <CryMath/Cry_Math.h>
#include <CryString/CryString.h>
#include <CrySchematyc/CoreAPI.h>
#include "StdAfx.h"
#include "GamePlugin.h"



class CProximityTriggerComponent final : public IEntityComponent
{
public:
    virtual ~CProximityTriggerComponent() = default;

    static void ReflectType(Schematyc::CTypeDesc<CProximityTriggerComponent>& desc)
    {
        desc.SetGUID("{D4A1B5C3-4F6E-4A8B-9F3C-2E8D5A1B5C3E}"_cry_guid);
        desc.SetEditorCategory("Triggers");
        desc.SetLabel("Proximity Trigger");
        desc.SetDescription("A component that triggers events when entities enter or leave a proximity area.");
        desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });
        desc.AddMember(&CProximityTriggerComponent::m_targetEntityName, 'name', "TargetEntityName", "Target Entity Name", "The name of the entity that can trigger this proximity area.", "");
        desc.AddMember(&CProximityTriggerComponent::m_triggerSize, 'size', "TriggerSize", "Trigger Size", "The size of the trigger box (width, height, depth).", Vec3(2.0f, 2.0f, 2.0f));

        CryLogAlways("CProximityTriggerComponent::ReflectType - End");
    }

    virtual void Initialize() override;
    virtual Cry::Entity::EventFlags GetEventMask() const override;
    virtual void ProcessEvent(const SEntityEvent& event) override;
    bool m_drawDebug;

private:
    Schematyc::CSharedString m_targetEntityName; // Name of the entity that can trigger the proximity logic
    Vec3 m_triggerSize = Vec3(2.0f, 2.0f, 2.0f); // Default size of the trigger box
    void DrawDebug() const;
    EntityId m_triggerEntityId = INVALID_ENTITYID; // ID of the trigger entity
};
