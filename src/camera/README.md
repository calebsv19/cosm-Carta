# camera Module

This module handles map camera movement and coordinate conversion.
It provides the transforms that make map meters visible as screen pixels.

## Files

- `camera.c`: Camera init, input-driven pan/zoom updates, smoothing, and world/screen conversion utilities.

## How It Connects

- Consumed by app, map rendering, and route rendering code.
- Ensures all subsystems use the same world-space interpretation and zoom scale.

