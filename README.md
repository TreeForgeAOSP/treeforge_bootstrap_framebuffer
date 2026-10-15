# TreeForge Bootstrap Framebuffer — v1.0b1

Framebuffer provider for TreeForge Bootstrap on Pixel Tablet (`tangorpro`).

## Build

    ./framebufferctl build

Successful builds publish `out/development-provider/current` and retain the newest five successful runs.

## Verify

    ./framebufferctl verify

## Release

    ./release prepare

Release packages the exact current development provider into `out/releases/<version>/assets`. It does not rebuild it.

`VERSION` is authoritative.
