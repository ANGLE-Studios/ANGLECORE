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

#include "Master.h"

#include "../../config/RenderingConfig.h"

namespace ANGLECORE
{
    Master::Master() :

        /*
        * The instrument request queue should only handle one request at a time, so
        * there is no need to reserve more space than one slot.
        */
        m_instrumentRequests(1)
    {}

    void Master::setSampleRate(floating_type sampleRate)
    {
        m_audioWorkflow.setSampleRate(sampleRate);
    }

    void Master::clearMIDIBufferForNextAudioBlock()
    {
        m_midiBuffer.clear();
    }

    MIDIMessage& Master::pushBackNewMIDIMessage()
    {
        return m_midiBuffer.pushBackNewMIDIMessage();
    }

    void Master::renderNextAudioBlock(float** audioBlockToGenerate, unsigned short numChannels, uint32_t numSamples)
    {
        /*
        * ===================================
        * STEP 1/2: PROCESS REQUESTS
        * ===================================
        */

        processRequests();

        /*
        * ===================================
        * STEP 2/2: RENDERING
        * ===================================
        */

        uint32_t numMIDIMessages = m_midiBuffer.getNumMIDIMessages();

        /*
        * If there is no MIDI message in the MIDI buffer, then we can use a shortcut
        * and directly proceed to the rendering, without imposing extra computation
        * on the rendering process.
        */
        if (numMIDIMessages == 0)
        {
            /*
            * Although we took a shortcut, we still need to fulfill the Master's
            * responsability to only request a valid number of samples to the
            * renderer. Therefore, we cut the audio block to generate into smaller
            * chunks, using the same idea as the Euclidian division: we launch
            * rendering sessions on blocks of size ANGLECORE_FIXED_STREAM_SIZE until
            * we obtain one remainder, which size will consequently be between 0 and
            * ANGLECORE_FIXED_STREAM_SIZE - 1.
            */

            uint32_t startSample = 0;
            uint32_t remainingSamples = numSamples;
            while (remainingSamples >= ANGLECORE_FIXED_STREAM_SIZE)
            {
                m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, startSample);
                m_renderer.render(ANGLECORE_FIXED_STREAM_SIZE);
                startSample += ANGLECORE_FIXED_STREAM_SIZE;
                remainingSamples -= ANGLECORE_FIXED_STREAM_SIZE;
            }

            /*
            * Now we are left with the remainder, but since we can only call the
            * renderer if the number of samples to render is not null, we need to
            * test whether or not the remainder is empty before launching a new
            * rendering session:
            */
            if (remainingSamples != 0)
            {
                m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, startSample);
                m_renderer.render(remainingSamples);
            }
        }
        else
        {
            /*
            * Throughout all of the rendering process and MIDI processing, we track
            * the position of the next sample to render with this variable:
            */
            uint32_t position = 0;

            for (uint32_t i = 0; i < numMIDIMessages; i++)
            {
                const MIDIMessage& message = m_midiBuffer[i];
                
                /*
                * We only process the MIDI message if its timestamp is in-range,
                * and if the message is properly placed within the MIDI Buffer, i.e.
                * its timestamp is greater than or equal to the maximum of all the
                * timestamps of the MIDI messages placed before. To check if the
                * latter property is respected, we can simply use the current
                * position within the audio block.
                */
                if (message.timestamp < numSamples && message.timestamp >= position)
                {
                    uint32_t samplesBeforeNextMessage = message.timestamp - position;

                    /*
                    * To fulfill the Master's responsability to only request a valid
                    * number of samples to the renderer, we cut the audio block to
                    * generate into smaller chunks, using the same process as in the
                    * shortcut above.
                    */

                    uint32_t startSample = position;
                    uint32_t remainingSamples = samplesBeforeNextMessage;
                    while (remainingSamples >= ANGLECORE_FIXED_STREAM_SIZE)
                    {
                        m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, startSample);
                        m_renderer.render(ANGLECORE_FIXED_STREAM_SIZE);
                        startSample += ANGLECORE_FIXED_STREAM_SIZE;
                        remainingSamples -= ANGLECORE_FIXED_STREAM_SIZE;
                    }

                    /*
                    * Since we cannot ask the renderer to render zero samples, we
                    * add the same test here as in the shortcut above:
                    */
                    if (remainingSamples != 0)
                    {
                        m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, startSample);
                        m_renderer.render(remainingSamples);
                    }

                    processMIDIMessage(message);

                    position += samplesBeforeNextMessage;
                }
            }

            /*
            * Once here, we know we have processed the last valid MIDI message, but
            * we also know we have not rendered all the subsequent samples to the
            * end of the audio block (since this is always done before rendering the
            * upcoming MIDI message, as implemented above). So we need to call our
            * renderer one last time, always in the same process.
            */

            uint32_t startSample = position;
            uint32_t remainingSamples = numSamples - position;
            while (remainingSamples >= ANGLECORE_FIXED_STREAM_SIZE)
            {
                m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, startSample);
                m_renderer.render(ANGLECORE_FIXED_STREAM_SIZE);
                startSample += ANGLECORE_FIXED_STREAM_SIZE;
                remainingSamples -= ANGLECORE_FIXED_STREAM_SIZE;
            }
            if (remainingSamples != 0)
            {
                m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, startSample);
                m_renderer.render(remainingSamples);
            }
        }
    }

    void Master::processRequests()
    {
        {
            /*
            * We define a scope here so that the following pointer is deleted as
            * soon as possible. This pointer will hold a copy of any
            * InstrumentRequest that has been sent by the Master to itself (on
            * different threads). When it is deleted, its reference count will
            * decrement, and hereby signal to the Master the InstrumentRequest has
            * been processed.
            */
            std::shared_ptr<InstrumentRequest> request;

            /* Has an InstrumentRequest been received? ... */
            if (m_instrumentRequests.pop(request) && request)
            {
                /*
                * ... YES! So we need to process the request. Note that null
                * pointers are ignored.
                */

                switch (request->type)
                {
                case InstrumentRequest::Type::ADD:
                    m_audioWorkflow.turnRackOn(request->rackNumber);
                    break;

                case InstrumentRequest::Type::REMOVE:
                    m_audioWorkflow.turnRackOff(request->rackNumber);
                    break;
                }

                ConnectionRequest& connectionRequest = request->connectionRequest;

                /* We first test if the connection request is valid... */
                uint32_t size = connectionRequest.newRenderingSequence.size();
                if (size > 0 && connectionRequest.newVoiceAssignments.size() == size && connectionRequest.oneIncrements.size() == size)
                {
                    /* The request is valid, so we execute the ConnectionPlan... */
                    bool success = m_audioWorkflow.executeConnectionPlan(connectionRequest.plan);

                    /*
                    * ... And notify the Master through the request if the execution
                    * was a success:
                    */
                    connectionRequest.hasBeenSuccessfullyProcessed.store(success);

                    /*
                    * If the connection request has failed, it could mean the
                    * connection plan has only been partially executed, in which
                    * case we will still want the renderer to update its internal
                    * rendering sequence, so we call the processConnectionRequest()
                    * method on the renderer, independently from the value of the
                    * 'success' boolean.
                    */
                    m_renderer.processConnectionRequest(connectionRequest);
                }
            }
        }
    }

    void Master::processMIDIMessage(const MIDIMessage& message)
    {
        switch (message.type)
        {

        case MIDIMessage::Type::NOTE_ON:
            {
                /* Can we play a new note? We need to find a free voice first */
                unsigned short freeVoiceNumber = m_audioWorkflow.findFreeVoice();
                if (freeVoiceNumber >= ANGLECORE_NUM_VOICES)

                    /*
                    * There is no free voice, and since ANGLECORE does not accept
                    * voice stealing, we stop here and ignore the MIDI message.
                    */
                    return;

                /*
                * If a free voice is available, then we take it, and request it to
                * play the given note. This will trigger all the instruments inside
                * the voice to reset and start playing for the next audio block.
                */
                m_audioWorkflow.takeVoiceAndPlayNote(freeVoiceNumber, message.noteNumber, message.noteVelocity);

                /*
                * Finally, we need to turn the voice on, and send the information it
                * is effectively on to every agent holding a copy of the voice's
                * current status.
                */
                m_audioWorkflow.turnVoiceOn(freeVoiceNumber);
                m_renderer.turnVoiceOn(freeVoiceNumber);
            }
            break;

        case MIDIMessage::Type::NOTE_OFF:
            {
                // TO DO: For the moment, we simply and brutally turn the voice off.
                for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
                {
                    if (m_audioWorkflow.playsNoteNumber(v, message.noteNumber))
                    {
                        m_audioWorkflow.turnVoiceOffAndSetItFree(v);
                        m_renderer.turnVoiceOff(v);
                    }
                }
            }
            break;
        }
    }
}