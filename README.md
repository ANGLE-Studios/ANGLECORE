# ANGLECORE

A free and open-source software development kit for developping real-time audio engines.

## What is ANGLECORE?

ANGLECORE is an audio framework that enables the modification of an audio plugin’s internal audio graph at run-time. It is primarily meant for audio programmers who develop instrument-based plugins, where the end user can dynamically create or remove multiple instrument layers to compose sound textures.

With ANGLECORE, you can let end users customize your plugin’s internal audio graph in real-time and in a thread-safe manner, for example to add instruments, effects, or modulators, or to change the internal routing of the audio signal. ANGLECORE also provides easy access to multiple features of audio programming, like automatic parameter smoothing, which can be used to facilitate the implementation of any plugin.

ANGLECORE is entirely written in C++, and is platform and audio framework-independent (it can be used with JUCE or with any other open-source audio framework without any change). It is light and uses minimal dependencies. The only external dependency is [farbot](https://github.com/hogliux/farbot), by Fabian Renn.

## I want to use ANGLECORE!

Amazing! 😊

If you are an audio programmer interested in using ANGLECORE, please read the following paragraphs to get started.

ANGLECORE is licensed under GPLv3, which basically means it can only be used in open-source projects. Note that the license does not prevent you from using ANGLECORE in a commercial project that generates revenue. See the license file for more information.

The best way to use ANGLECORE is as a static or dynamic library. To facilitate the integration of ANGLECORE, a single header file ANGLECORE.h is provided in the include folder of the repository. Use this file as your header file when statically or dynamically linking ANGLECORE to your project. Do not forget to provide a copy of the ANGLECORE’s GPLv3 license with your plugin.

The main concepts you should know about to use ANGLECORE are those of `Master` and `Instrument`. Other concepts will be introduced in subsequent versions (`Listener`, `Effect`, `Modulator`…).

#### Master

A `Master` is an entity that serves as a single interface for developers who use ANGLECORE in their project. It encompasses all the functionalities that developers should need to create plugins with ANGLECORE.

#### Instrument

An `Instrument` is an entity that generates audio based on its input parameters. End-users who want to create their own instruments can inherit from this class and override its `play()` method to benefit from ANGLECORE's architecture with their custom instrument.

## I want to contribute to ANGLECORE!

Excellent! 😊

Anyone is welcome to contribute. If you are a user and want to report a bug, do not hesitate to leverage GitHub’s issue feature to let me know about it.

If you want contribute to ANGLECORE as a developer, feel free to clone this repository and start working on it! You can work on bug fixes, implement new features, or improve the documentation. The following paragraphs will provide a good starting point for you as a developer to understand the basic structure of the framework.

The main concepts you should know about to contribute to ANGLECORE as a developer are those of `AudioWorkflow`, `Renderer`, and `Master`, which are all implemented in the source/core directory. There are many other concepts that comprise ANGLECORE, but understanding these basic ones first should help you get started.

#### AudioWorkflow

The `AudioWorkflow` class is derived from the `Workflow` class, which is presented hereafter.

A `Workflow` is an organized set of workers that read from and write into data streams. It can be represented as a directed graph, with each worker performing one task using data from its input streams and passing the corresponding results throughout its output streams.

An `AudioWorkflow` is a `Workflow` that is specially structured to generate and render audio using multiple instruments. It represents the graph that the audio signal follows from the audio generators to the final mix.

The typical example of a worker in an `AudioWorkflow` is an instrument that generates audio from its input parameters. Other types of workers, such as parameter generators that generate parameter values for instruments or effects to use, also compose a large portion of most `AudioWorkflow`s.

An interesting feature of an `AudioWorkflow` is its capacity to automatically compute its rendering sequence, which is the list of workers that should be called in order by the `Renderer` to generate a block of audio. This feature greatly facilitates the task of the `Renderer`, which just has to call all of the workers present in that sequence one after the other to perform a rendering session, with the guarantee that the order in which they are presented is consistent with the `AudioWorkflow`’s internal connections. 

#### Renderer

The Renderer is responsible for effectively generating the audio, calling all the workers of an `AudioWorkflow` in order.

#### Master

The `Master` is the global entity that is responsible for operating both audio and request processing pipelines. It is certainly the most central class to ANGLECORE, since it interacts with all the other classes to perform its tasks.

Audio developers who use ANGLECORE usually create only one instance of this class for their plugin, as it is meant to serve as a single interface class for them. They can then safely call the methods of their single `Master` object from both the real-time thread and non real-time threads to manipulate their plugin.
