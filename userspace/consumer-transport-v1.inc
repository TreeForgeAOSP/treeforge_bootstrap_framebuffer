static int tfb_fb_parse_dev_number(
    const char *buffer,
    usize length,
    u32 *major_out,
    u32 *minor_out
) {
    if (
        !buffer
        || !major_out
        || !minor_out
        || length == 0
    ) {
        return 0;
    }

    usize index = 0;
    u32 major_value = 0;
    u32 minor_value = 0;
    int major_digits = 0;
    int minor_digits = 0;

    while (
        index < length
        && buffer[index] >= '0'
        && buffer[index] <= '9'
    ) {
        u32 digit = (
            (u32) (
                buffer[index] - '0'
            )
        );

        if (major_value > 4095U) {
            return 0;
        }

        major_value = (
            major_value * 10U
            + digit
        );

        major_digits = 1;
        index++;
    }

    if (
        !major_digits
        || index >= length
        || buffer[index] != ':'
    ) {
        return 0;
    }

    index++;

    while (
        index < length
        && buffer[index] >= '0'
        && buffer[index] <= '9'
    ) {
        u32 digit = (
            (u32) (
                buffer[index] - '0'
            )
        );

        if (minor_value > 1048575U) {
            return 0;
        }

        minor_value = (
            minor_value * 10U
            + digit
        );

        minor_digits = 1;
        index++;
    }

    if (!minor_digits) {
        return 0;
    }

    if (
        major_value == 0
        || major_value > 4095U
        || minor_value > 1048575U
    ) {
        return 0;
    }

    *major_out = major_value;
    *minor_out = minor_value;

    return 1;
}


static unsigned long tfb_fb_encode_dev(
    u32 major_value,
    u32 minor_value
) {
    /*
     * Linux new_encode_dev() layout.
     *
     * The current misc minor is small, but use the complete encoding
     * rather than relying on the legacy 8:8 representation.
     */
    return (
        (unsigned long) (
            minor_value & 0xffU
        )
        | (
            (unsigned long) major_value
            << 8
        )
        | (
            (unsigned long) (
                minor_value & ~0xffU
            )
            << 12
        )
    );
}


static void tfb_fb_remove_created_node(void) {
    if (!tfb_fb_created_node) {
        return;
    }

    tfb_syscall3(
        SYS_UNLINKAT,
        AT_FDCWD,
        (long) TFB_FB_DEVICE_PATH,
        0
    );

    tfb_fb_created_node = 0;
}


static int tfb_fb_realize_node(void) {
    /*
     * If a device manager has already materialized the node, leave it
     * entirely alone.
     */
    long existing = tfb_open(
        TFB_FB_DEVICE_PATH,
        O_RDWR
    );

    if (existing >= 0) {
        tfb_close(
            existing
        );

        return 1;
    }

    long sysfs_fd = tfb_open(
        TFB_FB_SYSFS_DEV_PATH,
        O_RDONLY
        | O_NONBLOCK
    );

    if (sysfs_fd < 0) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-sysfs-missing"
        );

        return 0;
    }

    char value[32];

    long amount = tfb_syscall3(
        SYS_READ,
        sysfs_fd,
        (long) value,
        (long) sizeof(value)
    );

    tfb_close(
        sysfs_fd
    );

    if (amount <= 0) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-sysfs-read-failed"
        );

        return 0;
    }

    u32 major_value = 0;
    u32 minor_value = 0;

    if (
        !tfb_fb_parse_dev_number(
            value,
            (usize) amount,
            &major_value,
            &minor_value
        )
    ) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-dev-number-invalid"
        );

        return 0;
    }

    unsigned long encoded = (
        tfb_fb_encode_dev(
            major_value,
            minor_value
        )
    );

    long result = tfb_syscall4(
        SYS_MKNODAT,
        AT_FDCWD,
        (long) TFB_FB_DEVICE_PATH,
        S_IFCHR | 0600,
        (long) encoded
    );

    /*
     * Even if mknodat raced with another creator, opening the node is
     * the authoritative result.
     */
    long verify = tfb_open(
        TFB_FB_DEVICE_PATH,
        O_RDWR
    );

    if (verify < 0) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-node-failed"
        );

        return 0;
    }

    tfb_close(
        verify
    );

    if (result == 0) {
        tfb_fb_created_node = 1;

        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-node-created"
        );
    } else {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-node-existing"
        );
    }

    return 1;
}


static int tfb_fb_read_info(
    long fd,
    struct tfb_fb_info *info
) {
    if (
        fd < 0
        || !info
    ) {
        return 0;
    }

    unsigned char *cursor = (
        (unsigned char *) info
    );

    usize remaining = (
        sizeof(*info)
    );

    while (remaining != 0) {
        long amount = tfb_syscall3(
            SYS_READ,
            fd,
            (long) cursor,
            (long) remaining
        );

        if (amount <= 0) {
            return 0;
        }

        cursor += amount;
        remaining -= (usize) amount;
    }

    return 1;
}


