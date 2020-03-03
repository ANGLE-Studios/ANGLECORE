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

#include <mutex>
#include <memory>
#include <vector>
#include <atomic>
#include <unordered_map>

#include "../../utility/StringView.h"
#include "../audio/AudioChunk.h"

namespace ANGLECORE
{
    /** 
    * \struct InverseUnion Instrument.h
    * Provides a simple solution to perform atomic operations on a number and its
    * inverse.
    */
    template<typename T>
    struct InverseUnion
    {
        T value;    /**< Double or float value of a real number*/
        T inverse;  /**< Double or float value of the inverse of value*/
    };
    
    /**
    * \class BaseInstrument Instrument.h
    * This abstract class can be used to create audio instruments that manage internal
    * parameters automatically. The Instrument class also provides that functionnality,
    * with the additional benefit of also providing default parameters.
    */
    class BaseInstrument
    {
    protected:

        /**
        * \struct Parameter Instrument.h
        * Represents a parameter that an instrument can modify and use for rendering.
        */
        struct Parameter
        {
            /**
            * \struct TransientTracker Instrument.h
            * A TransientTracker tracks a Parameter's position while in a transient
            * state. This structure can store how close the Parameter is to its
            * target value (in terms of remaining samples), as well as the increment
            * to use for its update.
            * Additionally, this structure contains a mutex for trade-safe operations.
            */
            struct TransientTracker
            {
                std::mutex lock;
                double targetValue;
                uint32_t transientDurationInSamples;
                uint32_t position;
                double increment;
            };

            /**
            * \struct ChangeRequest Instrument.h
            * When the end-user asks to change a parameter from an instrument (volume,
            * etc.), an instance of this structure is created to store all necessary
            * information about that ChangeRequest.
            */
            struct ChangeRequest
            {
                double targetValue;
                bool smoothChange;
                uint32_t durationInSamples;

                /**
                * Creates a null ChangeRequest, which instructs to instantly set a
                * Parameter to 0 in a duration of 0 samples.
                */
                ChangeRequest() :
                    targetValue(0),
                    smoothChange(false),
                    durationInSamples(0)
                {}

                /**
                * Initializes all the members of a ChangeRequest.
                * @param[in] newValue   Target value for the parameter required to
                *   change. This parameter will simply be copied into targetValue.
                * @param[in] shouldBeSmooth Indicates if the parameter should change
                *   instantaneously or go through a transient phase. This parameter
                *   will simply be copied into smoothChange.
                * @param[in] numSamples Number of samples the transient phase should
                *   last. This parameter will simply be copied into
                *   durationInSamples, but it will only be used if \p shouldBeSmooth
                *   has been set to \p true.
                */
                ChangeRequest(double newValue, bool shouldBeSmooth, uint32_t numSamples) :
                    targetValue(newValue),
                    smoothChange(shouldBeSmooth),
                    durationInSamples(numSamples)
                {}
            };

            /**
            * \struct ChangeRequestDeposit Instrument.h
            * This structure represents a thread-safe mailbox. Every time the
            * end-user requests a parameter change, a new ChangeRequest is posted
            * here.
            * Additionally, this structure contains a mutex for trade-safe operations.
            */
            struct ChangeRequestDeposit
            {
                std::mutex lock;
                bool newChangeRequestReceived;
                ChangeRequest data;

                /**
                * Initializes all the const members of the internal ChangeRequest.
                */
                ChangeRequestDeposit() :
                    data(0, false, 0)
                {}
            };

            /**
            * \enum State
            * Represents the internal state of a parameter. The ParameterRenderer
            * will adapt to this state for its rendering.
            */
            enum State
            {
                INITIAL = 0,    /**< Parameter state right after its creation */
                STEADY,         /**< The parameter has a constant value */
                TRANSIENT,      /**< The parameter is currently changing its value */
                NUM_STATES      /**< Counts the number of possible states */
            };

            /**
            * \enum SmoothingMethod
            * Method used to render a smooth parameter change. If set to ADDITIVE,
            * then a parameter change will result in an arithmetic sequence from an
            * initial value to a target value. If set to MULTIPLICATIVE, then the
            * sequence will be geometric and use a multiplicative increment instead.
            */
            enum SmoothingMethod
            {
                ADDITIVE = 0,   /**< Generates an arithmetic sequence */
                MULTIPLICATIVE, /**< Generates a geometric sequence */
                NUM_METHODS     /**< Counts the number of available methods */
            };

