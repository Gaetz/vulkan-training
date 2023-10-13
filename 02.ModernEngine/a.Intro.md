# Lesson 1 : Introduction to modern rendering with Vulkan

The goal of this course is to study modern rendering techniques, that involve both graphics and engine code. As a base, it uses a existing engine, called raptor engine. You will go from there and add features in the engine, learning more each step of the lesson.

## Project

You will find the starter engine in the 02b.ModernEngine_Starter folder.

## Analysis of the starter engine

The starter engine is able to create a 3d scene with a loaded mesh. It uses deterministic memory allocation and various tools and abstractions to do so. Your first job as a team is to analyse and document the engine, then report this to your coworkers.

Separate the team in 4 groups:
- Group 1 will analyse and expose a report about memory allocation, data structures and . They will analyse memory.hpp, memory_utils.hpp, bit.hpp, array.hpp, color.hpp, data_structures.hpp, hash_map.hpp, numerics.hpp, string.hpp, and there cpp counterparts in the Foundation folder.
- Group 2 will analyse resource_manager.hpp, serialization.hpp, gltf.hpp, file.hpp, blob.hpp and blob_serialization.hpp, service.hpp, service_manager.hpp, process.hpp, log.hpp, assert.hpp and their cpp counterparts in the Foundation folder.
- Group 3 will analyse and expose the structure of the renderer in the Graphics folder.
- Group 4 will  analyse and expose the structure of the application from the Application folder and the main.cpp (inclusing shader code) from the Graphics folder.

Each analysis should at least contains a UML class diagram and a sequence diagram to explain initialization or any other feature.

You can use the `a.InformationDocument.pdf` to get an overview of the engine. You are expected to go into code details.

## Report

The report should be done as a 10 minutes speech to the class, with documents.