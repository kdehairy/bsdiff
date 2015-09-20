//
// Created by kdehairy on 9/15/15.
//

#include <stdint.h>
#include <stddef.h>

#ifndef DEXPATCHER_BSPATCH_H
#define DEXPATCHER_BSPATCH_H

typedef struct _PatchStream {
    void *opaque;

    void (*init)( struct _PatchStream *stream, size_t variantSize);

    size_t (*write)( struct _PatchStream *stream, const void *buff, size_t size );

    void (*end)( struct _PatchStream *stream );
} PatchStream;

void bspatch( const uint8_t *base, size_t baseSize, const uint8_t *patch, size_t patchSize, PatchStream *stream );

#endif //DEXPATCHER_BSPATCH_H

