//********************************************************
//** Done by  |      Date     |  version |    comment
//**------------------------------------------------------
//**   CEV    |  20-02-2022   |   1.0    | added missing free
//**   CEV    |  24-09-2022   |   1.1.0  | CEV_textureToCapsule corrected
//**   CEV    |  03-04-2023   |   1.1.1  | CEV_textureToCapsule modified to use tmpfile system

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <SDL_Mixer.h>
#include <SDL_ttf.h>
#include <SDL.h>
#include "CEV_types.h"
#include "CEV_mixSystem.h"
#include "CEV_dataFile.h"
#include "CEV_file.h"
#include "rwtypes.h"



static void L_blitRectCorrect(SDL_Rect* srcDim, SDL_Rect* srcClip, SDL_Rect* dstDim, SDL_Rect* dstBlit);

//reads capsule from file
static void L_capsuleDataTypeRead(FILE* src, CEV_Capsule* dst);

//reads capsule from RWpos
static void L_capsuleDataTypeRead_RW(SDL_RWops* src, CEV_Capsule* dst);


void CEV_fontClose(CEV_Font* font)
{/*closes overlay font*/

    if(IS_NULL(font))
        return;

    TTF_CloseFont(font->font);
    free(font->virtualFile);

    free(font);
}


void CEV_waveClose(CEV_Chunk* chunk)
{/*closes overlay sound*/

    if(IS_NULL(chunk))
        return;

    if(chunk->sound != NULL)
        Mix_FreeChunk(chunk->sound);

    if(chunk->virtualFile != NULL)
        free(chunk->virtualFile);

    free(chunk);
}


void CEV_musicClose(CEV_Music* music)
{/*closes overlay music*/

    if(IS_NULL(music))
        return;

    CEV_musicClear(music);

    free(music);
}


void CEV_musicClear(CEV_Music* music)
{//clear music structure contant
    if(IS_NULL(music))
        return;

    Mix_FreeMusic(music->music);
    free(music->virtualFile);

    CEV_soundSystemGet()->loadedMusic = NULL;
}


SDL_Surface* CEV_textureToSurface(SDL_Texture* src, void** pxlData)
{//convert texture into surface

    if (IS_NULL(src) || IS_NULL(pxlData))
    {
        fprintf(stderr, "Err at %s / %d : NULL arg provided.\n", __FUNCTION__, __LINE__ );
        return NULL;
    }

    SDL_Surface* result = NULL;
    uint32_t* pix = NULL;

    SDL_Renderer* render = CEV_videoSystemGet()->render;

    uint32_t format = CEV_PIXELFORMAT;
    int w, h;

    //quering dimension
    SDL_QueryTexture(src, &format, NULL, &w, &h);

    //texture as copy of src
    SDL_Texture* srcCpy = SDL_CreateTexture(render, format, SDL_TEXTUREACCESS_TARGET, w, h);

    if IS_NULL(srcCpy)
    {
        fprintf(stderr, "Err at %s / %d : unable to create texture : %s.\n", __FUNCTION__, __LINE__, SDL_GetError());
        goto end;
    }

    SDL_SetRenderTarget(render, srcCpy);    //copy as render
    //SDL_RenderClear(render);
    SDL_RenderCopy(render, src, NULL, NULL);//blitting src on srcCpy

    //allocating pixel field
    pix = calloc(1, w * h * 32);

    if IS_NULL(pix)
    {
        fprintf(stderr, "Err at %s / %d : unable to allocate pixel field : %s.\n", __FUNCTION__, __LINE__, strerror(errno));
        goto err_1;
    }

    //fetching pixel field
    SDL_RenderReadPixels(render, NULL, format, pix, w*4);

    result = SDL_CreateRGBSurfaceWithFormatFrom(pix, w, h, 32, w*4, format);

    if IS_NULL(result)
    {
        fprintf(stderr, "Err at %s / %d : unable to create surface : %s.\n", __FUNCTION__, __LINE__, SDL_GetError());
        goto err_2;
    }

    SDL_SetRenderTarget(render, NULL);

    SDL_DestroyTexture(srcCpy);
    *pxlData = pix;

end:
    return result;

err_2:
    free(pix);

err_1:
    SDL_SetRenderTarget(render, NULL);
    SDL_DestroyTexture(srcCpy);

    return result;
}


