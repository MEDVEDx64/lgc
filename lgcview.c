/// LGC Viewer

#include <SDL/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "lgc/lgc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static GLfloat scene_x = 0;
static GLfloat scene_y = 0;

static int scene_scale_x = 0;
static int scene_scale_y = 0;

static GLfloat scene_scale = 1;

static int grid = 1;

enum {
    KEY_NOTHING, KEY_DOWN,
    KEY_UP, KEY_PRESSED
};
static int keyst[0x200];

void print_layer(lgcLayer *l) {
    printf("w: %u\nh: %u\nx: %d\ny: %d\nformat: %d\nflags: %d\nlength (z): %u\ndataptr: 0x%X\n\n",
            l->w,
            l->h,
            l->x,
            l->y,
            l->format,
            l->flags,
            l->length,
            (unsigned int)l->data
    );

}

void eventloop() {
    SDL_Event ev;

    int i;
    for(i = 0; i < 0x200; i++) {
        if(keyst[i] == KEY_DOWN) keyst[i] = KEY_PRESSED;
        if(keyst[i] == KEY_UP) keyst[i] = KEY_NOTHING;
    }

    if(SDL_PollEvent(&ev)) {

        if(ev.type == SDL_QUIT)
            exit(0);

        if(ev.type == SDL_KEYDOWN)
            keyst[ev.key.keysym.sym] = KEY_DOWN;
        if(ev.type == SDL_KEYUP)
            keyst[ev.key.keysym.sym] = KEY_UP;

    }
}

#define SCENE_MOVEMENT_SPEED (-8.0*scene_scale)
#define SCENE_SCALING_SPEED 0.95

void kpress_logic() {

    if(keyst[SDLK_q] == KEY_DOWN) exit(0);

    // Movement
    if(keyst[SDLK_UP]) scene_y -= SCENE_MOVEMENT_SPEED;
    if(keyst[SDLK_LEFT]) scene_x -= SCENE_MOVEMENT_SPEED;

    if(keyst[SDLK_DOWN]) scene_y += SCENE_MOVEMENT_SPEED;
    if(keyst[SDLK_RIGHT]) scene_x += SCENE_MOVEMENT_SPEED;

    // Zoom
    if(keyst[SDLK_i]) {
        scene_scale /= SCENE_SCALING_SPEED;
    }
    if(keyst[SDLK_o]) {
        scene_scale *= SCENE_SCALING_SPEED;
    }

    // Grid toggling
    if(keyst[SDLK_g] == KEY_DOWN) grid =! grid;

}

void projection_reset() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-scene_scale_x, SDL_GetVideoSurface()->w+scene_scale_x,
        SDL_GetVideoSurface()->h+scene_scale_y, -scene_scale_y, -1, 1 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int init(uint16_t w, uint16_t h) {

    if(SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "%s: Unable to set up SDL (%s)\n",
                __FUNCTION__, SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

    if(SDL_SetVideoMode(w, h, 0, SDL_OPENGL | SDL_OPENGLBLIT) == NULL)
    {
        fprintf(stderr, "%s: Can't create screen (%s).\n",
                __FUNCTION__, SDL_GetError());
        return 1;
    }

    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)w/(GLfloat)h, 0.1f, 10.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    projection_reset();

    glClearColor(0,0,0,0);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    scene_x -= w/2;
    scene_y -= h/2;

    return 0;
}

typedef struct {
    GLuint *gltex;
    lgcImage *srcimg;
}
imagepack_t;

imagepack_t *imgload(const char *filename) {
    lgcImage *img = lgcReadImage(filename, LGC_RW_ENTRIE);
    if(!img) return NULL;
    if(!img->layers_count) return NULL;

    GLuint *gltex = malloc(sizeof(GLuint)*img->layers_count);
    glGenTextures(img->layers_count, gltex);

    int i;
    for(i = 0; i < img->layers_count; i++) {
        printf("layer %d:\n", i);
        print_layer(&img->layers[i]);

        if(img->layers[i].format&56) continue; // skipping if pixel format is not gray of RGB

        GLint maxTexSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
        if(img->layers[i].w > maxTexSize)
            continue;

        glBindTexture(GL_TEXTURE_2D, gltex[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        img->layers[i].data = realloc(img->layers[i].data, img->layers[i].length+1563);

        switch(LGC_BYTES_PER_PIXEL(img->layers[i].format)) {
            case 1: glTexImage2D(GL_TEXTURE_2D, 0, 1, img->layers[i].w, img->layers[i].h, 0,
                                  GL_RED, GL_UNSIGNED_BYTE, img->layers[i].data); break;
            case 3: glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img->layers[i].w, img->layers[i].h, 0,
                                  GL_RGB, GL_UNSIGNED_BYTE, img->layers[i].data); break;
            case 4: glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->layers[i].w, img->layers[i].h, 0,
                                  GL_RGBA, GL_UNSIGNED_BYTE, img->layers[i].data); break;
        }

    }

    imagepack_t *pk = malloc(sizeof(imagepack_t));
    pk->gltex = gltex;
    pk->srcimg = img;
    return pk;

}

