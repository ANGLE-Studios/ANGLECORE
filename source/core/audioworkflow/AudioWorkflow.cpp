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

#include <algorithm>

#include "AudioWorkflow.h"

#include "../../config/AudioConfig.h"
#include "../../utility/MIDI.h"

namespace ANGLECORE
{
    AudioWorkflow::AudioWorkflow() :
        Workflow(),
        VoiceAssigner(),
        Lockable(),
        m_exporter(std::make_shared<Exporter>()),
        m_mixer(std::make_shared<Mixer>())
    {
        /*
        * ===================================
        * STEP 1/3: MIXER AND EXPORTER
        * ===================================
        */

        /* We add the Exporter and the Mixer to the AudioWorkflow */
        addWorker(m_exporter);
        addWorker(m_mixer);

        /* We connect the mixer into the exporter */
        for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
        {
            std::shared_ptr<Stream> exporterInputStream = std::make_shared<Stream>();
            addStream(exporterInputStream);

            plugStreamIntoWorker(exporterInputStream->id, m_exporter->id, c);
            plugWorkerIntoStream(m_mixer->id, c, exporterInputStream->id);
        }

        /*
        * We prepare all of the Mixer's input streams, so that the memory is already
        * allocated, and the streams do not need to be created and destroyed
        * repeatedly, which would have unnecessarily consumed ID numbers.
        */
        for (unsigned short i = 0; i < m_mixer->getNumInputs(); i++)
        {
            std::shared_ptr<Stream> mixerInputStream = std::make_shared<Stream>();
            addStream(mixerInputStream);
            plugStreamIntoWorker(mixerInputStream->id, m_mixer->id, i);
        }

        /*
        * ===================================
        * STEP 2/3: GLOBAL CONTEXT
        * ===================================
        */

        /* We add the global context's streams */
        addStream(m_globalContext.sampleRateStream);
        addStream(m_globalContext.sampleRateReciprocalStream);

        /*
        * ===================================
        * STEP 3/3: VOICE CONTEXTS
        * ===================================
        */

        /*
        * Then we need to take care of all voices. We add their respective context
        * generator, and build their output streams.
        */
        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            VoiceContext& voiceContext = m_voices[v].voiceContext;

            /* We add the voice context's workers and streams */
            addWorker(voiceContext.frequencyGenerator);
            addStream(voiceContext.frequencyStream);
            addWorker(voiceContext.ratioCalculator);
            addStream(voiceContext.frequencyOverSampleRateStream);
            addStream(voiceContext.velocityStream);

            /*
            * And we properly assign the workers to the current voice. Note that, by
            * construction of a VoiceContext, we know none of voiceContext's member
            * pointers are null, so we do not need to check for null pointers here:
            */
            assignVoiceToWorker(v, voiceContext.frequencyGenerator->id);
            assignVoiceToWorker(v, voiceContext.ratioCalculator->id);

            /*
            * Then we create the connections between the context's items. Again, for
            * the same reason as above, we do not need to check for null pointers
            * here:
            */
            plugWorkerIntoStream(voiceContext.frequencyGenerator->id, 0, voiceContext.frequencyStream->id);
            plugStreamIntoWorker(voiceContext.frequencyStream->id, voiceContext.ratioCalculator->id, VoiceContext::RatioCalculator::Input::FREQUENCY);
            plugStreamIntoWorker(m_globalContext.sampleRateReciprocalStream->id, voiceContext.ratioCalculator->id, VoiceContext::RatioCalculator::Input::SAMPLE_RATE_RECIPROCAL);
            plugWorkerIntoStream(voiceContext.ratioCalculator->id, 0, voiceContext.frequencyOverSampleRateStream->id);
        }
    }

    void AudioWorkflow::setSampleRate(floating_type sampleRate)
    {
        m_globalContext.setSampleRate(sampleRate);
    }

    std::vector<std::shared_ptr<Worker>> AudioWorkflow::buildRenderingSequence(const ConnectionPlan& connectionPlan) const
    {
        std::vector<std::shared_ptr<Worker>> renderingSequence;
        completeRenderingSequenceForWorker(m_exporter, connectionPlan, renderingSequence);
        return renderingSequence;
    }

    void AudioWorkflow::setExporterOutput(float** buffer, unsigned short numChannels, uint32_t startSample)
    {
        m_exporter->setOutputBuffer(buffer, numChannels, startSample);
    }

    unsigned short AudioWorkflow::findEmptyRack() const
    {
        /* We test every rack to see if it is empty in every voice */
        for (unsigned short r = 0; r < ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE; r++)
        {
            /*
            * This boolean will indicate if the current rack is empty in every voice
            * and if, therefore, we should return its number:
            */
            bool isEmptyForEveryVoice = true;

            /*
            * We check every voice and stop if the rack is found occupied at some
            * point.
            */
            for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES && isEmptyForEveryVoice; v++)
                isEmptyForEveryVoice = isEmptyForEveryVoice && m_voices[v].racks[r].isEmpty;

            /* If we found an empty rack, we return its number: */
            if (isEmptyForEveryVoice)
                return r;
        }

        /*
        * If we arrive here, this means we could not find any empty spot, so we
        * return the number of racks, which is an out-of-range index to the Voices'
        * racks that will signal the caller the research failed:
        */
        return ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE;
    }

    void AudioWorkflow::addInstrumentAndPlanBridging(unsigned short voiceNumber, unsigned short rackNumber, const std::shared_ptr<Instrument>& instrument, ConnectionPlan& connectionPlanToComplete, ParameterRegistrationPlan& parameterRegistrationPlan)
    {
        /* We first add the instrument to the workflow... */
        addWorker(instrument);

        /* ... And then register it into the given voice. */
        m_voices[voiceNumber].racks[rackNumber].instrument = instrument;
        m_voices[voiceNumber].racks[rackNumber].isEmpty = false;
        assignVoiceToWorker(voiceNumber, instrument->id);

        /*
        * Then, we plan the connection of the instrument to the audio workflow's
        * global and voice contexts, based on the instrument's internal
        * configuration.
        */
        const Instrument::ContextConfiguration& configuration = instrument->getContextConfiguration();
        if (configuration.receiveSampleRate)
            connectionPlanToComplete.streamToWorkerPlugInstructions.emplace_back(getSampleRateStreamID(), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::SAMPLE_RATE));
        if (configuration.receiveSampleRateReciprocal)
            connectionPlanToComplete.streamToWorkerPlugInstructions.emplace_back(getSampleRateReciprocalStreamID(), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::SAMPLE_RATE_RECIPROCAL));
        if (configuration.receiveFrequency)
            connectionPlanToComplete.streamToWorkerPlugInstructions.emplace_back(getFrequencyStreamID(voiceNumber), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::FREQUENCY));
        if (configuration.receiveFrequencyOverSampleRate)
            connectionPlanToComplete.streamToWorkerPlugInstructions.emplace_back(getFrequencyOverSampleRateStreamID(voiceNumber), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::FREQUENCY_OVER_SAMPLE_RATE));
        if (configuration.receiveVelocity)
            connectionPlanToComplete.streamToWorkerPlugInstructions.emplace_back(getVelocityStreamID(voiceNumber), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::VELOCITY));

        /*
        * After handling the interaction between the instrument and the workflow's
        * contexts, now is the turn of the instrument's specific parameters to be
        * configured. We loop through each parameter to create its specific, short
        * rendering pipeline.
        */

        /* We loop through each parameter */
        for (const Parameter& parameter : instrument->getParameters())
        {
            /*
            * We first test if the parameter generators already exist for that
            * instrument, using the content of the current parameter registration
            * plan:
            */
            auto parameterRegistrationPlanIterator = std::find_if(
                parameterRegistrationPlan.addInstructions.cbegin(),
                parameterRegistrationPlan.addInstructions.cend(),

                /*
                * We use a lambda function to detect if an entry instruction in the
                * plan matches the current rack and parameter. We ensure we capture
                * the proper external variables in the capture list [], while
                * avoiding unecessary copies by passing them by reference. Note that
                * the lambda function will search for the parameter's identifier
                * first, as it is more likely that the rack number is already
                * correct when inserting several instruments in a row at a given
                * rack. Thanks to operator&& precedence, this should slightly help
                * improve performance.
                */
                [&parameter, &rackNumber](ParameterRegistrationPlan::Instruction instruction) { return instruction.parameterIdentifier == parameter.identifier && instruction.rackNumber == rackNumber; }
            );

            /*
            * If the parameter registration plan DOES contain an instruction for the
            * current parameter, then we need to use it to connect the instrument to
            * the existing generators. Note that it is not necessary to test if
            * another instruction exists to rather remove the parameter from the
            * register (in the "removeInstructions" vector), as removal instructions
            * will be executed first, and additions second.
            */
            if (parameterRegistrationPlanIterator != parameterRegistrationPlan.addInstructions.cend())
            {
                /*
                * We first retrieve the parameter stream from the parameter
                * registration plan. We do not need to retrieve the generator, as we
                * can directly use the stream and connect it to the instrument:
                */
                const std::shared_ptr<Stream>& stream = parameterRegistrationPlanIterator->parameterStream;

                /*
                * Afterwards, we connect the stream to the instrument to construct a
                * short rendering pipeline, which feeds in the instrument. Note that
                * here, the generator and the stream are already connected together,
                * so we do not need to plug them into one another. We simply need to
                * take care of the connection between the stream and the instrument.
                */

                /*
                * We make this connection directly here rather than planning it, as
                * we know the parameter generator involved will only be called when
                * the instrument is properly connected to the real-time rendering
                * pipeline. Therefore, the generator will not interfere with the
                * current rendering pipeline.
                */
                plugStreamIntoWorker(stream->id, instrument->id, instrument->getInputPortNumber(parameter.identifier));
            }

            /*
            * Otherwise, if the parameter registration plan does NOT contain any
            * instruction for the current parameter, we need to create our own.
            */
            else
            {
                /*
                * For the current parameter, we create a ParameterGenerator that is
                * not assigned to any voice:
                */
                std::shared_ptr<ParameterGenerator> generator = std::make_shared<ParameterGenerator>(parameter);
                addWorker(generator);

                /*
                * Then we create a stream that will contain the output of the
                * generator, i.e. the parameter's value:
                */
                std::shared_ptr<Stream> stream = std::make_shared<Stream>();
                addStream(stream);

                /*
                * Afterwards, we connect those elements together with the instrument
                * to construct a short rendering pipeline, which feeds in the
                * instrument. Note that we make these connections directly here
                * rather than planning them, as we know the parameter generator
                * involved will only be called when the instrument is properly
                * connected to the real-time rendering pipeline. Therefore, the
                * generator will not interfere with the current rendering pipeline.
                */
                plugWorkerIntoStream(generator->id, 0, stream->id);
                plugStreamIntoWorker(stream->id, instrument->id, instrument->getInputPortNumber(parameter.identifier));

                /*
                * And finally, we add a new instruction to the parameter
                * registration plan.
                */
                parameterRegistrationPlan.addInstructions.emplace_back(rackNumber, parameter.identifier, generator, stream);
            }
        }

        /*
        * The connection plan is then completed for connecting the Instrument's
        * output bus into the Mixer.
        */
        for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
            connectionPlanToComplete.workerToStreamPlugInstructions.emplace_back(getMixerInputStreamID(voiceNumber, rackNumber, c), instrument->id, c);
    }

    unsigned short AudioWorkflow::findFreeVoice() const
    {
        /* We test every voice to see if it is free */
        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {

            /* If the current voice is free, we return its number: */
            if (m_voices[v].isFree)
                return v;
        }

        /*
        * If we arrive here, this means we could not find any free voice, so we
        * return the number of voices instead, which is an out-of-range index to the
        * Voices' array that will signal the caller the research failed:
        */
        return ANGLECORE_NUM_VOICES;
    }

    void AudioWorkflow::turnVoiceOn(unsigned short voiceNumber)
    {
        /* We first turn on the given voice */
        m_voices[voiceNumber].isOn = true;

        /* And then send the information to the mixer and exporter */
        m_mixer->turnVoiceOn(voiceNumber);
        m_exporter->incrementVoiceCount();
    }

    void AudioWorkflow::turnVoiceOffAndSetItFree(unsigned short voiceNumber)
    {
        /* We first turn off the given voice */
        m_voices[voiceNumber].isOn = false;

        /* And then send the information to the mixer and exporter */
        m_mixer->turnVoiceOff(voiceNumber);
        m_exporter->decrementVoiceCount();

        /* Afterwards, we set it free */
        m_voices[voiceNumber].isFree = true;
    }

    bool AudioWorkflow::playsNoteNumber(unsigned short voiceNumber, unsigned char noteNumber) const
    {
        return m_voices[voiceNumber].isOn && m_voices[voiceNumber].currentNoteNumber == noteNumber;
    }

    void AudioWorkflow::takeVoiceAndPlayNote(unsigned short voiceNumber, unsigned char noteNumber, unsigned char noteVelocity)
    {
        /*
        * We first retrieve the indicated voice, assuming the voiceNumber is
        * in-range.
        */
        Voice& voice = m_voices[voiceNumber];

        /* We take the voice: */
        voice.isFree = false;

        /*
        * Then we send the note to the voice, which includes sending the note
        * number, the corresponding frequency, and the velocity.
        */

        voice.currentNoteNumber = noteNumber;
        voice.voiceContext.frequencyGenerator->setParameterValue(MIDI::getFrequencyOf(noteNumber));

        /*
        * For the velocity, since it is a discrete parameter, we simply write its
        * value into the first sample of the voice context's corresponding stream:
        */
        floating_type* velocity = voice.voiceContext.velocityStream->getDataForWriting();
        velocity[0] = static_cast<floating_type>(noteVelocity);

        /*
        * Finally, we reset every instrument located in the voice so that they get
        * ready to play, and instruct them to effectively start playing.
        */
        for (unsigned short i = 0; i < ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE; i++)

            /*
            * Since the voice may not be full, we need to check if each of its rack
            * is activated and non-empty before accessing the instrument inside.
            * Note that the rack's 'isEmpty' member variable is meant for detecting
            * an empty rack that can be overriden, which does not imply that the
            * underlying instrument pointer is null. Conversely, we will not take
            * for granted that the pointer is not null when the rack is tagged as
            * 'non empty', so we perform a double check:
            */
            if (voice.racks[i].isActivated && !voice.racks[i].isEmpty && voice.racks[i].instrument)
            {
                voice.racks[i].instrument->turnOn();
                voice.racks[i].instrument->reset();
                voice.racks[i].instrument->startPlaying();
            }
    }

    void AudioWorkflow::activateRack(unsigned short rackNumber)
    {
        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
        {
            Voice& voice = m_voices[v];

            m_voices[v].racks[rackNumber].isActivated = true;

            /*
            * After we activate the rack for a voice, we need to check if that voice
            * is already on. If that is the case, then we need to first wake up and
            * prepare the rack's instrument through a reset and start up, so it will
            * properly render audio within the newly activated rack. We also need to
            * check if the instrument rack is empty before accessing the instrument
            * (even though when a rack is activated, it is most likely because a new
            * instrument was added or an existing instrument is unmuted). Note that
            * the rack's 'isEmpty' member variable is meant for detecting an empty
            * rack that can be overriden, which does not imply that the underlying
            * instrument pointer is null. Conversely, we will not take for granted
            * that the pointer is not null when the rack is tagged as 'non empty',
            * so we perform a double check.
            */
            if (voice.isOn && !voice.racks[rackNumber].isEmpty && voice.racks[rackNumber].instrument)
            {
                voice.racks[rackNumber].instrument->turnOn();
                voice.racks[rackNumber].instrument->reset();
                voice.racks[rackNumber].instrument->startPlaying();
            }
        }

        m_mixer->activateRack(rackNumber);
    }

    void AudioWorkflow::deactivateRack(unsigned short rackNumber)
    {
        for (unsigned short v = 0; v < ANGLECORE_NUM_VOICES; v++)
            m_voices[v].racks[rackNumber].isActivated = false;

        m_mixer->deactivateRack(rackNumber);
    }

    uint32_t AudioWorkflow::stopVoice(unsigned short voiceNumber)
    {
        /* We first retrieve the voice that should be stopped */
        Voice& voice = m_voices[voiceNumber];

        /*
        * The voice that must be stopped will generate an audio tail before being
        * turned off. This method must return the time it will take for that voice
        * to stop playing its audio tail, in terms of samples. Mathematically, the
        * voice's stop duration is simply the maximum of all its racks' stop
        * durations. We will use the following variable to compute this value:
        */
        uint32_t voiceStopDuration = 0;

        /* We stop each rack and compute its stop duration */
        for (unsigned short r = 0; r < ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE; r++)
        {
            /*
            * We need to check if the instrument rack is activated and non-empty
            * before accessing the instrument inside to stop it. Note that the
            * rack's 'isEmpty' member variable is meant for detecting an empty rack
            * that can be overriden, which does not imply that the underlying
            * instrument pointer is null. Conversely, we will not assume the pointer
            * is not null when the rack is tagged as 'non empty', so we perform a
            * double check:
            */
            if (voice.racks[r].isActivated && !voice.racks[r].isEmpty && voice.racks[r].instrument)
            {
                /* We first compute the instrument's stop duration in samples */
                uint32_t instrumentStopDuration = voice.racks[r].instrument->computeStopDurationInSamples();

                /*
                * The above 'instrumentStopDuration' variable currently stores the
                * number of samples the instrument needs to fade out before silence.
                * However, the instrument's configuration may change right here,
                * after one of its input parameter has received a change request for
                * instance. The 'instrumentStopDuration' variable may no longer
                * correspond to the instrument's needs, but it is ok to discard this
                * change once in a while: the instrument will take it into account
                * when another note is released.
                */

                /*
                * We now request the instrument to stop playing, and update our
                * current evaluation of the voice's stop duration accordingly.
                */
                voice.racks[r].instrument->prepareToStop(instrumentStopDuration);
                voice.racks[r].instrument->stopPlaying();
                if (instrumentStopDuration > voiceStopDuration)
                    voiceStopDuration = instrumentStopDuration;
            }
        }

        /* Finally, we return the voice's tail duration we have just computed */
        return voiceStopDuration;
    }

    void AudioWorkflow::executeParameterRegistrationPlan(ParameterRegistrationPlan& plan)
    {
        /* We execute then given plan, starting from the remove instructions. */

        for (auto& instruction : plan.removeInstructions)
        {
            /*
            * We check if the instruction's rack number is valid (this is the
            * equivalent of checking if workers and streams exist in the workflow
            * for connection plans).
            */
            if (instruction.rackNumber < ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE)
            {
                /*
                * If the rack number is valid, then we access the corresponding
                * register and remove the parameter from it. Note that we first need
                * to retrieve the corresponding workflow items, in order to send
                * them to the Master for later deletion.
                */

                /*
                * We look for the workflow items corresponding to the parameter in
                * the register:
                */
                ParameterRegister::Entry entry = m_parameterRegisters[instruction.rackNumber].find(instruction.parameterIdentifier);

                /*
                * And if we found them, then we send them into the method's second
                * argument for later deletion, and we finally remove them from the
                * register.
                */
                if (entry.generator && entry.stream)
                {
                    // TO DO: send the generator and stream to a garbage collector

                    m_parameterRegisters[instruction.rackNumber].remove(instruction.parameterIdentifier);
                }
            }
        }

        for (auto& instruction : plan.addInstructions)
        {
            /*
            * We check if the instruction is valid regarding its rack number and
            * workflow items (this is the equivalent of checking if workers and
            * streams exist in the workflow for connection plans).
            */
            if (instruction.rackNumber < ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE && instruction.parameterGenerator && instruction.parameterStream)
            {
                /*
                * If the instruction is valid, then we add a new entry to the
                * corresponding register:
                */
                ParameterRegister::Entry entry;
                entry.generator = std::move(instruction.parameterGenerator);
                entry.stream = std::move(instruction.parameterStream);
                m_parameterRegisters[instruction.rackNumber].insert(instruction.parameterIdentifier, entry);
            }
        }
    }

    std::shared_ptr<ParameterGenerator> AudioWorkflow::findParameterGenerator(unsigned short rackNumber, StringView parameterIdentifier)
    {
        /* We retrieve the parameter register corresponding to the given rack */
        const ParameterRegister& parameterRegister = m_parameterRegisters[rackNumber];

        /* Then we look for the given parameter inside of this register */
        ParameterRegister::Entry entry = parameterRegister.find(parameterIdentifier);

        return entry.generator;
    }

    uint32_t AudioWorkflow::getMixerInputStreamID(unsigned short voiceNumber, unsigned short instrumentRackNumber, unsigned short channel) const
    {
        /*
        * We assume voiceNumber, instrumentRackNumber, and channel are in-range, and
        * we compute the corresponding input port to retrieve the stream ID from.
        */
        unsigned short inputPort = voiceNumber * ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE * ANGLECORE_NUM_CHANNELS + instrumentRackNumber * ANGLECORE_NUM_CHANNELS + channel;

        /*
        * Provided every input number is in-range, which we assumed, the inputPort
        * is guaranteed to be in-range for the Mixer's input bus. So we do not need
        * to perform any check before accessing this input bus. In addition, because
        * all the Mixer's input streams were initialized on construction, it is also
        * guaranteed that the Mixer's input bus does not contain any null pointer.
        */
        return m_mixer->getInputBus()[inputPort]->id;
    }

    uint32_t AudioWorkflow::getSampleRateStreamID() const
    {
        /* The information is located in the GlobalContext */
        return m_globalContext.sampleRateStream->id;
    }

    uint32_t AudioWorkflow::getSampleRateReciprocalStreamID() const
    {
        /* The information is located in the GlobalContext */
        return m_globalContext.sampleRateReciprocalStream->id;
    }

    uint32_t AudioWorkflow::getFrequencyStreamID(unsigned short voiceNumber) const
    {
        /* The information is located in the given voice's context. */
        return m_voices[voiceNumber].voiceContext.frequencyStream->id;
    }

    uint32_t AudioWorkflow::getFrequencyOverSampleRateStreamID(unsigned short voiceNumber) const
    {
        /* The information is located in the given voice's context. */
        return m_voices[voiceNumber].voiceContext.frequencyOverSampleRateStream->id;
    }

    uint32_t AudioWorkflow::getVelocityStreamID(unsigned short voiceNumber) const
    {
        /* The information is located in the given voice's context. */
        return m_voices[voiceNumber].voiceContext.velocityStream->id;
    }
}