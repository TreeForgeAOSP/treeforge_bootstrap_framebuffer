/*
 * Retained TreeForge Bootstrap framebuffer menu session.
 *
 * The framebuffer bridge itself still uses the native DRM mode
 * coordinates exactly as exposed by the kernel. TreeForge V17 adds
 * a software-only logical landscape presentation layer above this
 * raw transport; no DRM plane rotation is requested.
 */
static int tfb_fb_menu_active = 0;


static long tfb_fb_menu_mapped_address = -1;
static u64 tfb_fb_menu_mapped_size = 0;

static u32 tfb_fb_menu_width = 0;
static u32 tfb_fb_menu_height = 0;
static u32 tfb_fb_menu_pitch = 0;





/*
 * TREEFORGE_BOOTSTRAP_FB_CONSUMER_V1
 *
 * Early-menu transport for the read-only GS201 framebuffer bridge.
 *
 * The kernel owns discovery/validation of the already-scanning
 * bootloader framebuffer. Userspace only:
 *
 *   - realizes the misc node if Android ueventd has not done so yet;
 *   - reads the fixed ABI-v1 description;
 *   - mmaps the already-live linear 32-bit scanout;
 *   - points the existing menu renderer at that mapping;
 *   - seals the bridge before final OS handoff.
 *
 * No DECON/DPP/DRM programming is performed by this path.
 */
#define TFB_FB_DEVICE_PATH \
    "/dev/treeforge_bootstrap_fb"

#define TFB_FB_SYSFS_DEV_PATH \
    "/sys/class/misc/treeforge_bootstrap_fb/dev"

#define TFB_FB_ABI_VERSION 1U

#define TFB_FB_FLAG_LINEAR \
    (1U << 0)

#define TFB_FB_FLAG_WRITE_COMBINE \
    (1U << 1)

#define TFB_FB_REQUIRED_FLAGS \
    ( \
        TFB_FB_FLAG_LINEAR \
        | TFB_FB_FLAG_WRITE_COMBINE \
    )

/*
 * Linux generic _IO('P', 0x01).
 */
#define TFB_FB_IOCTL_SEAL 0x5001UL

struct tfb_fb_info {
    u32 abi_version;
    u32 hw_format;

    u32 width;
    u32 height;

    u32 source_width;
    u32 source_height;

    u32 source_x;
    u32 source_y;

    u32 stride;
    u32 bytes_per_pixel;

    u32 dpp_index;
    u32 flags;

    u64 framebuffer_bytes;
    u64 mmap_bytes;
};

typedef char tfb_fb_info_size_check[
    sizeof(struct tfb_fb_info) == 64
    ? 1
    : -1
];

static long tfb_fb_fd = -1;
static int tfb_fb_active = 0;
static int tfb_fb_created_node = 0;


static void tfb_fb_fill_rect(
    u32 x,
    u32 y,
    u32 width,
    u32 height,
    u32 pixel
) {
    if (
        !tfb_fb_menu_active
        || tfb_fb_menu_mapped_address < 0
        || x >= tfb_fb_menu_width
        || y >= tfb_fb_menu_height
    ) {
        return;
    }

    if (
        width
        > tfb_fb_menu_width - x
    ) {
        width = (
            tfb_fb_menu_width - x
        );
    }

    if (
        height
        > tfb_fb_menu_height - y
    ) {
        height = (
            tfb_fb_menu_height - y
        );
    }

    volatile unsigned char *base = (
        (volatile unsigned char *)
        (usize) tfb_fb_menu_mapped_address
    );

    for (
        u32 row_index = 0;
        row_index < height;
        row_index++
    ) {
        volatile u32 *row = (
            (volatile u32 *)
            (
                base
                + (
                    (usize) (
                        y + row_index
                    )
                    * (usize) tfb_fb_menu_pitch
                )
            )
        );

        for (
            u32 column = 0;
            column < width;
            column++
        ) {
            row[
                x + column
            ] = pixel;
        }
    }
}


