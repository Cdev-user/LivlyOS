# Improvements and Roadmap

This document outlines the planned development phases for LivlyOS. The project is actively being worked on with a clear roadmap for implementing core operating system features.

## Phase 1: Scheduler Refactoring

Currently in progress. This phase focuses on establishing a solid foundation for process management.

- Clean state enumeration: READY, RUNNING, BLOCKED, ZOMBIE
- iretq-based kernel entry (replacing manual jump instructions)
- Zombie reaper kernel thread for proper process cleanup
- STI restoration in IDT initialization
- Comprehensive documentation and comments

## Phase 2: Process Primitives

Implementing essential process management functions.

- process_sleep(ms) - Puts process into BLOCKED state with timer-based wake up
- process_yield() - Allows voluntary CPU relinquishment
- process_wait_event(event_id) / process_wakeup(event_id) - Event-based process synchronization

## Phase 3: PS/2 Mouse Support

Adding mouse input capabilities to the system.

- Mouse driver implementation (IRQ12)
- Mouse cursor rendering on framebuffer
- Global mouse state tracking (position and button states)

## Phase 4: Event System

Building an event handling infrastructure for the entire system.

- Centralized event queue for all input types
- Keyboard event distribution
- Mouse event distribution
- Timer event distribution
- Generic wait_for_event() API for applications

## Phase 5: Window Manager

Implementing a basic windowing system for graphical applications.

- Window structure and data representation
- Window list management
- Window rendering (title bar, borders, content area)
- Click detection and window identification