static int tfb_fb_validate_info(
    const struct tfb_fb_info *info
) {
    if (!info) {
        return 0;
    }

    if (
        info->abi_version
        != TFB_FB_ABI_VERSION
    ) {
        return 0;
    }

    if (
        info->bytes_per_pixel != 4U
        || info->hw_format > 7U
        || info->dpp_index >= 16U
    ) {
        return 0;
    }

    if (
        info->flags
        != TFB_FB_REQUIRED_FLAGS
    ) {
        return 0;
    }

    if (
        info->width == 0
        || info->height == 0
        || info->width > 8192U
        || info->height > 8192U
    ) {
        return 0;
    }

    /*
     * V1 userspace intentionally accepts only a full-surface scanout.
     *
     * The kernel ABI can describe a sub-rectangle, but the existing
     * renderer assumes origin 0,0. Refuse rather than guess.
     */
    if (
        info->source_x != 0U
        || info->source_y != 0U
        || info->source_width != info->width
        || info->source_height != info->height
    ) {
        return 0;
    }

    if (
        info->stride
        < info->width
        * info->bytes_per_pixel
    ) {
        return 0;
    }

    u64 expected_bytes = (
        (u64) info->stride
        * (u64) info->source_height
    );

    if (
        info->framebuffer_bytes
        != expected_bytes
    ) {
        return 0;
    }

    if (
        info->mmap_bytes
        < info->framebuffer_bytes
        || info->mmap_bytes == 0
        || info->mmap_bytes > 67108864ULL
        || (
            info->mmap_bytes
            & 4095ULL
        ) != 0
    ) {
        return 0;
    }

    return 1;
}


static int tfb_fb_begin(void) {
    if (tfb_fb_active) {
        return 1;
    }

    if (!tfb_fb_realize_node()) {
        return 0;
    }

    long fd = tfb_open(
        TFB_FB_DEVICE_PATH,
        O_RDWR
    );

    if (fd < 0) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-open-failed"
        );

        tfb_fb_remove_created_node();

        return 0;
    }

    struct tfb_fb_info info;

    if (
        !tfb_fb_read_info(
            fd,
            &info
        )
    ) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-info-read-failed"
        );

        tfb_close(
            fd
        );

        tfb_fb_remove_created_node();

        return 0;
    }

    if (!tfb_fb_validate_info(&info)) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-info-invalid"
        );

        tfb_close(
            fd
        );

        tfb_fb_remove_created_node();

        return 0;
    }

    long mapped_address = tfb_syscall6(
        SYS_MMAP,
        0,
        (long) info.mmap_bytes,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (mapped_address < 0) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-mmap-failed"
        );

        tfb_close(
            fd
        );

        tfb_fb_remove_created_node();

        return 0;
    }

    /*
     * Reuse the existing renderer state.
     *
     * The renderer already understands:
     *   - mapped base address;
     *   - visible width/height;
     *   - driver/kernel supplied row pitch.
     */
    tfb_fb_fd = fd;
    tfb_fb_active = 1;

    tfb_fb_menu_active = 1;
    tfb_fb_menu_mapped_address = (
        mapped_address
    );
    tfb_fb_menu_mapped_size = (
        info.mmap_bytes
    );
    tfb_fb_menu_width = (
        info.width
    );
    tfb_fb_menu_height = (
        info.height
    );
    tfb_fb_menu_pitch = (
        info.stride
    );

    tfb_create_presence_marker(
        "/dev/treeforge-bootstrap-fb-info-ok"
    );

    tfb_create_presence_marker(
        "/dev/treeforge-bootstrap-fb-mmap-ok"
    );

    tfb_create_presence_marker(
        "/dev/treeforge-bootstrap-fb-menu-active"
    );

    tfb_log(
        "treeforge-bootstrap-fb menu-active"
    );

    return 1;
}


static void tfb_fb_cleanup(void) {
    if (!tfb_fb_active) {
        return;
    }

    if (
        tfb_fb_menu_mapped_address >= 0
        && tfb_fb_menu_mapped_size != 0
    ) {
        tfb_syscall2(
            SYS_MUNMAP,
            tfb_fb_menu_mapped_address,
            (long) tfb_fb_menu_mapped_size
        );
    }

    tfb_close(
        tfb_fb_fd
    );

    tfb_fb_remove_created_node();

    tfb_fb_fd = -1;
    tfb_fb_active = 0;

    tfb_fb_menu_active = 0;
    tfb_fb_menu_mapped_address = -1;
    tfb_fb_menu_mapped_size = 0;

    tfb_fb_menu_width = 0;
    tfb_fb_menu_height = 0;
    tfb_fb_menu_pitch = 0;

    tfb_create_presence_marker(
        "/dev/treeforge-bootstrap-fb-cleanup"
    );
}


static void tfb_fb_menu_cleanup(void) {
    /*
     * The renderer uses the tfb_fb_* internal state
     * names, while display ownership is exclusively the TreeForge
     * Bootstrap framebuffer bridge.
     */
    if (tfb_fb_active) {
        tfb_fb_cleanup();
    }
}

static void tfb_probe_display_surfaces(void) {
    /*
     * TREEFORGE_BOOTSTRAP_FB_BRIDGE_ONLY_V1
     *
     * Hardware acceptance proved the GS201 bootloader framebuffer
     * bridge as the canonical TreeForge Bootstrap early-display path.
     *
     * Do not program DRM/KMS here and do not fall back to the legacy
     * TreeForge Bootstrap framebuffer realization path.
     */
    if (tfb_fb_begin()) {
        tfb_create_presence_marker(
            "/dev/treeforge-bootstrap-fb-primary"
        );

        return;
    }

    tfb_create_presence_marker(
        "/dev/treeforge-bootstrap-fb-required-failed"
    );

    tfb_log(
        "treeforge-bootstrap-fb required-failed"
    );
}
