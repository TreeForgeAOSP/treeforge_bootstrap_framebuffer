# TreeForge Bootstrap Framebuffer — v1.0b2

Framebuffer provider for TreeForge Bootstrap on Pixel Tablet (`tangorpro`).

## Owns

- GS201 early framebuffer bridge patches
- framebuffer ABI 1
- Bootstrap framebuffer consumer and presentation fragments

The renderer uses a private RAM surface and presents completed dirty regions to the live scanout framebuffer.

## Verify

```bash
./framebufferctl verify
```

## Package

```bash
./framebufferctl package
```

`VERSION` is the authoritative component version. Provider metadata, release names, and checksums are generated from it.
