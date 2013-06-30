/**

    lgc.c
    Layer Graphics Container API implementation

    © Andrey Bobkov 'MEDVEDx64', 2013.
    This software comes under the terms of MIT License.

**/

/* There are almost no comments. Sorry about this. */

#include "lgc.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#define COMPRESION_LEVEL 9

lgcImage * lgcBlankImage() {

    lgcImage * img = malloc(sizeof(lgcImage));
    memset(img, 0, sizeof(lgcImage));

    img->magic = LGC_MAGIC;
    return img;

}

lgcLayer * lgcBlankLayer() {

    lgcLayer *layer = malloc(sizeof(lgcLayer));
    memset(layer, 0, sizeof(lgcLayer));
    return layer;

}

void lgcDestroyImage(lgcImage *image, int force_freeing){
    if(image->layers) {
        int i;
        for(i = 0; i < image->layers_count; ++i) {

            lgcDestroyLayer(&image->layers[i], 0);

        }

        free(image->layers);
    }

    if(force_freeing)
        free(image);

}

void lgcDestroyLayer(lgcLayer *layer, int force_freeing) {
    if(layer->data)
        free(layer->data);

    if(force_freeing)
        free(layer);

}

void lgcPushLayer(lgcImage *dest, lgcLayer *layer)
{
    if(dest->layers_count && !dest->layers) {
        fprintf(stderr, "%s: warning: there are no layers present in destination "
                        "image, but it's layers_count != 0. I will not append a layer there!",
                         __FUNCTION__);
        return;
    }

    if(dest->layers) {
        dest->layers = realloc(dest->layers, (dest->layers_count+1)*sizeof(lgcLayer));
        memcpy(&dest->layers[dest->layers_count], layer, sizeof(lgcLayer));
        dest->layers[dest->layers_count].data = malloc(dest->layers[dest->layers_count].length);
        memcpy(dest->layers[dest->layers_count].data, layer->data, layer->length);
    }
    else {
        dest->layers = malloc(sizeof(lgcLayer));
        memcpy(dest->layers, layer, sizeof(lgcLayer));
        dest->layers[0].data = malloc(layer->length);
        memcpy(dest->layers[0].data, layer->data, layer->length);
    }

    dest->layers_count++;

}

lgcLayer * lgcPopLayer(lgcImage *image) {
    if(!image->layers_count || !image->layers)
        return NULL;

    lgcLayer *layer = malloc(sizeof(lgcLayer));
    memcpy(layer, &image->layers[image->layers_count-1], sizeof(lgcLayer));
    image->layers_count--;
    image->layers = realloc(image->layers, image->layers_count*sizeof(lgcImage));

    return layer;

}

#define RET_R_FAILURE do \
    {                                                                       \
        fprintf(stderr, "%s: read error (%d)\n", __FUNCTION__, __LINE__);   \
        fclose(f);                                                          \
        return NULL;                                                        \
    } while(0)

#define RET_W_FAILURE do \
    {                                                                       \
        fprintf(stderr, "%s: write error (%d)\n", __FUNCTION__, __LINE__);  \
        fclose(f);                                                          \
        return -1;                                                          \
    } while(0)

int checkHead(FILE *file, uint32_t *layers_c) { // checks magic number, returns zero on success
    // it also fetches layers count value
    fseek(file, LGC_BASE_OFFSET, SEEK_SET);
    int mgck = 0;
    if(fread(&mgck, 4, 1, file) != 1) {
        rewind(file);
        return -1;
    }

    if(layers_c) {
        if(fread(layers_c, 4, 1, file) != 1) {
            rewind(file);
            return -1;
        }
    }

    rewind(file);
    if(mgck == LGC_MAGIC)
        return 0;
    else
        return 1;
}

int readLayer(FILE *f, lgcLayer *layer, int only_head) {
    uLongf len = 0;

    if(fread(&layer->w, 2, 1, f) != 1) return -1;
    if(fread(&layer->h, 2, 1, f) != 1) return -1;
    if(fread(&layer->x, 4, 1, f) != 1) return -1;
    if(fread(&layer->y, 4, 1, f) != 1) return -1;
    if(fread(&layer->format, 1, 1, f) != 1) return -1;
    if(fread(&layer->flags, 4, 1, f) != 1) return -1;
    if(fread(&len, 4, 1, f) != 1) return -1;

    if(only_head) return 0;

    void *src_buf = malloc(len);
    if(fread(src_buf, len, 1, f) != 1) {
        free(src_buf);
        return -1;
    }

    if(layer->format&LGC_FMT_COMPRESSED) {
        uLong ucomp_len = LGC_LAYER_BODY_LENGTH(layer);
        layer->data = malloc(ucomp_len);
        uncompress(layer->data, &ucomp_len,
                    src_buf, len);
        //layer->data = realloc(layer->data, ucomp_len);
        free(src_buf);

        layer->length = ucomp_len;

    }
    else layer->data = src_buf;

    return 0;
}

