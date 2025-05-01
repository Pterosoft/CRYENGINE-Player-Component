#include "StdAfx.h"
#include "ProximityTrigger.h"
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryCore/StaticInstanceList.h>
#include <CryAction.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryRenderer/IRenderAuxGeom.h>

static void RegisterProximityTriggerComponent(Schematyc::IEnvRegistrar& registrar)
{
    Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
    {
        Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CProximityTriggerComponent));
    }
}

CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterProximityTriggerComponent);


void CProximityTriggerComponent::Initialize()
{
    CryLogAlways("CProximityTriggerComponent::Initialize - Start");

    m_drawDebug = true; // Enable debug drawing by default

    IEntity* pEntity = GetEntity();
    if (!pEntity)
    {
        CryLogAlways("CProximityTriggerComponent::Initialize - Entity is null!");
        return;
    }

    CryLogAlways("CProximityTriggerComponent::Initialize - Entity is valid");

    IEntityTriggerComponent* pTriggerComponent = pEntity->CreateComponent<IEntityTriggerComponent>();
    if (!pTriggerComponent)
    {
        CryLogAlways("CProximityTriggerComponent::Initialize - Failed to create IEntityTriggerComponent!");
        return;
    }

    CryLogAlways("CProximityTriggerComponent::Initialize - IEntityTriggerComponent created");

    const AABB triggerBounds = AABB(m_triggerSize * -0.5f, m_triggerSize * 0.5f);
    pTriggerComponent->SetTriggerBounds(triggerBounds);

    pTriggerComponent->InvalidateTrigger();

    // Log the trigger bounds
    CryLogAlways("CProximityTriggerComponent::Initialize - Trigger Bounds: Min(%f, %f, %f), Max(%f, %f, %f)",
        triggerBounds.min.x, triggerBounds.min.y, triggerBounds.min.z,
        triggerBounds.max.x, triggerBounds.max.y, triggerBounds.max.z);

    CryLogAlways("CProximityTriggerComponent::Initialize - Trigger bounds set");
}


void CProximityTriggerComponent::DrawDebug() const
{
    IEntity* pEntity = GetEntity();
    if (!pEntity)
    {
        CryLogAlways("CProximityTriggerComponent::DrawTriggerBounds - Entity is null!");
        return;
    }

    // Check if the entity is selected or highlighted in the editor
    bool isSelected = false, isHighlighted = false;
    pEntity->GetEditorObjectInfo(isSelected, isHighlighted);

    // Render the bounds only if the entity is selected or highlighted
    if (!isSelected && !isHighlighted)
        return;

    IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
    if (!pAuxGeom)
    {
        CryLogAlways("CProximityTriggerComponent::DrawTriggerBounds - IRenderAuxGeom is null!");
        return;
    }

    // Get the world position of the entity
    const Vec3 entityPos = pEntity->GetWorldPos();

    // Define the trigger bounds in world space
    const AABB triggerBounds(entityPos - m_triggerSize * 0.5f, entityPos + m_triggerSize * 0.5f);

    // Draw the bounding box with a translucent green color
    pAuxGeom->DrawAABB(triggerBounds, false, ColorB(0, 255, 0, 128), eBBD_Faceted);
}



Cry::Entity::EventFlags CProximityTriggerComponent::GetEventMask() const
{
    // Listen for area-related events: entity entering, leaving, update, and game start
    return ENTITY_EVENT_ENTERAREA | ENTITY_EVENT_LEAVEAREA | ENTITY_EVENT_UPDATE | ENTITY_EVENT_START_GAME;
}


void CProximityTriggerComponent::ProcessEvent(const SEntityEvent& event)
{

    //CryLogAlways("CProximityTriggerComponent::ProcessEvent - Event ID: %d", event.event);

    if (event.event == ENTITY_EVENT_UPDATE)
    {
        DrawDebug();
    }
    else if (event.event == ENTITY_EVENT_START_GAME)
    {
        CryLogAlways("CProximityTriggerComponent::ProcessEvent - Game started, disabling debug rendering");
        m_drawDebug = false; // Disable debug rendering when the game starts
    }
    else if (event.event == ENTITY_EVENT_ENTERAREA)
    {
        CryLogAlways("CProximityTriggerComponent::ProcessEvent - ENTITY_EVENT_ENTERAREA triggered");

        static bool isProcessingEnterArea = false;
        if (isProcessingEnterArea)
        {
            CryLogAlways("CProximityTriggerComponent::ProcessEvent - Recursive ENTITY_EVENT_ENTERAREA detected, skipping");
            return;
        }

        isProcessingEnterArea = true; // Set the guard

        if (!event.nParam[0])
        {
            CryLogAlways("CProximityTriggerComponent::ProcessEvent - Invalid nParam[0]");
            isProcessingEnterArea = false; // Reset the guard
            return;
        }

        const EntityId enteredEntityId = static_cast<EntityId>(event.nParam[0]);
        CryLogAlways("CProximityTriggerComponent::ProcessEvent - Entered EntityId: %d", enteredEntityId);

        IEntity* pEnteredEntity = gEnv->pEntitySystem->GetEntity(enteredEntityId);
        if (!pEnteredEntity)
        {
            CryLogAlways("CProximityTriggerComponent::ProcessEvent - Entered entity is null!");
            isProcessingEnterArea = false; // Reset the guard
            return;
        }

        CryLogAlways("CProximityTriggerComponent::ProcessEvent - Entered entity name: %s", pEnteredEntity->GetName());

        if (std::string(pEnteredEntity->GetName()) == m_targetEntityName.c_str())
        {
            CryLogAlways("Entity '%s' entered the proximity trigger.", m_targetEntityName.c_str());
        }

        isProcessingEnterArea = false; // Reset the guard
    }

    else if (event.event == ENTITY_EVENT_LEAVEAREA)
    {
        CryLogAlways("CProximityTriggerComponent::ProcessEvent - ENTITY_EVENT_LEAVEAREA triggered");

        static bool isProcessingLeaveArea = false;
        if (isProcessingLeaveArea)
        {
            CryLogAlways("CProximityTriggerComponent::ProcessEvent - Recursive ENTITY_EVENT_LEAVEAREA detected, skipping");
            return;
        }

        isProcessingLeaveArea = true; // Set the guard

        const EntityId leftEntityId = static_cast<EntityId>(event.nParam[0]);
        IEntity* pLeftEntity = gEnv->pEntitySystem->GetEntity(leftEntityId);

        if (pLeftEntity && std::string(pLeftEntity->GetName()) == m_targetEntityName.c_str())
        {
            CryLogAlways("Entity '%s' left the proximity trigger.", m_targetEntityName.c_str());
        }

        isProcessingLeaveArea = false; // Reset the guard
    }
}





