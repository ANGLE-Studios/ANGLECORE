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
    {
        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            m_voiceIsStopping[v] = false;
            m_stopTrackers[v].stopDurationInSamples = 0;
            m_stopTrackers[v].position = 0;
        }
    }

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
        * STEP 1/3: PROCESS REQUESTS
        * ===================================
        */

        processRequests();

        /*
        * ===================================
        * STEP 2/3: RENDERING
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
            splitAndRenderNextAudioBlock(audioBlockToGenerate, numChannels, numSamples, 0);
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

                    splitAndRenderNextAudioBlock(audioBlockToGenerate, numChannels, samplesBeforeNextMessage, position);

                    processMIDIMessage(message);

                    position += samplesBeforeNextMessage;
                }
            }

            /*
            * Once here, we know we have processed the last valid MIDI message, but
            * we also know we have not rendered all the subsequent samples to the
            * end of the audio block (since this is always done before rendering the
            * upcoming MIDI message, as implemented above). So we need to call our
            * renderer one last time.
            */

            splitAndRenderNextAudioBlock(audioBlockToGenerate, numChannels, numSamples - position, position);
        }
    }

    void Master::splitAndRenderNextAudioBlock(float** audioBlockToGenerate, unsigned short numChannels, uint32_t numSamples, uint32_t startSample)
    {
        /*
        * The purpose of this method is to fulfill the Master's responsability to
        * only request a valid number of samples to the renderer. Since a valid
        * number of samples is a number that is both greater than 0 and less than
        * ANGLECORE_FIXED_STREAM_SIZE, we need to cut the audio block to generate
        * into smaller chunks, using the same idea as the Euclidian division: we
        * launch rendering sessions on blocks of size ANGLECORE_FIXED_STREAM_SIZE
        * until we obtain one remainder, which size will consequently be between 0
        * and ANGLECORE_FIXED_STREAM_SIZE - 1. This is what this method does, which
        * ensures that the number of samples requested to the renderer is always
        * less than ANGLECORE_FIXED_STREAM_SIZE. The method also ensures we never
        * ask the renderer to generate 0 samples, which completes the requirements.
        */

        /*
        * As stated above, we only call the renderer if the number of samples
        * requested is greater than zero:
        */
        if (numSamples > 0)
        {
            uint32_t start = startSample;
            uint32_t remainingSamples = numSamples;
            while (remainingSamples >= ANGLECORE_FIXED_STREAM_SIZE)
            {
                m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, start);
                m_renderer.render(ANGLECORE_FIXED_STREAM_SIZE);
                start += ANGLECORE_FIXED_STREAM_SIZE;
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
                m_audioWorkflow.setExporterOutput(audioBlockToGenerate, numChannels, start);
                m_renderer.render(remainingSamples);
            }

            /*
            * Finally, we update the stop trackers of the voices currently
            * generating their audio tails before being turned off. Since we are in
            * between two MIDI messages here, we can simply do this update once,
            * instead of doing it after rendering each individual chunk for the same
            * result.
            */
            updateStopTrackersAfterRendering(numSamples);
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
                /*
                * We request all the voices that are currently playing the message's
                * note to stop playing. Note that stopping the voices will not turn
                * them off immediately in general: the instruments inside the voices
                * will likely need some time to completely fade out, before the
                * voice can be turned off. The purpose of the m_stopTrackers
                * variable is precisely to track that time before turning the voices
                * off.
                */
                for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
                {
                    /*
                    * Note that the AudioWorkflow's playsNoteNumber() method returns
                    * true when the given voice is on AND playing the note, so using
                    * it will properly avoid voices that are off but were playing
                    * the note before, which we do not want to stop again (primarily
                    * because it will call the instruments'
                    * computeStopDurationInSamples() and stopPlaying() methods in an
                    * illegal order, before reset() and startPlaying() are called).
                    */
                    if (m_audioWorkflow.playsNoteNumber(v, message.noteNumber))
                    {
                        /*
                        * If the voice is on and playing the note that has been
                        * released, then we need to stop the voice.
                        */

                        /*
                        * We first register the voice as being in a stopping state:
                        */
                        m_voiceIsStopping[v] = true;

                        /*
                        * And then we effectively stop the voice. To do that, we
                        * call the AudioWorkflow's stopVoice() method which stops
                        * the given voice, and returns number of samples that the
                        * voice needs to complete its fade out properly.
                        */
                        uint32_t voiceStopDuration = m_audioWorkflow.stopVoice(v);

                        /*
                        * We finally use that value to initialize the voice's stop
                        * tracker.
                        */
                        m_stopTrackers[v].stopDurationInSamples = voiceStopDuration;
                        m_stopTrackers[v].position = 0;
                    }
                }
            }
            break;
        }
    }

    void Master::updateStopTrackersAfterRendering(uint32_t numSamples)
    {
        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            if (m_voiceIsStopping[v])
            {
                /*
                * We first retrieve the stop tracker corresponding to the current
                * voice:
                */
                StopTracker& stopTracker = m_stopTrackers[v];

                /*
                * We start by updating the tracker's position. We assume the voice
                * tails are always short enough so that the tracker's position will
                * never overflow.
                */
                stopTracker.position += numSamples;

                if (stopTracker.position >= stopTracker.stopDurationInSamples)
                {
                    /* We turn off the current voice */
                    m_audioWorkflow.turnVoiceOffAndSetItFree(v);
                    m_renderer.turnVoiceOff(v);

                    /*
                    * And we save the information that the voice is no longer in its
                    * stopping state, which means it no longer generates its tail,
                    * in order to stop the Master from further updating the voice's
                    * tracker.
                    */
                    m_voiceIsStopping[v] = false;
                }
            }
        }
    }
}