lgcLayer * lgcReadLayer(const char * filename, int rwopts, uint32_t layer_n) {

    if(!(rwopts&LGC_RW_ENTRIE)) return NULL;

    FILE *f = rwopts&LGC_RW_FORCE_FILE_POINTER? (FILE*)filename: fopen(filename, "rb");
    if(!f)
    {
        fprintf(stderr, rwopts&LGC_RW_FORCE_FILE_POINTER? "%s: filename is NULL\n":
                "%s: can't open the file (%s)\n", __FUNCTION__, filename);
        return NULL;
    }

    uint32_t lc = 0;

    if(checkHead(f, &lc)) {
        fprintf(stderr, "%s: read error or bad magic number\n", __FUNCTION__);
        if(!(rwopts&LGC_RW_FORCE_FILE_POINTER)) fclose(f);
        return NULL;
    }

    if(layer_n >= lc) {
        fprintf(stderr, "%s: layer %u does not exist in image\n", __FUNCTION__, layer_n);

        if(rwopts&LGC_RW_FORCE_FILE_POINTER)
            rewind(f);
        else
            fclose(f);
        return NULL;
    }

    fseek(f, LGC_BASE_OFFSET+8, SEEK_SET);

    int i;
    for(i = 0; i < layer_n; ++i) {

        fseek(f, 17, SEEK_CUR);
        long len = 0;
        if(fread(&len, 4, 1, f) != 1) {
            if(rwopts&LGC_RW_FORCE_FILE_POINTER) rewind(f);
            else fclose(f);
        }

        fseek(f, len, SEEK_CUR);
    }

    lgcLayer *layer = lgcBlankLayer();
    if(readLayer(f, layer, !(rwopts&LGC_RW_BODY))) {
        fprintf(stderr, "%s: read error\n", __FUNCTION__);
        lgcDestroyLayer(layer, 1);
        if(rwopts&LGC_RW_FORCE_FILE_POINTER) rewind(f);
        else
            fclose(f);
        return NULL;
    }

    if(rwopts&LGC_RW_FORCE_FILE_POINTER)
        rewind(f);
    else
        fclose(f);

    return layer;

}

lgcImage * lgcReadImage(const char * filename, int rwopts)
{

    if(!(rwopts&LGC_RW_ENTRIE)) return NULL;
    FILE *f = rwopts&LGC_RW_FORCE_FILE_POINTER? (FILE*)filename: fopen(filename, "rb");
    if(!f)
    {
        fprintf(stderr, rwopts&LGC_RW_FORCE_FILE_POINTER? "%s: filename is NULL\n":
                "%s: can't open the file (%s)\n", __FUNCTION__, filename);
        return NULL;
    }

    lgcImage * img = malloc(sizeof(lgcImage));
    memset(img, 0, sizeof(lgcImage));

    if(fread(&img->unused, LGC_BASE_OFFSET, 1, f) != 1) RET_R_FAILURE;
    if(fread(&img->magic, 4, 1, f) != 1) RET_R_FAILURE;

    if(img->magic != LGC_MAGIC) {
        fprintf(stderr, "%s: bad magic number\n", __FUNCTION__);
        free(img);
        return NULL;
    }

    if(fread(&img->layers_count, 4, 1, f) != 1) RET_R_FAILURE;
    if(!img->layers_count) {

        if(rwopts&LGC_RW_FORCE_FILE_POINTER)
            rewind(f);
        else
            fclose(f);

        return img;
    }

    if(!(rwopts&LGC_RW_BODY)) {
        if(rwopts&LGC_RW_FORCE_FILE_POINTER)
            rewind(f);
        else
            fclose(f);

        return img;
    }

    img->layers = malloc(sizeof(lgcLayer)*img->layers_count);

    register int i;
    for(i = 0; i < img->layers_count; ++i) {

        if(readLayer(f, &img->layers[i], 0)) {

            fprintf(stderr, "%s: warning — skipping corrupted layer\n", __FUNCTION__);
            i--;
            img->layers_count--;
            continue;

        }

    }

    if(rwopts&LGC_RW_FORCE_FILE_POINTER) rewind(f);
    else fclose(f);

    return img;

}

