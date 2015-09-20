//
// Created by kdehairy on 4/11/15.
//

#ifndef LIBBSDIFF_BSDIFF_H
#define LIBBSDIFF_BSDIFF_H

#include <stdint.h>
#include <stddef.h>

typedef struct _DiffStream {
    void *opaque;
    size_t headerSize;

    /* Called before sending any data to the implementor.
     * At the time of this call, the diff header size is set and the
     * diff size is estimated */
    void (*init)( struct _DiffStream *stream, size_t diffSize );

    /* Marks the end of writing. No calls to write data or header after
     * this call. */
    void (*end)( struct _DiffStream *stream );

    /* Called once to write the header of the diff. It will always be called
     * after or before writing the data blocks. */
    size_t (*writeHeader)( struct _DiffStream *stream, const void *buff );

    /* Called to write diff data. Could be called multiple times with
     * subsequent blocks of data */
    size_t (*write)( struct _DiffStream *stream, const void *buff, size_t size );
} DiffStream;

/* returns the size of the diff if successful, zero otherwise */
size_t bsdiff( const uint8_t *base, size_t baseSize, const uint8_t *variant,
            size_t varSize, DiffStream *stream );

#endif //LIBBSDIFF_BSDIFF_H
