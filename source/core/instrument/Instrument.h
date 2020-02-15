/*********************************************************************
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
*********************************************************************/

#pragma once

#include <atomic>

namespace ANGLECORE
{
    /** 
    * \struct InverseUnion
    * Provides a simple solution to perform atomic operations on a
    * number and its inverse.
    */
    template<typename T>
    struct InverseUnion
    {

    };
    
    /**
    * \class AbstractInstrument Instrument.h
    * This abstract class can be used to create audio instruments that
    * manage internal parameters automatically. The Instrument class
    * also provides that functionnality, with the additional benefit
    * of also providing default parameters.
    */
    class AbstractInstrument
    {
        /**
        * \class Parameter Instrument.h
        * Represents a parameter that an instrument can modify and
        * use for rendering.
        */
        struct Parameter
        {
            /**
            * \struct TransientTimer Instrument.h
            * When a parameter is in a transient state and changes towards
            * a new value, this structure can store how close the parameter
            * is to its target value (in terms of remaining samples). Also,
            * this structure contains a mutex for trade-safe operations.
            */
            struct TransientTimer
            {

            };

            /**
            * \struct ChangeRequest Instrument.h
            * When the end-user asks to change a parameter from an instrument
            * (volume, etc.), an instance of this structure is created to
            * store all necessary information about that ChangeRequest. Also,
            * this structure contains a mutex for trade-safe operations.
            */
            struct ChangeRequest
            {

            };

            /**
            * \struct ChangeRequestDeposit Instrument.h
            * This structure is meant to be used as a thread-safe mailbox. Each
            * time a parameter is requested to change by the end-user, a new
            * ChangeRequest is posted here.
            */
            struct ChangeRequestDeposit
            {

            };
        };

        /**
        * \class ParameterRenderer Instrument.h
        * A ParameterRenderer is an object that prerenders the values of
        * all the parameters of an instrument to prepare the latterfor its
        * audioCallback. A ParameterRenderer typically computes any smooth
        * parameter change or modulation in advance, so that the instrument
        * can subsequently do its rendering in a row.
        */
        class ParameterRenderer
        {

        };

    };

    /**
    * \class Instrument Instrument.h
    * This abstract class can be used to create audio instruments.
    * Just like the AbstractInstrument class, it manages internal
    * parameters automatically, and is capable of smoothing any
    * parameter change. However, it features default parameters
    * that can be monitored through dedicated methods. These
    * parameters are: frequency to play, velocity, and gain.
    */
    class Instrument : public AbstractInstrument
    {

    };
}