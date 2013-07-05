/**
bmp/png/jpg/etc to lgc converter
MEDVEDx64
**/

#include "lgc/lgc.h"

#include <stdio.h>
#include <SDL/SDL_image.h>

lgcImage * surf2lgc(SDL_Surface *surf, int compressed) {

    if(!surf) return NULL;

    lgcImage *img = lgcBlankImage();
    lgcLayer *lyr = lgcBlankLayer();

    lyr->w = surf->w;
    lyr->h = surf->h;

    switch(surf->format->BytesPerPixel) {

        case 1: lyr->format = LGC_FMT_GRAY | LGC_FMT_8BIT; break;
        case 3: lyr->format = LGC_FMT_RGB | LGC_FMT_24BIT; break;
        case 4: lyr->format = LGC_FMT_RGB | LGC_FMT_32BIT; break;
        default:
            lgcDestroyImage(img, 1);
            printf("Error: unsupported pixel format.\n");
            return NULL;

    }

    lyr->format |= compressed? LGC_FMT_COMPRESSED: 0;
    lyr->length = LGC_LAYER_BODY_LENGTH(lyr);

    lyr->data = malloc(lyr->length);
    memcpy(lyr->data, surf->pixels, lyr->length);

    lgcPushLayer(img, lyr);
    lgcDestroyLayer(lyr, 1);

    return img;

}

int main(int argc, char *argv[]) {

    if(argc < 3) {
        printf("usage: %s [SOURCE FILE] [DESTINATION FILE]\n", argv[0]);
        return 0;
    }

    SDL_Surface *surf = IMG_Load(argv[1]);
    if(!surf) {
        printf("Failed to load source image.\n");
        return -1;
    }

    lgcImage *img = surf2lgc(surf, 1);
    if(!img) {
        printf("Conversion failed.\n");
        SDL_FreeSurface(surf);
        return -1;
    }

    if(lgcWriteToFile(argv[2], LGC_RW_ENTRIE, img)) {
        printf("Error: can't write destination file.\n");
        SDL_FreeSurface(surf);
        lgcDestroyImage(img, 1);
        return -1;
    }

    return 0;

}