static unsigned char tfb_fb_glyph_row(
    char character,
    int row
) {
    if (row < 0 || row > 6) {
        return 0;
    }

    /*
     * The bring-up UI does not require distinct lowercase glyphs.
     * Folding lowercase into the complete uppercase alphabet keeps
     * paths and diagnostic text readable without duplicating font data.
     */
    if (
        character >= 'a'
        && character <= 'z'
    ) {
        character = (
            character
            - 'a'
            + 'A'
        );
    }

#define TF_GLYPH(a,b,c,d,e,f,g) \
    ((const unsigned char[7]) { \
        (a),(b),(c),(d),(e),(f),(g) \
    })[row]

    switch (character) {
        case 'A': return TF_GLYPH(0x0E,0x11,0x11,0x1F,0x11,0x11,0x11);
        case 'B': return TF_GLYPH(0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E);
        case 'C': return TF_GLYPH(0x0E,0x11,0x10,0x10,0x10,0x11,0x0E);
        case 'D': return TF_GLYPH(0x1E,0x11,0x11,0x11,0x11,0x11,0x1E);
        case 'E': return TF_GLYPH(0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F);
        case 'F': return TF_GLYPH(0x1F,0x10,0x10,0x1E,0x10,0x10,0x10);
        case 'G': return TF_GLYPH(0x0E,0x11,0x10,0x17,0x11,0x11,0x0F);
        case 'H': return TF_GLYPH(0x11,0x11,0x11,0x1F,0x11,0x11,0x11);
        case 'I': return TF_GLYPH(0x0E,0x04,0x04,0x04,0x04,0x04,0x0E);
        case 'J': return TF_GLYPH(0x07,0x02,0x02,0x02,0x12,0x12,0x0C);
        case 'K': return TF_GLYPH(0x11,0x12,0x14,0x18,0x14,0x12,0x11);
        case 'L': return TF_GLYPH(0x10,0x10,0x10,0x10,0x10,0x10,0x1F);
        case 'M': return TF_GLYPH(0x11,0x1B,0x15,0x15,0x11,0x11,0x11);
        case 'N': return TF_GLYPH(0x11,0x19,0x15,0x13,0x11,0x11,0x11);
        case 'O': return TF_GLYPH(0x0E,0x11,0x11,0x11,0x11,0x11,0x0E);
        case 'P': return TF_GLYPH(0x1E,0x11,0x11,0x1E,0x10,0x10,0x10);
        case 'Q': return TF_GLYPH(0x0E,0x11,0x11,0x11,0x15,0x12,0x0D);
        case 'R': return TF_GLYPH(0x1E,0x11,0x11,0x1E,0x14,0x12,0x11);
        case 'S': return TF_GLYPH(0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E);
        case 'T': return TF_GLYPH(0x1F,0x04,0x04,0x04,0x04,0x04,0x04);
        case 'U': return TF_GLYPH(0x11,0x11,0x11,0x11,0x11,0x11,0x0E);
        case 'V': return TF_GLYPH(0x11,0x11,0x11,0x11,0x11,0x0A,0x04);
        case 'W': return TF_GLYPH(0x11,0x11,0x11,0x15,0x15,0x15,0x0A);
        case 'X': return TF_GLYPH(0x11,0x11,0x0A,0x04,0x0A,0x11,0x11);
        case 'Y': return TF_GLYPH(0x11,0x11,0x0A,0x04,0x04,0x04,0x04);
        case 'Z': return TF_GLYPH(0x1F,0x01,0x02,0x04,0x08,0x10,0x1F);

        case '0': return TF_GLYPH(0x0E,0x11,0x13,0x15,0x19,0x11,0x0E);
        case '1': return TF_GLYPH(0x04,0x0C,0x04,0x04,0x04,0x04,0x0E);
        case '2': return TF_GLYPH(0x0E,0x11,0x01,0x02,0x04,0x08,0x1F);
        case '3': return TF_GLYPH(0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E);
        case '4': return TF_GLYPH(0x02,0x06,0x0A,0x12,0x1F,0x02,0x02);
        case '5': return TF_GLYPH(0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E);
        case '6': return TF_GLYPH(0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E);
        case '7': return TF_GLYPH(0x1F,0x01,0x02,0x04,0x08,0x08,0x08);
        case '8': return TF_GLYPH(0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E);
        case '9': return TF_GLYPH(0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E);

        case '/': return TF_GLYPH(0x01,0x02,0x02,0x04,0x08,0x08,0x10);
        case '\\': return TF_GLYPH(0x10,0x08,0x08,0x04,0x02,0x02,0x01);
        case '-': return TF_GLYPH(0x00,0x00,0x00,0x1F,0x00,0x00,0x00);
        case '_': return TF_GLYPH(0x00,0x00,0x00,0x00,0x00,0x00,0x1F);
        case '.': return TF_GLYPH(0x00,0x00,0x00,0x00,0x00,0x06,0x06);
        case ':': return TF_GLYPH(0x00,0x04,0x04,0x00,0x04,0x04,0x00);
        case '=': return TF_GLYPH(0x00,0x1F,0x00,0x1F,0x00,0x00,0x00);
        case '+': return TF_GLYPH(0x00,0x04,0x04,0x1F,0x04,0x04,0x00);
        case '%': return TF_GLYPH(0x19,0x1A,0x04,0x04,0x08,0x0B,0x13);
        case '^': return TF_GLYPH(0x04,0x0A,0x11,0x00,0x00,0x00,0x00);
        case '(': return TF_GLYPH(0x02,0x04,0x08,0x08,0x08,0x04,0x02);
        case ')': return TF_GLYPH(0x08,0x04,0x02,0x02,0x02,0x04,0x08);
        case '[': return TF_GLYPH(0x0E,0x08,0x08,0x08,0x08,0x08,0x0E);
        case ']': return TF_GLYPH(0x0E,0x02,0x02,0x02,0x02,0x02,0x0E);
        case ' ': return 0;

        default:
            /*
             * Unknown characters are intentionally visible rather than
             * silently disappearing from diagnostic output.
             */
            return TF_GLYPH(
                0x1F,
                0x11,
                0x01,
                0x02,
                0x04,
                0x00,
                0x04
            );
    }

#undef TF_GLYPH
}