void draw_grid() {

    glLoadIdentity();
    glTranslatef(scene_x/scene_scale+SDL_GetVideoSurface()->w/2,
        scene_y/scene_scale+SDL_GetVideoSurface()->h/2, 0);

    glColor3f(0, 0, 0.5);

    glBegin(GL_LINES);

    GLint i;
    for(i = -1024; i <= 1024; i += 32) {

        glVertex3f(i/scene_scale, -1024/scene_scale, -1);
        glVertex3f(i/scene_scale, 1024/scene_scale, -1);

        glVertex3f(-1024/scene_scale, i/scene_scale, -1);
        glVertex3f(1024/scene_scale, i/scene_scale, -1);

    }

    glColor3f(0, 0.25, 1);
    for(i = -1024; i <= 1024; i += 512) {

        glVertex3f(i/scene_scale, -1024/scene_scale, -1);
        glVertex3f(i/scene_scale, 1024/scene_scale, -1);

        glVertex3f(-1024/scene_scale, i/scene_scale, -1);
        glVertex3f(1024/scene_scale, i/scene_scale, -1);

    }

    glColor3f(1, 1, 1);

    glVertex3f(0/scene_scale, -1024/scene_scale, -1);
    glVertex3f(0/scene_scale, 1024/scene_scale, -1);

    glVertex3f(-1024/scene_scale, 0/scene_scale, -1);
    glVertex3f(1024/scene_scale, 0/scene_scale, -1);

    glEnd();

}

#define DISTANCE_DIV 50

void draw_images(imagepack_t *pk) {
    int i; for(i = 0; i < pk->srcimg->layers_count; i++) {

        glColor4f(1,1,1,1);
        glBindTexture(GL_TEXTURE_2D, pk->gltex[i]);

        glEnable(GL_TEXTURE_2D);

        glLoadIdentity();
        glTranslatef((pk->srcimg->layers[i].x+scene_x)/scene_scale+SDL_GetVideoSurface()->w/2,
            (pk->srcimg->layers[i].y+scene_y)/scene_scale+SDL_GetVideoSurface()->h/2,0);

        glBegin(GL_QUADS);

        glTexCoord2f(0, 0);
        glVertex4f(0, 0, (GLfloat)i/(GLfloat)DISTANCE_DIV, scene_scale);
        glTexCoord2f(0, 1);
        glVertex4f(0, pk->srcimg->layers[i].h, (GLfloat)i/(GLfloat)DISTANCE_DIV, scene_scale);
        glTexCoord2f(1, 1);
        glVertex4f(pk->srcimg->layers[i].w,pk->srcimg->layers[i].h, (GLfloat)i/(GLfloat)DISTANCE_DIV, scene_scale);
        glTexCoord2f(1, 0);
        glVertex4f(pk->srcimg->layers[i].w, 0, (GLfloat)i/(GLfloat)DISTANCE_DIV, scene_scale);

        glEnd();

    }

}

int main(int argc, char *argv[]) {
    if(argc < 2) return -1;
    if(init(640,400)) exit(-1);

    imagepack_t *pk = imgload(argv[1]);
    if(!pk) return -1;

    memset(&keyst, KEY_NOTHING, 0x200);
    glClearColor(0,0,0,1);

    while(1) {
        eventloop();
        kpress_logic();

        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        if(grid) draw_grid();
        draw_images(pk);
        SDL_GL_SwapBuffers();

        SDL_Delay(32);

    }

    SDL_Quit();
    return 0;

}
