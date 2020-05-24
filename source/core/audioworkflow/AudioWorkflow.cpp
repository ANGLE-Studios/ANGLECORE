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

#include "AudioWorkflow.h"

#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    AudioWorkflow::AudioWorkflow() :
        Workflow(),
        VoiceAssigner(),
        m_exporter(std::make_shared<Exporter<float>>()),
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

        /* We add the global context's workers and streams */
        addWorker(m_globalContext.sampleRateGenerator);
        addStream(m_globalContext.sampleRateStream);
        addWorker(m_globalContext.sampleRateInverter);
        addStream(m_globalContext.sampleRateReciprocalStream);

        /*
        * Then we create the connections between the context's items. Note that, by
        * construction of a GlobalContext, we know none of m_globalContext's member
        * pointers are null, so we do not check for nullptr here:
        */
        plugWorkerIntoStream(m_globalContext.sampleRateGenerator->id, 0, m_globalContext.sampleRateStream->id);
        plugStreamIntoWorker(m_globalContext.sampleRateStream->id, m_globalContext.sampleRateInverter->id, 0);
        plugWorkerIntoStream(m_globalContext.sampleRateInverter->id, 0, m_globalContext.sampleRateReciprocalStream->id);

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

    unsigned short AudioWorkflow::findEmptyRackNumber() const
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
        * If we arrive here, this means we could not find an empty spot, so we
        * return the number of racks, which is an out-of-range index to the Voices'
        * racks:
        */
        return ANGLECORE_MAX_NUM_INSTRUMENTS_PER_VOICE;
    }

    void AudioWorkflow::addInstrumentAndPlanBridging(unsigned short voiceNumber, unsigned short rackNumber, const std::shared_ptr<Instrument>& instrument, ConnectionPlan& planToComplete)
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
            planToComplete.streamToWorkerPlugInstructions.emplace_back(getSampleRateStreamID(), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::SAMPLE_RATE));
        if (configuration.receiveSampleRateReciprocal)
            planToComplete.streamToWorkerPlugInstructions.emplace_back(getSampleRateReciprocalStreamID(), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::SAMPLE_RATE_RECIPROCAL));
        if (configuration.receiveFrequency)
            planToComplete.streamToWorkerPlugInstructions.emplace_back(getFrequencyStreamID(voiceNumber), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::FREQUENCY));
        if (configuration.receiveFrequencyOverSampleRate)
            planToComplete.streamToWorkerPlugInstructions.emplace_back(getFrequencyOverSampleRateStreamID(voiceNumber), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::FREQUENCY_OVER_SAMPLE_RATE));
        if (configuration.receiveVelocity)
            planToComplete.streamToWorkerPlugInstructions.emplace_back(getVelocityStreamID(voiceNumber), instrument->id, instrument->getInputPortNumber(Instrument::ContextParameter::VELOCITY));

        /*
        * After handling the interaction between the instrument and the workflow's
        * contexts, now is the turn of the instrument's specific parameters to be
        * configured. We loop through each parameter to create its specific, short
        * rendering pipeline.
        */
        for (const Parameter& parameter : instrument->getParameters())
        {
            /*
            * For each parameter, we create a ParameterGenerator, that we assign to
            * the instrument's voice:
            */
            std::shared_ptr<ParameterGenerator> generator = std::make_shared<ParameterGenerator>(parameter);
            addWorker(generator);
            assignVoiceToWorker(voiceNumber, generator->id);

            /*
            * Then we create a stream that will contain the output of the generator,
            * i.e. the parameter's values:
            */
            std::shared_ptr<Stream> parameterStream = std::make_shared<Stream>();
            addStream(parameterStream);

            /*
            * And finally, we connect those elements together with the instrument to
            * construct a short rendering pipeline, which feeds in the instrument.
            * Note that we make these connections directly here rather than planning
            * them, as we know the parameter generator involved will only be called
            * when the instrument is properly connected to the real-time rendering
            * pipeline. Therefore, although the generator can almost already receive
            * parameter change requests, it will not interfere with the current
            * rendering pipeline.
            */
            plugWorkerIntoStream(generator->id, 0, parameterStream->id);
            plugStreamIntoWorker(parameterStream->id, instrument->id, instrument->getInputPortNumber(parameter.identifier));
        }

        /*
        * The plan is then completed for connecting the Instrument's output bus into
        * the Mixer.
        */
        for (unsigned short c = 0; c < ANGLECORE_NUM_CHANNELS; c++)
            planToComplete.workerToStreamPlugInstructions.emplace_back(getMixerInputStreamID(voiceNumber, rackNumber, c), instrument->id, c);
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