/*
    ---------------
    Flowgraph Nodes
    ---------------
*/



class CFlowProximityTriggerNode : public CFlowBaseNode<eNCT_Singleton>, public IEntityEventListener
{
public:
    CFlowProximityTriggerNode(SActivationInfo* pActInfo) {}

    virtual ~CFlowProximityTriggerNode() {}
    EntityId m_triggerEntityId = INVALID_ENTITYID; // ID of the trigger entity

    // Removed the Clone function as it cannot override a final function

    virtual void GetConfiguration(SFlowNodeConfig& config) override
    {
        static const SInputPortConfig inputPorts[] = {
            InputPortConfig<EntityId>("TriggerEntity", _HELP("Entity to use as the trigger (Assign Selected Entity)")),
            { 0 }
        };

        static const SOutputPortConfig outputPorts[] = {
            OutputPortConfig<EntityId>("EntityEnter", _HELP("Triggered when an entity enters the proximity trigger")),
            OutputPortConfig<EntityId>("EntityLeave", _HELP("Triggered when an entity leaves the proximity trigger")),
            { 0 }
        };

        config.sDescription = _HELP("Proximity Trigger Node");
        config.pInputPorts = inputPorts;
        config.pOutputPorts = outputPorts;
        config.SetCategory(EFLN_APPROVED);
    }

    void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override
    {
        if (event == eFE_Initialize)
        {
            CryLogAlways("CFlowProximityTriggerNode::ProcessEvent - Initializing flowgraph node");
            m_pActInfo = *pActInfo;

            // Retrieve the assigned entity from the input port
            EntityId triggerEntityId = GetPortEntityId(pActInfo, 0); // Input port index 0
            if (triggerEntityId != INVALID_ENTITYID)
            {
                CryLogAlways("CFlowProximityTriggerNode::ProcessEvent - Assigned Trigger Entity ID: %d", triggerEntityId);
                m_triggerEntityId = triggerEntityId;

                // Register for events on the assigned entity
                EnableTrigger(true);
            }
            else
            {
                CryLogAlways("CFlowProximityTriggerNode::ProcessEvent - No Trigger Entity assigned!");
            }
        }
    }



    void OnEntityEnter(EntityId entityId)
    {
        CryLogAlways("CFlowProximityTriggerNode::OnEntityEnter - EntityId: %d", entityId);
        ActivateOutput(&m_pActInfo, 0, entityId); // EntityEnter output
    }

    void OnEntityLeave(EntityId entityId)
    {
        CryLogAlways("CFlowProximityTriggerNode::OnEntityLeave - EntityId: %d", entityId);
        ActivateOutput(&m_pActInfo, 1, entityId); // EntityLeave output
    }

    virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override
    {
        // Serialization logic (if needed)
    }

    virtual void GetMemoryUsage(ICrySizer* sizer) const override
    {
        sizer->AddObject(this, sizeof(*this));
    }

    void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) override
    {
        CryLogAlways("CFlowProximityTriggerNode::OnEntityEvent - Event ID: %d, EntityId: %d", event.event, pEntity->GetId());

        if (event.event == ENTITY_EVENT_ENTERAREA)
        {
            OnEntityEnter(pEntity->GetId());
        }
        else if (event.event == ENTITY_EVENT_LEAVEAREA)
        {
            OnEntityLeave(pEntity->GetId());
        }
    }


private:
    void EnableTrigger(bool enable)
    {
        if (m_triggerEntityId == INVALID_ENTITYID)
        {
            CryLogAlways("CFlowProximityTriggerNode::EnableTrigger - Trigger Entity is invalid!");
            return;
        }

        CryLogAlways("CFlowProximityTriggerNode::EnableTrigger - Trigger Entity ID: %d", m_triggerEntityId);

        if (enable)
        {
            CryLogAlways("CFlowProximityTriggerNode::EnableTrigger - Registering for events");
            gEnv->pEntitySystem->AddEntityEventListener(m_triggerEntityId, ENTITY_EVENT_ENTERAREA, this);
            gEnv->pEntitySystem->AddEntityEventListener(m_triggerEntityId, ENTITY_EVENT_LEAVEAREA, this);
        }
        else
        {
            CryLogAlways("CFlowProximityTriggerNode::EnableTrigger - Unregistering from events");
            gEnv->pEntitySystem->RemoveEntityEventListener(m_triggerEntityId, ENTITY_EVENT_ENTERAREA, this);
            gEnv->pEntitySystem->RemoveEntityEventListener(m_triggerEntityId, ENTITY_EVENT_LEAVEAREA, this);
        }
    }





    SActivationInfo m_pActInfo;
};


REGISTER_FLOW_NODE("Triggers:Proximity Trigger", CFlowProximityTriggerNode); 