int CEV_textureSavePNG(SDL_Texture *src, char* fileName)
{//save texture as png file

    int funcSts = FUNC_OK;
    void* ptr   = NULL; //holds pxl field

    SDL_Surface *surf = CEV_textureToSurface(src, &ptr);

    if IS_NULL(surf)
    {
        fprintf(stderr, "Err at %s / %d : unable to create surface :%s.\n", __FUNCTION__, __LINE__, SDL_GetError());
        return FUNC_ERR;
    }

    if(IMG_SavePNG(surf, fileName))
    {
        fprintf(stderr, "Err at %s / %d : save failed : %s.\n", __FUNCTION__, __LINE__, IMG_GetError());
        funcSts = FUNC_ERR;
    }

    SDL_FreeSurface(surf);
    free(ptr);

    return funcSts;
}


int CEV_textureSavePNG_RW(SDL_Texture *src, SDL_RWops* dst)
{//saves texture as png virtual file

    int funcSts = FUNC_OK;
    void* ptr = NULL;

    SDL_Surface *surf = CEV_textureToSurface(src, &ptr);

    if IS_NULL(surf)
    {
        fprintf(stderr, "Err at %s / %d : unable to create surface :%s.\n", __FUNCTION__, __LINE__, SDL_GetError());
        return FUNC_ERR;
    }

    if(IMG_SavePNG_RW(surf, dst, 0))
    {
        fprintf(stderr, "Err at %s / %d : save failed : %s.\n", __FUNCTION__, __LINE__, IMG_GetError());
        funcSts = FUNC_ERR;
    }

    SDL_FreeSurface(surf);
    free(ptr);

    return funcSts;
}


int CEV_textureToCapsule(SDL_Texture* src, CEV_Capsule* dst)
{//texture into png file's CEV_Capsule


    int funcSts = FUNC_OK;

    char *fileName/*[L_tmpnam]*/ = NULL;
    fileName = tmpnam(NULL)+1;
    strcat(fileName, ".png");

    //saving png as new file file
    if(CEV_textureSavePNG(src, fileName))
    {
        fprintf(stderr, "Err at %s / %d : unable to save texture into file.\n", __FUNCTION__, __LINE__ );
        funcSts = FUNC_ERR;
        goto end;
    }

    if(CEV_capsuleFromFile(dst, fileName))
    {
        fprintf(stderr, "Err at %s / %d : unable to convert %s into capsule.\n", __FUNCTION__, __LINE__, fileName);
        funcSts = FUNC_ERR;
    }

end:
    remove(fileName);

    return funcSts;
}


SDL_Rect CEV_textureDimGet(SDL_Texture* src)
{//texture size to SDL_Rect

    SDL_Rect result = CLEAR_RECT;

    if(src)
        SDL_QueryTexture(src, NULL, NULL, &result.w, &result.h);

    return result;
}


