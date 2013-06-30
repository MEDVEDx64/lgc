#include "lgc.h"

#include <malloc.h>
#include <string.h>

int main() {

    lgcImage *img = lgcBlankImage();

    char dumm[LGC_BASE_OFFSET] = "Wololo!";
    memcpy(&img->unused, &dumm, LGC_BASE_OFFSET);

    img->magic = LGC_MAGIC;
    img->layers_count = 2;
    img->layers = malloc(sizeof(lgcLayer)*2);

    img->layers[0].w = 320;
    img->layers[0].h = 240;
    img->layers[0].x = 100;
    img->layers[0].y = 120;
    img->layers[0].format = LGC_FMT_RGBA8|LGC_FMT_COMPRESSED;
    img->layers[0].flags = 0;
    lgcLayer *bp = (&img->layers[0]);
    img->layers[0].length = LGC_LAYER_LENGTH(bp);

    memcpy(&img->layers[1], &img->layers[0], 21);

    img->layers[1].x = 0;
    img->layers[1].y = 20;
    img->layers[1].w = 128;
    img->layers[1].h = 128;
    img->layers[1].format = LGC_FMT_8BIT|LGC_FMT_COMPRESSED;
    bp = (&img->layers[1]);
    img->layers[1].length = LGC_LAYER_LENGTH(bp);

    img->layers[0].data = malloc(LGC_BYTES_PER_PIXEL(img->layers[0].format)*(320*240));
    img->layers[1].data = malloc(128*128);

    memset(img->layers[0].data, 'a', LGC_BYTES_PER_PIXEL(img->layers[0].format)*(320*240));
    memset(img->layers[1].data, 'b', 128*128);

    lgcWriteToFile("ngtest.lc1", LGC_RW_ENTRIE, img);

    free(img->layers[0].data);
    free(img->layers[1].data);
    free(img->layers);
    free(img);

    printf("load/resave test\n");
    lgcImage *test2 = lgcReadImage("ngtest.lc1", LGC_RW_ENTRIE);
    if(test2 == NULL) {
        printf("difread fail\n");
        return 1;
    }
    printf("_out\n");
    lgcWriteToFile("ngtest_out.lc1", LGC_RW_ENTRIE, test2);

    lgcLayer *lr = lgcReadLayer("ngtest_out.lc1", 3, 1);
    /*lgcLayer *lr = lgcBlankLayer();
    lr->w = 80;
    lr->h = 40;
    lr->format = LGC_FMT_8BIT|LGC_FMT_COMPRESSED;
    lr->length = 80*40;
    lr->data = malloc(80*40);
    memset(lr->data, 'x', 80*40);*/

    if(!lr) return 123;

    lr->x = 50;
    lr->y = -50;

    lgcPushLayer(test2, lr);
    printf("_2\n");
    lgcWriteToFile("ngtest_2.lc1", LGC_RW_ENTRIE, test2);
    printf("layers: %u\n", test2->layers_count);

    lgcDestroyLayer(lgcPopLayer(test2), 1);
    //lgcPopLayer(test2);
    printf("_3\n");
    lgcWriteToFile("ngtest_3.lc1", 3, test2);

    //lgcAppendLayerToFile("ngtest_3.lc1", LGC_RW_ENTRIE, lr);

    lgcDestroyLayer(lr, 1);
    lgcDestroyImage(test2, 1);

    return 0;
}