            const bool minimalSmoothingEnabled;
            const SmoothingMethod smoothingMethod;
            std::atomic<State> state;
            std::atomic<double> internalValue;
            std::unique_ptr<std::vector<double>> transientCurve;
            ChangeRequestDeposit changeRequestDeposit;
            TransientTracker transientTracker;

            /**
            * Creates a parameter with a fixed smoothing technique.
            * @param[in] initialValue   Value to initialize the parameter's
            *   internalValue.
            * @param[in] minimalSmoothing   Indicates whether the parameter should
            *   enter a transient phase when requested to change, even if the
            *   corresponding ChangeRequest is meant to be instantaneous.
            * @param[in] smoothingMethod    Method to use for computing the
            *   evolution of a Parameter in a transient phase.
            */
            Parameter(double initialValue, bool minimalSmoothing, SmoothingMethod smoothingMethod);

            /**
            * Allocates memory for storing the parameter transient curves.
            * This method should be called only once after creating a parameter, as
            * no memory allocation should be performed during real-time rendering.
            * @param[in]    maxSamplesPerBlock  Upper bound of the number of samples
            *   expected per block to render. This limit is used to preallocate
            *   a sufficient amount of memory in advance for efficient rendering.
            */
            void initialize(uint32_t maxSamplesPerBlock);
        };

        /**
        * \class ParameterRenderer Instrument.h
        * A ParameterRenderer is an object that prerenders the values of all the
        * parameters of an instrument to prepare the latter for its audioCallback.
        * A ParameterRenderer typically computes any smooth parameter change or
        * modulation in advance, so that the instrument can subsequently do its
        * rendering faster.
        */
        class ParameterRenderer
        {
        public:

            /**
            * Creates a parameter renderer dedicated to a set of parameters, usually
            * those of a single BaseInstrument.
            * Before the renderer receives any sample rate, m_minSmoothingSamples is
            * set to 0.
            * @param[in] parameters&    Reference to the parameter set to be rendered
            *   through this renderer.
            * @param[in] minimalSmoothingDuration   Minimal duration of all transient
            *   phases when using this renderer.
            */
            ParameterRenderer(std::unordered_map<StringView, std::shared_ptr<Parameter>>& parameters, double minimalSmoothingDuration) :
                m_parameters(parameters),
                m_minSmoothingDuration(minimalSmoothingDuration),
                m_minSmoothingSamples(0)
            {}

            /**
            * Sends a new sample rate to the ParameterRenderer.
            * The new \p sampleRate is then used to update the number of samples
            * needed to render a minimal smoothing transient.
            * @param[in] sampleRate the new sample rate to use for rendering
            */
            void setSampleRate(double sampleRate);

            /**
            * Sends a new upper bound on the rendering block size to the
            * ParameterRenderer. This can lead the renderer to reallocate more
            * memory for all parameters, in preparation of their future rendering.
            * @param[in] maxSamplesPerBlock New maximum block size to use for
            *   rendering.
            */
            void setMaxSamplesPerBlock(uint32_t maxSamplesPerBlock);

            /**
            * Instructs the renderer to render all of the parameters it is attached
            * to.
            * This method is called at the beginning of an BaseInstrument
            * audioCallback function, in order to precompute the values of all
            * parameters for the next audio block to render. Depending on each
            * parameter's state, the renderer will trigger different computations,
            * and process every ChangeRequest is has received.
            * @param[in] blockSize  Size of the audio block to render. This
            *   corresponds to the number of samples the ParameterRenderer has to
            *   prepare for all of its parameters.
            */
            void renderParametersForNextAudioChunk(uint32_t chunkSize);

            /**
            * Updates the state of every parameter after having rendered an audio
            * block of size \p blockSize.
            * This method is called at the end of an BaseInstrument
            * audioCallback function, to update the parameters in a transient state.
            * More precisely, this method increments the TransientTracker of all
            * parameters in transient state, and updates the internalValue of those
            * to their latest value.
            * @param[in] blockSize  Size of the audio block that has just been
            *   rendered.
            */
            void updateParametersAfterRendering(uint32_t blockSize);

        private:

