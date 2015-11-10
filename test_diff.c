//
// Created by kdehairy on 9/19/15.
//

#include "bsdiff.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "test_diff.h"

static void initStream(DiffStream *stream, size_t buffSize) {
    FILE *fpatch = (struct _IO_FILE *) stream->opaque;
    void *buff = malloc(stream->headerSize);
    memset(buff, 0, stream->headerSize);
    fwrite(buff, sizeof(uint8_t), stream->headerSize, fpatch);
    free(buff);
}

static size_t writeStream(DiffStream *stream, const void *data, size_t size) {
    FILE *fpatch = (struct _IO_FILE *) stream->opaque;
    fwrite(data, sizeof(uint8_t), size, fpatch);
    return size;
}

static size_t writeHeaderStream(DiffStream *stream, const void *data) {
    FILE *fpatch = (FILE *) stream->opaque;
    off_t oldPos = ftello(fpatch);
    fseek(fpatch, 0, SEEK_SET);
    fwrite(data, sizeof(uint8_t), stream->headerSize, fpatch);
    fseek(fpatch, oldPos, SEEK_SET);
    return stream->headerSize;
}

static void endStream(DiffStream *stream) {

}

void testDiff(const char *baseFile, const char *variantFile, const char *patchFile) {
    uint8_t *base = NULL;
    size_t baseSize;
    uint8_t *variant = NULL;
    size_t variantSize;

    off_t result;

    FILE *fbase = NULL;
    FILE *fvariant = NULL;
    FILE *fpatch = NULL;

    fbase = fopen(baseFile, "r");

    if (fbase == NULL) {
        fprintf(stderr, "Failed to open base file\n");
        return;
    }

    fvariant = fopen(variantFile, "r");

    if (fvariant == NULL) {
        fprintf(stderr, "Failed to open variant file\n");
        return;
    }

    // get base size
    if (fseek(fbase, 0, SEEK_END) != 0) {
        goto fail;
    }
    result = ftello(fbase);
    if (result == -1) {
        goto fail;
    }
    baseSize = (size_t) result;
    fprintf(stdout, "baseSize: %zu\n", baseSize);

    //allocate base buffer
    base = (uint8_t *) malloc(baseSize + 1);
    if (fseek(fbase, 0, SEEK_SET) != 0) {
        goto fail;
    }
    fread(base, sizeof(uint8_t), baseSize, fbase);
    fclose(fbase);
    fbase = NULL;

    //get variant size
    if (fseek(fvariant, 0, SEEK_END) != 0) {
        goto fail;
    }
    result = ftello(fvariant);
    if (result == -1) {
        goto fail;
    }
    variantSize = (size_t) result;
    fprintf(stdout, "variantSize: %zu\n", variantSize);

    // allocate variant buffer
    variant = (uint8_t*) malloc(variantSize + 1);
    fseek(fvariant, 0, SEEK_SET);
    if (fread(variant, sizeof(uint8_t), variantSize, fvariant) != variantSize) {
        goto fail;
    }
    fclose(fvariant);
    fvariant = NULL;

    fpatch = fopen(patchFile, "w");

    DiffStream stream;
    stream.write = writeStream;
    stream.writeHeader = writeHeaderStream;
    stream.init = initStream;
    stream.end = endStream;
    stream.opaque = fpatch;

    fprintf(stdout, "starting generating diff...\n");

    bsdiff(base, baseSize, variant, variantSize, &stream);

    fprintf(stdout, "patch file generated\n");

    fclose(fpatch);


    fail:
    if (fbase != NULL) {
        fclose(fbase);
    }
    if (fvariant != NULL) {
        fclose(fvariant);
    }
    if (base != NULL) {
        free(base);
    }
    if (variant != NULL) {
        free(variant);
    }
}