int writeLayer(FILE *f, lgcLayer *layer) {

    uLong len = LGC_LAYER_BODY_LENGTH(layer);
    void *compressed = NULL;
    if(layer->format&LGC_FMT_COMPRESSED) {
        len += 0x20;
        compressed = malloc(len);
        compress2(compressed, &len, layer->data, LGC_LAYER_BODY_LENGTH(layer),
                    COMPRESION_LEVEL);
    }

    if(fwrite(&layer->w, 2, 1, f) != 1) return -1;
    if(fwrite(&layer->h, 2, 1, f) != 1) return -1;
    if(fwrite(&layer->x, 4, 1, f) != 1) return -1;
    if(fwrite(&layer->y, 4, 1, f) != 1) return -1;
    if(fwrite(&layer->format, 1, 1, f) != 1) return -1;
    if(fwrite(&layer->flags, 4, 1, f) != 1) return -1;
    if(fwrite(&len, 4, 1, f) != 1) return -1;
    if(fwrite((layer->format&LGC_FMT_COMPRESSED)?
                compressed:
                layer->data, len, 1, f) != 1) return -1;

    if(compressed) free(compressed);

    return 0;

}

int lgcWriteToFile(const char * filename, int rwopts, lgcImage* image)
{

    if(!(rwopts&LGC_RW_ENTRIE)) return 0;

    if(!filename || !image) {
        fprintf(stderr, "%s: NULL in arguments\n", __FUNCTION__);
        return -1;
    }

    if(image->magic != LGC_MAGIC) {
        fprintf(stderr, "%s: 'image' is not a LGC?\n", __FUNCTION__);
        return -1;
    }

    FILE *f = rwopts&LGC_RW_FORCE_FILE_POINTER? (FILE*)filename: NULL;
    if(rwopts&LGC_RW_FORCE_FILE_POINTER) rewind(f); else {
        if(!(f = fopen(filename, "wb"))) {
            fprintf(stderr, "%s: can't open the file for writing (%s)\n", __FUNCTION__, filename);
            return -1;
        }
    }

    if(fwrite(&image->unused, LGC_BASE_OFFSET, 1, f) != 1) RET_W_FAILURE;
    if(fwrite(&image->magic, 4, 1, f) != 1) RET_W_FAILURE;
    if(fwrite(&image->layers_count, 4, 1, f) != 1) RET_W_FAILURE;

    if(!image->layers_count) {
        if(rwopts&LGC_RW_FORCE_FILE_POINTER) rewind(f);
        else fclose(f);
        return 0;
    }

    if(!(rwopts&LGC_RW_BODY)) {
        if(rwopts&LGC_RW_FORCE_FILE_POINTER)
            rewind(f);
        else
            fclose(f);
        return 0;
    }

    register int i;
    for(i = 0; i < image->layers_count; ++i) {

        if(writeLayer(f, &image->layers[i])) {
            fprintf(stderr, "%s: write error\n", __FUNCTION__);

            if(rwopts&LGC_RW_FORCE_FILE_POINTER) rewind(f);
            else fclose(f);

            return -1;
        }

    }

    if(rwopts&LGC_RW_FORCE_FILE_POINTER) rewind(f);
    else fclose(f);
    return 0;

}

int lgcAppendLayerToFile(const char * filename, int rwopts, lgcLayer *layer) {
    if(rwopts&LGC_RW_FORCE_FILE_POINTER) {
        fprintf(stderr, "%s: error: usage of external stream is not supported by this function\n",
            __FUNCTION__);
        return 1;
    }

    FILE *f = fopen(filename, "r+b");
    if(!f) {
        fprintf(stderr, "%s: error: can't open the file (%s)\n",
            __FUNCTION__, filename);
        return 1;
    }

    if(checkHead(f, NULL)) {
        fprintf(stderr, "%s: bad magic number\n", __FUNCTION__);
        fclose(f);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    if(writeLayer(f, layer)) {
        fprintf(stderr, "%s: error occured while writing\n", __FUNCTION__);
        fclose(f);
        return 1;
    }

    uint32_t l_count = 0;

    fseek(f, LGC_BASE_OFFSET+4, SEEK_SET);
    if(fread(&l_count, 4, 1, f) != 1) {
        fprintf(stderr, "%s: failed to read layers_count\n", __FUNCTION__);
        fclose(f);
        return 1;
    }

    l_count += 1;
    fseek(f, LGC_BASE_OFFSET+4, SEEK_SET);
    if(fwrite(&l_count, 4, 1, f) != 1) {
        fprintf(stderr, "%s: failed to overwrite layers_count\n", __FUNCTION__);
        fclose(f);
        return 1;
    }

    fclose(f);
    return 0;

}
