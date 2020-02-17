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

#include "Instrument.h"

#include "../../config/AudioConfig.h"

namespace ANGLECORE
{
    /*  Parameter
    ************************************/

    BaseInstrument::Parameter::Parameter(double initialValue, bool minimalSmoothing, SmoothingMethod smoothingMethod) :
        state(INITIAL),
        minimalSmoothingEnabled(minimalSmoothing),
        smoothingMethod(smoothingMethod),
        internalValue(initialValue)
    {
        /* Initialize the transient curve pointer */
        transientCurve = std::make_unique<std::vector<double>>();

        /* Initialize the changeRequestDeposit */
        changeRequestDeposit.newChangeRequestReceived = false;
    }

    void BaseInstrument::Parameter::initialize(uint32_t maxSamplesPerBlock)
    {
        /* By construction, transientCurve is never a null pointer */
        transientCurve->reserve(maxSamplesPerBlock);

        /* Once the memory allocated, the parameter is ready for rendering */
        state.store(STEADY);
    }

    /*  ParameterRenderer
    ************************************/

    void BaseInstrument::ParameterRenderer::setSampleRate(double sampleRate)
    {
        m_minSmoothingSamples.store(static_cast<uint32_t>(m_minSmoothingDuration * sampleRate));
        /* Note that we should actually add an extra sample when the result of the
        * multiplication is not an integer (which should be rare) */
    }

    void BaseInstrument::ParameterRenderer::setMaxSamplesPerBlock(uint32_t maxSamplesPerBlock)
    {
        /*
        * For each parameter, we reserve the necessary capacity in the transient
        * curve. Note that internal pointers to Parameter cannot be null by
        * construction, so there is no need to test for nullptr.
        */
        for (auto& it : m_parameters)
            it.second->initialize(maxSamplesPerBlock);
    }

    void BaseInstrument::ParameterRenderer::renderParametersForNextAudioBlock(uint32_t blockSize)
    {
        /*
        * First step is to update all parameters. This is done once, and only at the
        * beginning of the block rendering. It is not done at the end, just to be
        * sure we perform updates only if necessary, that is only if we are asked to
        * render an audio block
        */
        for (auto& it : m_parameters)
        {
            // TO DO
        }
    }

    void BaseInstrument::ParameterRenderer::updateParametersAfterRendering(uint32_t blockSize)
    {
        // TO DO
    }

    /*  BaseInstrument
    ************************************/

    BaseInstrument::BaseInstrument() :
        m_parameterRenderer(m_parameters, ANGLECORE_INSTRUMENT_MINIMUM_SMOOTHING_DURATION)
    {
        // TO DO
    }

    void BaseInstrument::initialize(double sampleRate, uint32_t maxSamplesPerBlock)
    {
        // TO DO
    }

    void BaseInstrument::setSampleRate(double sampleRate)
    {
        // TO DO
    }

    void BaseInstrument::setMaxSamplesPerBlock(uint32_t maxSamplesPerBlock)
    {
        // TO DO
    }

    void BaseInstrument::requestParameterChange(const char* ID, double newValue, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    double BaseInstrument::sampleRate() const
    {
        // TO DO
    }

    double BaseInstrument::inverseSampleRate() const
    {
        // TO DO
    }

    void BaseInstrument::addParameter(const char* ID, double initialValue, bool minimalSmoothing, Parameter::SmoothingMethod smoothingMethod)
    {
        // TO DO
    }

    double BaseInstrument::parameter(const char* ID, uint32_t index) const
    {
        // TO DO
    }

    /*  Instrument
    ************************************/

    const char* Instrument::PARAMETER_ID_FREQUENCY_TO_PLAY = "ANGLECORE_PARAMETER_FREQUENCY_TO_PLAY";
    const char* Instrument::PARAMETER_ID_VELOCITY = "ANGLECORE_PARAMETER_VELOCITY";
    const char* Instrument::PARAMETER_ID_GAIN = "ANGLECORE_PARAMETER_GAIN";

    Instrument::Instrument()
    {
        // TO DO
    }

    void Instrument::requestNewFrequencyToPlay(double frequency, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    void Instrument::requestNewVelocity(double velocity, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    void Instrument::requestNewGain(double gain, bool changeShouldBeSmooth, uint32_t durationInSamples)
    {
        // TO DO
    }

    double Instrument::frequencyToPlay(uint32_t index)
    {
        // TO DO
    }

    double Instrument::velocity(uint32_t index)
    {
        // TO DO
    }

    double Instrument::gain(uint32_t index)
    {
        // TO DO
    }
}