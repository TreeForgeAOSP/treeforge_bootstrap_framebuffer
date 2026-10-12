# TreeForge Bootstrap Framebuffer

TreeForge Bootstrap framebuffer/graphics provider for Pixel Tablet (`tangorpro`, GS201).

Repository owner: `TreeForgeDEV/treeforge_bootstrap_framebuffer`.

The initial `v1.0b1` extraction is intentionally behavior-preserving. It moves the already hardware-accepted framebuffer ownership boundary out of the Bootstrap and Kernel repositories without redesigning the display path.

## Owns

- the GS201 early framebuffer kernel bridge patches;
- framebuffer ABI v1;
- the freestanding Bootstrap userspace consumer primitives;
- the freestanding Bootstrap framebuffer transport/lifecycle fragment.

The kernel bridge discovers and validates the already-scanning bootloader framebuffer. It does not program DECON, DPP, RDMA, DSIM, the panel, or DRM/KMS.

## Does not own

- Boot Manager page/layout policy;
- Android or Alternate-OS handoff policy;
- Pixel Partitioner authorization;
- Bootstrap transaction policy;
- Owner AVB signing.

Those remain with their existing owners.

## Initial extraction contract

The two userspace files are textual C fragments. This is deliberate for the first extraction: the accepted Bootstrap dispatcher is one freestanding translation unit and the fragments currently depend on its syscall/string/logging helpers. Keeping textual inclusion preserves runtime behavior while moving source ownership. A later ABI cleanup can make the renderer a separately linked object after hardware parity is proven.

Framebuffer ABI remains `1`. Component version remains `v1.0b1`, matching the Bootstrap v1.0b7 runtime inventory.

## Verify

```bash
./framebufferctl verify
```

## Package

```bash
./framebufferctl package
```

This creates a deterministic release archive under `out/` and writes its SHA-256 next to it.