int CEV_blitSurfaceToTexture(SDL_Surface *src, SDL_Texture* dst, SDL_Rect* srcRect, SDL_Rect* dstRect)
{//blits surface onto texture

    if (IS_NULL(src) || IS_NULL(dst))
    {
        fprintf(stderr, "Err at %s / %d : One arg at least is NULL.\n", __FUNCTION__, __LINE__ );
        return ARG_ERR;
    }

    int access;

    SDL_Rect    clip,
                blit,
                srcDim    = {0, 0, src->w, src->h},
                dstDim    = CLEAR_RECT;

    SDL_QueryTexture(dst, NULL, &access, &dstDim.w, &dstDim.h);

    if(IS_NULL(srcRect))
        clip = srcDim;
    else
        clip = *srcRect;

    if(IS_NULL(dstRect))
        blit = dstDim;
    else
        blit = *dstRect;

    if (access != SDL_TEXTUREACCESS_STREAMING)
    {
        fprintf(stderr, "Err at %s / %d : Texture provided is not SDL_TEXTUREACCESS_STREAMING.\n", __FUNCTION__, __LINE__ );
        return FUNC_ERR;
    }

	L_blitRectCorrect(&srcDim, &clip, &dstDim, &blit);

    int minW = MIN(clip.w, blit.w),
        minH = MIN(clip.h, blit.h),
        dstPitch;

    uint32_t *srcPixel = src->pixels,
             *dstPixel = NULL;

    SDL_LockTexture(dst, &blit, (void**)&dstPixel, &dstPitch);
    dstPitch /= 4;

    for(int ix = 0; ix < minW; ix++)
        for(int iy = 0; iy < minH; iy++)
        {
            uint32_t color = *(srcPixel + (clip.y + iy)*(src->pitch/4) + (clip.x + ix));

            if(color & 0xFF)
                *(dstPixel + iy*dstPitch + ix)=color ;
        }


    SDL_UnlockTexture(dst);

    return FUNC_OK;
}


int CEV_surfaceToCapsule(SDL_Surface* src, CEV_Capsule* dst)
{//inserts SDL_Surface into CEV_Capsule

    int funcSts = FUNC_OK;

    char *fileName/*[L_tmpnam]*/ = NULL;
    fileName = tmpnam(NULL)+1;
    strcat(fileName, ".png");

    //saving png as new file file
    if(IMG_SavePNG(src, fileName))
    {
        fprintf(stderr, "Err at %s / %d : unable to save surface into file.\n", __FUNCTION__, __LINE__ );
        funcSts = FUNC_ERR;
        goto end;
    }

    if(CEV_capsuleFromFile(dst, fileName))
    {
        fprintf(stderr, "Err at %s / %d : unable to convert %s into capsule.\n", __FUNCTION__, __LINE__, fileName);
        funcSts = FUNC_ERR;
    }

end:
    remove(fileName);

    return funcSts;

}


int CEV_capsuleFromFile(CEV_Capsule* caps, const char* fileName)
{//fills Capsule from file _v2

    FILE* file  = NULL;
    readWriteErr = 0;

    if(IS_NULL(caps) || IS_NULL(fileName))
    {
        fprintf(stderr, "Err at %s / %d : arg error.\n", __FUNCTION__, __LINE__);
        return ARG_ERR;
    }

    file = fopen(fileName, "rb");

    if (IS_NULL(file))
    {
        printf("Err at %s / %d : Unable to open file %s : %s\n", __FUNCTION__, __LINE__, fileName, strerror(errno));
        return FUNC_ERR;
    }

    caps->type = CEV_fTypeFromName(fileName);
    caps->size = CEV_fileSize(file);
    rewind(file);

    L_capsuleDataTypeRead(file, caps);//allocs & fills data field

    fclose(file);

    return (readWriteErr)? FUNC_ERR : FUNC_OK;
}


void CEV_capsuleTypeWrite(CEV_Capsule* src, FILE* dst)
{//writes capsule into file where it is

    write_u32le(src->type, dst);
    write_u32le(src->size, dst);

    if(fwrite(src->data, sizeof(char), src->size, dst) != src->size)
        readWriteErr++;
}


void CEV_capsuleTypeRead(FILE* src, CEV_Capsule* dst)
{//reads capsule from file where it is

    dst->type   = read_u32le(src);
    dst->size   = read_u32le(src);

    if(!readWriteErr)
        L_capsuleDataTypeRead(src, dst);//allocs & fills data field
}


void CEV_capsuleTypeWrite_RW(CEV_Capsule* src, SDL_RWops* dst)
{//writes capsule into RWops

    readWriteErr += SDL_WriteLE32(dst, src->type);
    readWriteErr += SDL_WriteLE32(dst, src->size);

    if(SDL_RWwrite(dst, src->data, 1, src->size) != src->size)
        readWriteErr++;
}


