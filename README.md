# TreeForge Bootstrap Framebuffer — v1.0b2

Framebuffer provider for TreeForge Bootstrap on Pixel Tablet (`tangorpro`).

## Build

    ./framebufferctl build

A successful build verifies the provider and promotes it to `out/development-provider/current`. The newest five successful runs are retained.

## Verify

    ./framebufferctl verify

## Package

    ./framebufferctl package

Package archives the exact current development provider into `.release/`. It does not rebuild it.

`VERSION` is authoritative.
