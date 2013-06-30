#ifndef LGC_H_
#define LGC_H_

/* lgc.h */

#define LGC_VERSION_MAJOR   0
#define LGC_VERSION_MINOR   2
#define LGC_VERSION_MICRO   0

/**

    Layered Graphics Container

    packaging multiple images into
    one layered image file

**/

/** © Andrey Bobkov 'MEDVEDx64', 2013.
    This software comes under the terms of MIT License.
**/

/*  -- FILE STRUCTURE --

    32 bytes        | unused (may contain custom data)
    4 bytes         | magic
    uint32          | layers_count

    Layers table
    (repeating for layers_count)

    Layer structure:
        uint16          | width
        uint16          | height
        int             | x
        int             | y
        uint8           | format
        int             | flags
        uint32          | length
        raw             | data (pixels)

    ...

*/

/*  -- FORMAT FLAGS --

    7               0
    -----------------
    | | | | | | | | |
    -----------------
     | ||       |   |
     | ||-------|---|
     | |    |     |
     | |    |     -------------- bits per pixel
     | |    -------------------- color model
     | ------------------------- compressed?
     --------------------------- unused

*/

#define LGC_BASE_OFFSET 0x20
#define LGC_MAGIC 0x100006ef

#include <stdint.h>

// Layer struct
typedef struct {

    uint16_t        w, h;
    int32_t         x, y;
    uint8_t         format;
    int32_t         flags;
    uint32_t        length;
    void *          data;

    // Note that 'data' does not stores compressed pixels,
    // and 'length" tells it's uncompressed size.

} lgcLayer;

// Format flags
#define LGC_FMT_8BIT        0
#define LGC_FMT_16BIT       1
#define LGC_FMT_24BIT       2
#define LGC_FMT_32BIT       3

#define LGC_FMT_GRAY        0
#define LGC_FMT_RGB         (1<<2)
#define LGC_FMT_CMYK        (2<<2)
#define LGC_FMT_HSV         (3<<2)
#define LGC_FMT_HLS         (4<<2)
#define LGC_FMT_LAB         (5<<2)

#define LGC_FMT_COMPRESSED  0x40

#define LGC_FMT_RGB8        LGC_FMT_RGB|LGC_FMT_24BIT
#define LGC_FMT_RGBA8       LGC_FMT_RGB|LGC_FMT_32BIT

// Image struct
typedef struct {

    uint8_t         unused[LGC_BASE_OFFSET];
    int32_t         magic;
    uint32_t        layers_count;
    lgcLayer *      layers;

} lgcImage;

#define LGC_BYTES_PER_PIXEL(format) ((format&3)+1)

#define LGC_LAYER_HEAD_LENGTH (sizeof(lgcLayer)-sizeof(void*))

#define LGC_LAYER_BODY_LENGTH(layer) \
    ((layer->w*layer->h)*LGC_BYTES_PER_PIXEL(layer->format))

#define LGC_LAYER_LENGTH(layer) \
    (LGC_LAYER_HEAD_LENGTH+LGC_LAYER_BODY_LENGTH(layer))

// Read/write options flags (rwopts)
#define LGC_RW_HEAD 1   // Will read or write only file's/layer's head
#define LGC_RW_BODY 2   // Will read or write only file's/layer's body
#define LGC_RW_ENTRIE (LGC_RW_HEAD|LGC_RW_BODY)     // Read/write entrie file or layer

#define LGC_RW_FORCE_FILE_POINTER 0x100     // forces 'filename' to be used as file stream

#ifdef __cplusplus
extern "C" {
#endif

/*  Read lgcImage from file.
    filename — file name string or FILE stream pointer
        (if LGC_FORCE_FILE_POINTER specified in rwopts);
    rwopts — read/write options (LGC_RW_HEAD, LGC_RW_ENTRIE, ..).
    Returns lgcImage or NULL on failure. */
extern lgcImage * lgcReadImage(const char * filename, int rwopts);

/*  Read single lgcLayer from file.
    filename — file name string or FILE stream pointer
        (if LGC_FORCE_FILE_POINTER specified in rwopts);
    rwopts — read/write options (LGC_RW_HEAD, LGC_RW_ENTRIE, ..).
    layer_n — number of layer in file.
    Returns lgcLayer or NULL on failure or if layer_n is out of range. */
extern lgcLayer * lgcReadLayer(const char * filename, int rwopts, uint32_t layer_n);

/*  Write lgcImage to file.
    filename — file name string or FILE stream pointer;
    image — source lgcImage;
        (if LGC_FORCE_FILE_POINTER specified in rwopts);
    rwopts — read/write options (LGC_RW_HEAD, LGC_RW_ENTRIE, ..).
    Returns non-zero on failure. */
extern int lgcWriteToFile(const char * filename, int rwopts, lgcImage* image);

/*  Append lgcLayer to file.
    layer — source lgcLayer;
    filename — file name string or FILE stream pointer
        (if LGC_FORCE_FILE_POINTER specified in rwopts);
    rwopts — read/write options (LGC_RW_HEAD, LGC_RW_ENTRIE, ..).
    Returns non-zero on failure. */
extern int lgcAppendLayerToFile(const char * filename, int rwopts, lgcLayer *layer);

/*  Returns newly created lgcImage or lgcLyaer. */
extern lgcImage * lgcBlankImage();
extern lgcLayer * lgcBlankLayer();

// Stack-like layers operations
/*  Appends lgcLayer to image.
    dest — destination lgcImage;
    layer — source lgcLayer.
    It creates a copy of layer in the destination image. */
extern void lgcPushLayer(lgcImage *dest, lgcLayer *layer);

/*  Takes out the last lgcLayer from image and returns it.
    image — lgcImage from which lgcLayer will be taken.
    Returns lgcLayer or NULL when image has no layers or there are some error happened. */
extern lgcLayer * lgcPopLayer(lgcImage *image);

/*  Memory freeing functions for lgcImage and lgcLayer.
    lgcDestroyImage() applies lgcDestroyLayer() to all image's existing layers. */
extern void lgcDestroyImage(lgcImage *image, int force_freeing);
extern void lgcDestroyLayer(lgcLayer *layer, int force_freeing);

#ifdef __cplusplus
}
#endif

#endif // LGC_H_