static void tfb_fb_draw_char(
    u32 x,
    u32 y,
    char value,
    u32 scale,
    u32 pixel
) {
    for (
        int row = 0;
        row < 7;
        row++
    ) {
        unsigned char bits = (
            tfb_fb_glyph_row(
                value,
                row
            )
        );

        for (
            int column = 0;
            column < 5;
            column++
        ) {
            if (
                bits
                & (
                    1U
                    << (
                        4 - column
                    )
                )
            ) {
                tfb_fb_fill_rect(
                    x + (
                        (u32) column
                        * scale
                    ),
                    y + (
                        (u32) row
                        * scale
                    ),
                    scale,
                    scale,
                    pixel
                );
            }
        }
    }
}


static u32 tfb_fb_text_width(
    const char *value,
    u32 scale
) {
    usize length = (
        tfb_strlen(
            value
        )
    );

    if (length == 0) {
        return 0;
    }

    return (
        ((u32) length * 6U * scale)
        - scale
    );
}


static void tfb_fb_draw_text(
    u32 x,
    u32 y,
    const char *value,
    u32 scale,
    u32 pixel
) {
    u32 cursor = x;

    for (
        usize index = 0;
        value[index] != '\0';
        index++
    ) {
        if (value[index] != ' ') {
            tfb_fb_draw_char(
                cursor,
                y,
                value[index],
                scale,
                pixel
            );
        }

        cursor += (
            6U * scale
        );
    }
}
