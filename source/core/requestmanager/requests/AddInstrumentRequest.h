/**********************************************************************
**
** This file is part of ANGLECORE, an open-source software development
** kit for audio plugins.
**
** ANGLECORE is free software: you can redistribute it and / or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** ANGLECORE is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with ANGLECORE.If not, see <https://www.gnu.org/licenses/>.
**
** Copyright(C) 2020, ANGLE
**
**********************************************************************/

#pragma once

#include "../Request.h"
#include "../../audioworkflow/AudioWorkflow.h"
#include "../../renderer/Renderer.h"
#include "ConnectionRequest.h"
#include "../../audioworkflow/ParameterRegistrationPlan.h"
#include "../../../config/AudioConfig.h"
#include "../../../config/RenderingConfig.h"

namespace ANGLECORE
{
    /**
    * \class AddInstrumentRequest AddInstrumentRequest.h
    * When the end-user adds a new Instrument to an AudioWorkflow, an instance of
    * this class is created to request the Mixer of the AudioWorkflow to activate
    * the corresponding rack for its mixing process, to create the necessary
    * connections within the AudioWorkflow, and to add entries to the corresponding
    * ParameterRegister.
    */
    template <class InstrumentType>
	class AddInstrumentRequest :
        public Request
	{
    public:
        AddInstrumentRequest(AudioWorkflow& audioWorkflow, Renderer& renderer);
        AddInstrumentRequest(const AddInstrumentRequest<InstrumentType>& other) = delete;

        /**
        * Returns true if the preprocessing went well, that is if a free spot was
        * found for the Instrument and all the corresponding instances were
        * successfully created accordingly, and false otherwise.
        */
        bool preprocess() override;

        /**
        * Connects the Instrument instances created during the preprocessing step to
        * the real-time rendering pipeline within the AudioWorkflow.
        */
        void process();

    private:
        AudioWorkflow& m_audioWorkflow;
        Renderer& m_renderer;

        unsigned short m_selectedRackNumber;

        /**
        * ConnectionRequest that instructs to add connections to the AudioWorkflow
        * the Instrument will be inserted into.
        */
        ConnectionRequest m_connectionRequest;

        /**
        * ParameterRegistrationPlan that instructs to add entries to the
        * ParameterRegister of the AudioWorkflow the Instrument will be inserted
        * into.
        */
        ParameterRegistrationPlan m_parameterRegistrationPlan;
	};

    template<class InstrumentType>
    AddInstrumentRequest<InstrumentType>::AddInstrumentRequest(AudioWorkflow& audioWorkflow, Renderer& renderer) :
        Request(),
        m_audioWorkflow(audioWorkflow),
        m_renderer(renderer),
        m_connectionRequest(audioWorkflow, renderer),

        /*
        * The selected rack number is initialized to the total number of racks,
        * which is an out-of-range index to all Voices' racks that signals no racks
        * has been selected.
        */
        m_selectedRackNumber(ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE)
    {}

    template<class InstrumentType>
    bool AddInstrumentRequest<InstrumentType>::preprocess()
    {
        std::lock_guard<std::mutex> scopedLock(m_audioWorkflow.getLock());

        /* Can we insert a new instrument? We need to find an empty spot first: */
        unsigned short emptyRackNumber = m_audioWorkflow.findEmptyRack();
        if (emptyRackNumber >= ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE)
        {
            /*
            * There is no empty spot, so we stop here and return false to signal the
            * caller the operation failed:
            */
            return false;
        }

        /* Otherwise, if we found an empty spot, we assign the Instrument to it: */
        m_selectedRackNumber = emptyRackNumber;

        /*
        * We then need to create all the Instrument instances and prepare their
        * environment before connecting the instruments to the real-time rendering
        * pipeline.
        */

        ConnectionPlan& connectionPlan = m_connectionRequest.plan;

        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            /*
            * We create an Instrument of the given type, and then cast it to an
            * Instrument, to ensure type validity.
            */
            std::shared_ptr<Instrument> instrument = std::make_shared<InstrumentType>();

            /*
            * Then, we insert the Instrument into the Workflow and plan its bridging
            * to the real-time rendering pipeline.
            */
            m_audioWorkflow.addInstrumentAndPlanBridging(v, m_selectedRackNumber, instrument, connectionPlan, m_parameterRegistrationPlan);
        }

        /*
        * Once here, we have a ConnectionPlan and a ParameterRegisterPlan ready to
        * be used. We now need to precompute the consequences of executing the
        * ConnectionPlan and connecting all of the instruments to the
        * AudioWorkflow's real-time rendering pipeline.
        */

        /*
        * We first calculate the rendering sequence that will take effect right
        * after the connection plan is executed.
        */
        std::vector<std::shared_ptr<Worker>> newRenderingSequence = m_audioWorkflow.buildRenderingSequence(connectionPlan);

        /*
        * And from that sequence, we can precompute and assign the rest of the
        * request properties:
        */
        m_connectionRequest.newRenderingSequence = newRenderingSequence;
        m_connectionRequest.newVoiceAssignments = m_audioWorkflow.getVoiceAssignments(newRenderingSequence);
        m_connectionRequest.oneIncrements.resize(newRenderingSequence.size(), 1);

        /*
        * If we arrive here, then the preparation went well, so we return true for
        * the Request to be then sent to the real-time thread and processed.
        */
        return true;
    }

    template<class InstrumentType>
    void AddInstrumentRequest<InstrumentType>::process()
    {
        /*
        * We create all the necessary connections between the Instrument and the
        * real-time rendering pipeline:
        */
        m_connectionRequest.process();

        /*
        * Then, we ask the AudioWorkflow to execute the parameter registration
        * plan corresponding to the current request. This will cause the
        * AudioWorkflow to add some parameters to its registers, so that the
        * end-user can then change their value through parameter change
        * requests.
        */
        m_audioWorkflow.executeParameterRegistrationPlan(m_parameterRegistrationPlan);

        /*
        * Once all connections are made, we finish with the update of the racks
        * in the workflow.
        */
        m_audioWorkflow.activateRack(m_selectedRackNumber);

        success.store(m_connectionRequest.success.load());
    }
}