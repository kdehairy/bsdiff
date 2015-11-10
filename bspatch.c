#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <bzlib.h>
#include "bspatch.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"

static size_t offtin(const uint8_t *buf) {
    size_t y;

    y = (buf[7] & 0x7F);
    y = y * 256;
    y += buf[6];
    y = y * 256;
    y += buf[5];
    y = y * 256;
    y += buf[4];
    y = y * 256;
    y += buf[3];
    y = y * 256;
    y += buf[2];
    y = y * 256;
    y += buf[1];
    y = y * 256;
    y += buf[0];

    if (buf[7] & 0x80) y = -y;

    return y;
}

#pragma clang diagnostic pop

typedef struct _PatchHeader {
    const uint8_t magic[8];
    const uint8_t ctrlBlockSize[8];
    const uint8_t diffBlockSize[8];
    const uint8_t newFileLength[8];
} PatchHeader;

typedef struct _PatchFile {
    const PatchHeader *header;
    const uint8_t *ctrlBlock;
    const uint8_t *diffBlock;
    const uint8_t *extraBlock;
} PatchFile;


void bspatch(const uint8_t *base, size_t baseSize, const uint8_t *patch, size_t patchSize, PatchStream *stream) {

    int bzExtraRet;
    int bzCtrlRet;
    int bzDiffRet;
    unsigned int oldPos = 0;
    unsigned int newPos = 0;
    size_t bzExtraLen;
    size_t bzCtrlLen;
    size_t bzDataLen;
    size_t newSize;

    uint8_t buff[8 * 3] = {0};

    uint8_t *variant = NULL;
    PatchFile *patchFile = NULL;

    patchFile = (PatchFile *) malloc(sizeof(PatchFile));
    memset(patchFile, 0, sizeof(PatchFile));

    patchFile->header = (PatchHeader *) patch;
    if (memcmp(patchFile->header->magic, "BSDIFF40", 8) != 0) {
        goto fail;
    }
    bzCtrlLen = offtin(patchFile->header->ctrlBlockSize);
    fprintf(stdout, "ctrl block size: %zu\n", bzCtrlLen);
    bzDataLen = offtin(patchFile->header->diffBlockSize);
    fprintf(stdout, "diff block size: %zu\n", bzDataLen);
    newSize = offtin(patchFile->header->newFileLength);
    fprintf(stdout, "variant size: %zu\n", newSize);

    patchFile->ctrlBlock = patch + sizeof(PatchHeader);
    patchFile->diffBlock = patchFile->ctrlBlock + bzCtrlLen;
    patchFile->extraBlock = patchFile->diffBlock + bzDataLen;

    bzExtraLen = patchSize - (32 + bzCtrlLen + bzDataLen);
    fprintf(stdout, "extra block size: %zu\n", bzExtraLen);

    stream->init(stream, newSize);

    variant = (uint8_t *) malloc(newSize + 1);


    // prepare decompression for ctrl data
    size_t ctrl[3];
    bz_stream bzCtrlStream;
    bzCtrlStream.next_in = (char *) patchFile->ctrlBlock;
    bzCtrlStream.avail_in = bzCtrlLen;
    bzCtrlStream.next_out = (char *) buff;
    bzCtrlStream.avail_out = 8 * 3;
    bzCtrlStream.bzalloc = NULL;
    bzCtrlStream.bzfree = NULL;
    bzCtrlStream.opaque = NULL;
    bzCtrlRet = BZ2_bzDecompressInit(&bzCtrlStream, 0, 0);
    if (bzCtrlRet != BZ_OK) {
        fprintf(stderr, "BZ2_DecompressInit: failed to init decompression for ctrl\n");
        BZ2_bzCompressEnd(&bzCtrlStream);
        goto fail;
    }

    // prepare decompression for diff data
    bz_stream bzDiffStream;
    bzDiffStream.next_in = (char *) patchFile->diffBlock;
    bzDiffStream.avail_in = bzDataLen;
    bzDiffStream.bzalloc = NULL;
    bzDiffStream.bzfree = NULL;
    bzDiffRet = BZ2_bzDecompressInit(&bzDiffStream, 0, 0);
    if (bzDiffRet != BZ_OK) {
        fprintf(stderr, "BZ2_bzDecompressInit: failed to init decompression for diff\n");
        BZ2_bzCompressEnd(&bzDiffStream);
        goto fail;
    }

    // prepare decompression for extra data
    bz_stream bzExtraStream;
    bzExtraStream.next_in = (char *) patchFile->extraBlock;
    bzExtraStream.avail_in = bzExtraLen;
    bzExtraStream.bzalloc = NULL;
    bzExtraStream.bzfree = NULL;
    bzExtraRet = BZ2_bzDecompressInit(&bzExtraStream, 0, 0);
    if (bzExtraRet != BZ_OK) {
        fprintf(stderr, "BZ2_bzDecompressInit: failed to init decompression for extra\n");
        BZ2_bzCompressEnd(&bzExtraStream);
        goto fail;
    }

    while (newPos < newSize) {
        // decompress ctrl data
        bzCtrlStream.avail_out = 8 * 3;
        memset(buff, 0, 8 * 3);
        bzCtrlStream.next_out = (char *) buff;
        bzCtrlRet = BZ2_bzDecompress(&bzCtrlStream);
        if ((bzCtrlStream.avail_out > 0)
            || (bzCtrlRet != BZ_OK && bzCtrlRet != BZ_STREAM_END)) {
            BZ2_bzDecompressEnd(&bzCtrlStream);
            goto fail;
        }

        // read ctrl data
        for (int i = 0; i <= 2; ++i) {
            ctrl[i] = offtin(buff + (i * 8));
        }

        if (newPos + ctrl[0] > newSize) {
            fprintf(stderr, "Corrupted patch\n");
            BZ2_bzDecompressEnd(&bzCtrlStream);
            goto fail;
        }

        // decompress and read diff data
        bzDiffStream.next_out = (char *) (variant + newPos);
        bzDiffStream.avail_out = ctrl[0];
        bzDiffRet = BZ2_bzDecompress(&bzDiffStream);
        if ((bzDiffStream.avail_out > 0)
            || (bzDiffRet != BZ_OK && bzDiffRet != BZ_STREAM_END)) {
            fprintf(stderr, "BZ2_Decompress: failed to decompress data: errCode %d\n", bzDiffRet);
            BZ2_bzDecompressEnd(&bzCtrlStream);
            BZ2_bzDecompressEnd(&bzDiffStream);
            goto fail;
        }

        for (unsigned int i = 0; i < ctrl[0]; ++i) {
            if ((oldPos + i >= 0) && (oldPos + i < baseSize)) {
                variant[newPos + i] += base[oldPos + i];
            }
        }

        // Adjust pointers
        newPos += ctrl[0];
        oldPos += ctrl[0];

        if (newPos + ctrl[1] > newSize) {
            fprintf(stderr, "Corrupt patch\n");
            BZ2_bzDecompressEnd(&bzCtrlStream);
            BZ2_bzDecompressEnd(&bzDiffStream);
            goto fail;
        }

        // decompress and read extra data
        bzExtraStream.next_out = (char *) (variant + newPos);
        bzExtraStream.avail_out = ctrl[1];
        bzExtraRet = BZ2_bzDecompress(&bzExtraStream);
        if ((bzExtraStream.avail_out > 0)
            || (bzExtraRet != BZ_OK && bzExtraRet != BZ_STREAM_END)) {
            fprintf(stderr, "BZ2_bzDecompress: failed to decompress data: errCode %d\n", bzExtraRet);
            BZ2_bzDecompressEnd(&bzCtrlStream);
            BZ2_bzDecompressEnd(&bzDiffStream);
            BZ2_bzDecompressEnd(&bzExtraStream);
            goto fail;
        }

        newPos += ctrl[1];
        oldPos += ctrl[2];
    }

    BZ2_bzDecompressEnd(&bzCtrlStream);
    BZ2_bzDecompressEnd(&bzDiffStream);
    BZ2_bzDecompressEnd(&bzExtraStream);

    stream->write(stream, variant, newSize);
    stream->end(stream);


    fail:
    free(patchFile);
    if (variant != NULL) {
        free(variant);
    }
}
