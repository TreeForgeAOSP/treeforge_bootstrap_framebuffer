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

## License

TreeForge-owned userspace, host tooling, tests, documentation, and
provider metadata are licensed under Apache-2.0.

Linux kernel patch material under `kernel/` is licensed under
GPL-2.0-only. See `LICENSE` and `LICENSES/`.
