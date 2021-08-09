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

        /**
        * \struct Listener AddInstrumentRequest.h
        * Since requests are processed asynchronously, information on their
        * execution is not available immediately upon posting. As a consequence,
        * ANGLECORE implements a broadcaster-listener mechanism for every type of
        * Request, so that all requests can send information back to their caller.
        * 
        * In that context, the AddInstrumentRequest::Listener structure defines the
        * listener associated with the AddInstrumentRequest class. An
        * AddInstrumentRequest::Listener can be attached to an AddInstrumentRequest
        * object based on the same Instrument class. Once the attachement is made,
        * the AddInstrumentRequest::Listener will be called by the RequestManager
        * during postprocessing to receive information about how the Instrument's
        * insertion went. Note that such attachment is not mandatory for the request
        * to be processed: an AddInstrumentRequest can be posted to the
        * RequestManager without any listener, in which case the RequestManager will
        * not signal when the request is processed.
        * 
        * The AddInstrumentRequest::Listener structure provides two pure virtual
        * callback methods that must be implemented in derived classes:
        * addedInstrument() and failedToAddInstrument(). Only one of these two
        * methods will be called by the RequestManager, depending on how the
        * execution went. If the AddInstrumentRequest is successfully processed, and
        * the corresponding Instrument objects successfully created and inserted
        * into the AudioWorkflow, then the addedInstrument() method will be called.
        * Otherwise, if the processing fails, then the failedToAddInstrument()
        * method will be called. Both of these methods are always called on one of
        * the RequestManager's non real-time threads.
        */
        struct Listener
        {
            /**
            * This method serves as a callback for the AddInstrumentRequest being
            * listened to. It is called during postprocessing and only if the
            * request was successfully executed, that is if the Instrument objects
            * were all successfully created and inserted into the AudioWorkflow. In
            * this case, this method is called by one of the RequestManager's non
            * real-time threads during the request's postprocessing.
            * 
            * If the AddInstrumentRequest being listened to does not succeed in its
            * execution, then the failedToAddInstrument() method will be called
            * instead.
            * @param[in] selectedRackNumber The rack number where the Instrument has
            *   been inserted.
            * @param[in] sourceRequest A reference to the request being listened to,
            *   which is at the origin of this callback. This parameter can be used
            *   to retrieve information about how the request's execution went, or
            *   to differentiate implementation between instrument types.
            */
            virtual void addedInstrument(unsigned short selectedRackNumber, const AddInstrumentRequest<InstrumentType>& sourceRequest) = 0;

            /**
            * This method serves as a callback for the AddInstrumentRequest being
            * listened to. It is called during postprocessing and only if the
            * request was not executed correctly, and failed during either
            * preprocessing or processing.
            * 
            * The preprocessing step will fail if there are no free slots available
            * for inserting a new Instrument, or if an error occurs during the
            * preparation of the AudioWorkflow. The processing step will fail if an
            * error occurs when changing the connections of the AudioWorkflow to
            * link all the new Instrument objects to the real-time rendering
            * pipeline. In either case, this method will be called by one of the
            * RequestManager's non real-time threads during the request's
            * postprocessing.
            * 
            * If the AddInstrumentRequest being listened does succeed in its
            * execution, then the addedInstrument() method will be called instead.
            * @param[in] intendedRackNumber The rack number that was selected during
            *   preprocessing for inserting the new Instrument. If that number is
            *   equal to or greater than the maximum number of instruments
            *   (ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE), then it means there were
            *   no spots left for inserting the new Instrument.
            * @param[in] sourceRequest A reference to the request being listened to,
            *   which is at the origin of this callback. This parameter can be used
            *   to retrieve information about what went wrong during the request's
            *   execution, or to differentiate implementation between instrument
            *   types.
            */
            virtual void failedToAddInstrument(unsigned short intendedRackNumber, const AddInstrumentRequest<InstrumentType>& sourceRequest) = 0;
        };

        AddInstrumentRequest(AudioWorkflow& audioWorkflow, Renderer& renderer);
        AddInstrumentRequest(AudioWorkflow& audioWorkflow, Renderer& renderer, Listener* listener);
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

        /**
        * Calls the request's Listener to send information about how the request's
        * execution went.
        */
        void postprocess() override;

    private:
        AudioWorkflow& m_audioWorkflow;
        Renderer& m_renderer;
        Listener* m_listener;

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
        AddInstrumentRequest<InstrumentType>(audioWorkflow, renderer, nullptr)
    {}

    template<class InstrumentType>
    AddInstrumentRequest<InstrumentType>::AddInstrumentRequest(AudioWorkflow& audioWorkflow, Renderer& renderer, Listener* listener) :
        Request(),
        m_audioWorkflow(audioWorkflow),
        m_renderer(renderer),
        m_listener(listener),
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

    template<class InstrumentType>
    void AddInstrumentRequest<InstrumentType>::postprocess()
    {
        if (m_listener)
        {
            if (hasBeenPreprocessed.load() && hasBeenProcessed.load() && success.load())
                m_listener->addedInstrument(m_selectedRackNumber, *this);
            else
                m_listener->failedToAddInstrument(m_selectedRackNumber, *this);
        }
    }

    /** Handy short name for listeners of AddInstrumentRequest objects */
    template<class InstrumentType>
    using AddInstrumentListener = typename AddInstrumentRequest<InstrumentType>::Listener;
}