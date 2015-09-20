//
// Created by kdehairy on 9/19/15.
//

#include <stdio.h>
#include <stdlib.h>
#include "test_patch.h"
#include "bspatch.h"


static void initStream(PatchStream *stream, size_t variantSize) {

}

static size_t writeStream(PatchStream *stream, const void *buff, size_t size) {
    FILE *fvariant = (FILE *) stream->opaque;
    fwrite(buff, sizeof(uint8_t), size, fvariant);
    return size;
}

static void endStream(PatchStream *stream) {

}

void testPatch(const char *baseFile, const char *variantFile, const char *patchFile) {
    uint8_t *base = NULL;
    size_t baseSize;
    uint8_t *patch = NULL;
    size_t patchSize;

    off_t result;

    FILE *fbase = NULL;
    FILE *fpatch = NULL;
    FILE *fvariant = NULL;

    fbase = fopen(baseFile, "r");

    //get baseSize
    if (fseek(fbase, 0, SEEK_END) != 0) {
        goto fail;
    }
    result = ftello(fbase);
    if (result == -1) {
        goto fail;
    }
    baseSize = (size_t) result;

    // allocate base buffer
    base = (uint8_t *) malloc(baseSize + 1);
    if (fseek(fbase, 0, SEEK_SET) != 0) {
        goto fail;
    }
    fread(base, sizeof(uint8_t), baseSize, fbase);
    fclose(fbase);
    fbase = NULL;

    fpatch = fopen(patchFile, "r");

    // get patch size
    if (fseek(fpatch, 0, SEEK_END) != 0) {
        goto fail;
    }
    result = ftello(fpatch);
    if (result == -1) {
        goto fail;
    }
    patchSize = (size_t) result;

    // allocate patch buffer
    patch = (uint8_t *) malloc(patchSize);
    fseek(fpatch, 0, SEEK_SET);
    fread(patch, sizeof(uint8_t), patchSize, fpatch);
    fclose(fpatch);
    fpatch = NULL;

    fvariant = fopen(variantFile, "w");

    PatchStream stream;
    stream.write = writeStream;
    stream.init = initStream;
    stream.end = endStream;
    stream.opaque = fvariant;

    bspatch(base, baseSize, patch, patchSize, &stream);

    fclose(fvariant);
    fvariant = NULL;

    fail:
    if (fbase != NULL) {
        fclose(fbase);
    }
    if (base != NULL) {
        free(base);
    }
    if (patch != NULL) {
        free(patch);
    }
    if (fpatch != NULL) {
        fclose(fpatch);
    }
}