            /**
            * Reference to a set of parameters owned by an instrument.
            */
            std::unordered_map<StringView, std::shared_ptr<Parameter>>& m_parameters;

            /**
            * Minimal smoothing duration to apply when rendering, in seconds.
            */
            const double m_minSmoothingDuration;

            /**
            * Number of samples corresponding to m_minSmoothingDuration. The
            * conversion is done using the sample rate.
            */
            std::atomic<uint32_t> m_minSmoothingSamples;
        };

        /**
        * \enum State
        * Represents the internal state of an audio instrument. The audioCallback
        * method of an instrument will adapt to this state when called for rendering.
        */
        enum State
        {
            INITIAL = 0,    /**< Corresponds to the instrument's state right after
                            its creation */
            READY_TO_PLAY,  /**< Corresponds to the instrument's state after a sample
                            rate and a number of maximum samples per block have been
                            defined */
            NUM_STATES      /**< Counts the number of states */
        };

    public:

        /**
        * Creates an empty instrument, with no parameters inside.
        */
        BaseInstrument();

        /**
        * Initializes the instrument, providing it with a sample rate and number of
        * maximum samples per block. Calling this method will push the instrument
        * into the READY_TO_PLAY state.
        * @param[in] sampleRate Sample rate to use for rendering
        * @param[in] maxSamplesPerBlock Maximum block size to use for rendering
        */
        void initialize(double sampleRate, uint32_t maxSamplesPerBlock);

        /**
        * Changes the instrument's sample rate. Note that this will not affect the
        * instrument's state. Use the initialize function to trigger a state
        * update instead to do so.
        * @param[in] sampleRate New sample rate to use for rendering
        */
        void setSampleRate(double sampleRate);

        /**
        * Sends a new upper bound on the rendering block size to the 
        * BaseInstrument. This usually triggers memory allocation, to prepare
        * for rendering future audio blocks of size \p maxSamplesPerBlock.
        * @param[in] maxSamplesPerBlock New maximum block size to use for
        *   rendering.
        */
        void setMaxSamplesPerBlock(uint32_t maxSamplesPerBlock);

        /**
        * Creates and posts a ChangeRequest for the corresponding Parameter. The
        * ChangeRequest will be used by the ParameterRenderer to compute the next
        * values of the Parameter.
        * Note that a sequence of multiple calls to this method will result in only
        * the most recent request to be proceeded for the next audio block.
        * @param[in] ID Identifier of the Parameter
        * @param[in] newValue   Target value requested for the Parameter
        * @param[in] changeShouldBeSmooth   Indicates if the parameter should change
        *   instantaneously or go through a transient phase.
        * @param[in] durationInSamples  Number of samples the transient phase should
        *   last.
        */
        void requestParameterChange(const char* ID, double newValue, bool changeShouldBeSmooth, uint32_t durationInSamples);

        /**
        * Requests the instrument to render the next audio chunk.
        * @param[in] audioChunkToFill   The AudioChunk to render to.
        */
        void audioCallback(const AudioChunk<double>& audioChunkToFill);

    protected:

        /**
        * Returns the instrument's current sample rate. This should only be used
        * once per render block, because it requires a copy of the sampleRate
        * InverseUnion.
        */
        double sampleRate() const;

        /**
        * Returns the inverse of the instrument's current sample rate. This should
        * only be used once per render block, because it requires a copy of the
        * sampleRate InverseUnion.
        */
        double inverseSampleRate() const;

        /**
        * Creates and adds a parameter to the instrument. Note that adding a
        * parameter which ID is already used by another parameter will replace the
        * old parameter by the new one.
        * This method should only be called in the instrument's constructor, as the
        * set of parameters (m_parameters) is not protected for thread-safety.
        * @param[in] ID Identifier of the Parameter to create
        * @param[in] initialValue   Value to initialize the Parameter's
        *   internalValue.
        * @param[in] minimalSmoothing   Indicates whether the parameter should
        *   enter a transient phase when requested to change, even if the
        *   change is required to be instantaneous.
        * @param[in] smoothingMethod    Method to use for computing the evolution of
        * a Parameter in a transient phase.
        */
        void addParameter(const char* ID, double initialValue, bool minimalSmoothing, Parameter::SmoothingMethod smoothingMethod);