void CEV_capsuleTypeRead_RW(SDL_RWops* src, CEV_Capsule* dst)
{//reads capsule from RWops

    dst->type   = SDL_ReadLE32(src);
    dst->size   = SDL_ReadLE32(src);

    L_capsuleDataTypeRead_RW(src, dst);//allocs & fills data field
}


void CEV_capsuleClear(CEV_Capsule* caps)
{//clears fileinfo content

    if(IS_NULL(caps))
        return;

    free(caps->data);

    *caps = (CEV_Capsule){NULL};
}


void CEV_capsuleDestroy(CEV_Capsule* caps)
{//frees fileinfo and content

    free(caps->data);
    free(caps);
}


static void L_blitRectCorrect(SDL_Rect* srcDim, SDL_Rect* srcClip, SDL_Rect* dstDim, SDL_Rect* dstBlit)
{

    CEV_constraint(0, &srcClip->w, srcDim->w);
    CEV_constraint(0, &srcClip->h, srcDim->h);
    CEV_constraint(0, &dstBlit->w, dstDim->w);
    CEV_constraint(0, &dstBlit->h, dstDim->h);

    /*correcting source rect if out of dimensions*/
    if(srcClip->x < 0)
    {
        srcClip->w += srcClip->x;
        srcClip->x = 0;
    }

    if(srcClip->x + srcClip->w > srcDim->w)
        srcClip->w = srcDim->w - srcClip->x;

    if(srcClip->y < 0)
    {
        srcClip->h += srcClip->y;
        srcClip->y = 0;
    }

    if(srcClip->y + srcClip->h > srcDim->h)
        srcClip->h = srcDim->h - srcClip->y;

    /*correcting dst rect if out of dimensions*/
    if(dstBlit->x < 0)
    {
        srcClip->w += dstBlit->x;
        srcClip->x -= dstBlit->x;
        dstBlit->x = 0;
    }

    if(dstBlit->x + dstBlit->w > dstDim->w)
    {
        dstBlit->w = dstDim->w - dstBlit->x;
    }

    if(dstBlit->y < 0)
    {
        srcClip->h += dstBlit->y;
        srcClip->y -= dstBlit->y;
        dstBlit->y = 0;
    }

    if(dstBlit->y + dstBlit->h > dstDim->h)
        dstBlit->h = dstDim->h - dstBlit->y;
}

static void L_capsuleDataTypeRead(FILE* src, CEV_Capsule* dst)
{//fills and allocate caps->data 1.0

    if(dst==NULL || src==NULL)
        readWriteErr++;

    dst->data = malloc(dst->size);

    if (dst->data == NULL)
    {
        fprintf(stderr, "Err at %s / %d : %s.\n", __FUNCTION__, __LINE__, strerror(errno));
        readWriteErr++;
    }
    else if (fread(dst->data, 1, dst->size, src) != dst->size)
    {
        fprintf(stderr, "Err at %s / %d : Unable to read data in src : %s.\n", __FUNCTION__, __LINE__, strerror(errno));
        CEV_capsuleClear(dst);
        readWriteErr++;
    }
}


static void L_capsuleDataTypeRead_RW(SDL_RWops* src, CEV_Capsule* dst)
{//fills and allocate dst->data 1.0

    if(IS_NULL(dst) || IS_NULL(dst))
    {
        fprintf(stderr, "Err at %s / %d :  NULL arg provided.\n", __FUNCTION__, __LINE__ );
        readWriteErr++;
        return;
    }

    dst->data = malloc(dst->size);

    if (IS_NULL(dst->data))
    {
        fprintf(stderr, "Err at %s / %d : %s.\n", __FUNCTION__, __LINE__, strerror(errno));
        readWriteErr++;
    }
    else if (SDL_RWread(src, dst->data, 1, dst->size) != dst->size)
    {
        fprintf(stderr, "Err at %s / %d : Unable to read data in src : %s.\n", __FUNCTION__, __LINE__, strerror(errno));
        CEV_capsuleClear(dst);
        readWriteErr++;
    }
}