        /**
        * Retrieve the value of a Parameter at a certain \p index in the current
        * audio block being rendered. The method should be inline, as it may be
        * called a lot of times during rendering. However it is currently not,
        * since it would prevent users of the library from accessing it.
        * @param[in] ID Identifier of the Parameter to retrieve the value from
        * @param[in] index  Positing in the current audio block
        */
        double parameter(const char* ID, uint32_t index) const;

        /**
        * Retrieve a const Parameter for rendering. This method should be preferred
        * to the parameter(ID, index) method which tests the parameter's state on
        * each call. The method should be inline, as it may be called a lot of times
        * during rendering. However it is currently not, since it would prevent
        * users of the library from accessing it.
        * @param[in] ID Identifier of the Parameter to retrieve the value from
        */
        const std::shared_ptr<const Parameter> parameter(const char* ID) const;

        /**
        * Rendering method of an instrument, which is called once all the parameters
        * have been rendered.
        */
        virtual void renderNextAudioChunk(const AudioChunk<double>& audioChunkToFill) = 0;

    private:
        std::atomic<State> m_state;
        std::atomic<InverseUnion<double>> m_sampleRate;
        std::unordered_map<StringView, std::shared_ptr<Parameter>> m_parameters;
        ParameterRenderer m_parameterRenderer;
    };

    /**
    * \class Instrument Instrument.h
    * This abstract class can be used to create audio instruments.
    * Just like the BaseInstrument class, it manages internal
    * parameters automatically, and is capable of smoothing any
    * parameter change. However, it features default parameters
    * that can be monitored through dedicated methods. These
    * parameters are: frequency to play, velocity, and gain.
    */
    class Instrument : public BaseInstrument
    {
    public:

        /**
        * Creates a BaseInstrument, and populates it with three parameters:
        * a frequency to play for the next audio block, a velocity, and a gain.
        */
        Instrument();

        /**
        * Requests the Instrument to play a new frequency.
        * Note that a sequence of multiple calls to this method will result in only
        * the most recent request to be proceeded for the next audio block.
        * @param[in] frequency   New frequency to use for rendering
        * @param[in] changeShouldBeSmooth   Indicates if the frequency should change
        *   instantaneously or go through a transient phase
        * @param[in] durationInSamples  Number of samples the transient phase should
        *   last
        */
        void requestNewFrequencyToPlay(double frequency, bool changeShouldBeSmooth, uint32_t durationInSamples);

        /**
        * Requests the Instrument to use a new velocity to play.
        * Note that a sequence of multiple calls to this method will result in only
        * the most recent request to be proceeded for the next audio block.
        * @param[in] velocity   New velocity to use for rendering
        * @param[in] changeShouldBeSmooth   Indicates if the velocity should change
        *   instantaneously or go through a transient phase
        * @param[in] durationInSamples  Number of samples the transient phase should
        *   last
        */
        void requestNewVelocity(double velocity, bool changeShouldBeSmooth, uint32_t durationInSamples);

        /**
        * Requests the Instrument to change the gain.
        * Note that a sequence of multiple calls to this method will result in only
        * the most recent request to be proceeded for the next audio block.
        * @param[in] gain   New gain to apply where necessary during rendering
        * @param[in] changeShouldBeSmooth   Indicates if the gain should change
        *   instantaneously or go through a transient phase
        * @param[in] durationInSamples  Number of samples the transient phase should
        *   last
        */
        void requestNewGain(double gain, bool changeShouldBeSmooth, uint32_t durationInSamples);

    protected:

        /**
        * Retrieve the frequency to play at a certain \p index in the current audio
        * block being rendered.
        * @param[in] index  Positing in the current audio block
        */
        double frequencyToPlay(uint32_t index);

        /**
        * Retrieve the velocity for a certain \p index in the current audio block
        * being rendered.
        * @param[in] index  Positing in the current audio block
        */
        double velocity(uint32_t index);

        /**
        * Retrieve the gain to use for a certain \p index in the current audio block
        * being rendered.
        * @param[in] index  Positing in the current audio block
        */
        double gain(uint32_t index);

    private:
        static const char* PARAMETER_ID_FREQUENCY_TO_PLAY;
        static const char* PARAMETER_ID_VELOCITY;
        static const char* PARAMETER_ID_GAIN;
    };
}