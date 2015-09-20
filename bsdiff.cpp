/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sais/sais.h"
#include <bzlib.h>
#include "bsdiff.h"

#define MIN( x, y ) (((x)<(y)) ? (x) : (y))

static void offtout( off_t x, uint8_t *buf ) {
    off_t y = (x < 0) ? -x : x;

    buf[0] = y % 256;
    y -= buf[0];
    y = y / 256;
    buf[1] = y % 256;
    y -= buf[1];
    y = y / 256;
    buf[2] = y % 256;
    y -= buf[2];
    y = y / 256;
    buf[3] = y % 256;
    y -= buf[3];
    y = y / 256;
    buf[4] = y % 256;
    y -= buf[4];
    y = y / 256;
    buf[5] = y % 256;
    y -= buf[5];
    y = y / 256;
    buf[6] = y % 256;
    y -= buf[6];
    y = y / 256;
    buf[7] = y % 256;

    if ( x < 0 ) buf[7] |= 0x80;
}

static off_t matchlen( const uint8_t *base, off_t baseSize,
                       const uint8_t *variant, off_t varSize ) {
    off_t i;

    for ( i = 0; (i < baseSize) && (i < varSize); i++ )
        if ( base[i] != variant[i] ) break;

    return i;
}

static off_t search( off_t *I, const uint8_t *base, off_t baseSize,
                     const uint8_t *variant, off_t varSize, off_t st, off_t en,
                     off_t *pos ) {
    off_t x, y;

    if ( en - st < 2 ) {
        x = matchlen( base + I[st], baseSize - I[st], variant, varSize );
        y = matchlen( base + I[en], baseSize - I[en], variant, varSize );

        if ( x > y ) {
            *pos = I[st];
            return x;
        } else {
            *pos = I[en];
            return y;
        }
    };

    x = st + (en - st) / 2;
    if ( memcmp( base + I[x], variant, MIN( baseSize - I[x], varSize )) < 0 ) {
        return search( I, base, baseSize, variant, varSize, x, en, pos );
    } else {
        return search( I, base, baseSize, variant, varSize, st, x, pos );
    };
}

static size_t writeHeader( DiffStream *pStream, uint8_t *buff ) {
    return pStream->writeHeader( pStream, buff );
}

/* Returns the amount of data written */
static size_t writeDataBlock( DiffStream *pStream, uint8_t *buff,
                              size_t size ) {
    size_t bzBuffSize = (size_t) ((1.01 * (float)size) + 600.0F);
    uint8_t *bzBuff = (uint8_t *) malloc( bzBuffSize );
    bz_stream bzStream;
    bzStream.next_in = (char *) buff;
    bzStream.avail_in = size;
    bzStream.next_out = (char *) bzBuff;
    bzStream.avail_out = bzBuffSize;
    bzStream.bzalloc = NULL;
    bzStream.bzfree = NULL;
    bzStream.opaque = NULL;

    int bzret = 0;
    bzret = BZ2_bzCompressInit( &bzStream, 9, 0, 0 );
    if ( bzret != BZ_OK ) {
        fprintf( stderr, "BZ2_bzCompressInit: failed to init compression\n" );
        BZ2_bzCompressEnd( &bzStream );
        free( bzBuff );
        return 0;
    }
    do {
        bzret = BZ2_bzCompress( &bzStream, BZ_FINISH );
        if ( bzret != BZ_RUN_OK && bzret != BZ_FINISH_OK &&
             bzret != BZ_STREAM_END ) {
            fprintf( stderr,
                     "BZ2_bzCompress: failed to compress data: errCode %d\n",
                     bzret );
            BZ2_bzCompressEnd( &bzStream );
            free( bzBuff );
            return 0;
        }
    } while ( bzret != BZ_STREAM_END );
    size_t diffCompressedSize = bzStream.total_out_hi32;
    diffCompressedSize <<= 32;
    diffCompressedSize += bzStream.total_out_lo32;

    BZ2_bzCompressEnd( &bzStream );

    size_t count = pStream->write( pStream, bzBuff, diffCompressedSize );

    free( bzBuff );

    return count;
}

size_t bsdiff( const uint8_t *base, size_t baseSize, const uint8_t *variant,
               size_t varSize, DiffStream *stream ) {
    off_t *I;
    off_t scan, pos, len;
    off_t lastscan, lastpos, lastoffset;
    off_t oldscore, scsc;
    off_t s, Sf, lenf, Sb, lenb;
    off_t overlap, Ss, lens;
    off_t i;
    size_t ctrlDataSize, diffDataSize, extraDataSize;
    size_t ctrlBuffSize, diffBuffSize, extraBuffSize;
    size_t bzCtrlBuffSize, bzDiffBuffSize, bzExtraBuffSize;
    uint8_t *ctrlBuff = NULL;
    uint8_t *diffBuff = NULL;
    uint8_t *extraBuff = NULL;
    uint8_t header[32];

    size_t estimatedSize;
    size_t patchSize;

    I = (off_t *) malloc((baseSize + 1) * sizeof( off_t ));
    I[0] = baseSize;
    sais( base, I + 1, baseSize );

    /*
     * Assuming that the diff and extra buffers will never exceed the variant size.
     * For the ctrl buffer it may exceed (specially for very small files). It will be extended
     * later if an overflow is detected.
     */
    ctrlBuffSize = diffBuffSize = extraBuffSize = varSize;

    diffBuff = (uint8_t *) malloc(diffBuffSize);
    extraBuff = (uint8_t *) malloc(extraBuffSize);
    ctrlBuff = (uint8_t *) malloc(ctrlBuffSize);

    memset( ctrlBuff, 0, ctrlBuffSize);
    memset( diffBuff, 0, diffBuffSize);
    memset( extraBuff, 0, extraBuffSize);

    ctrlDataSize = 0;
    diffDataSize = 0;
    extraDataSize = 0;


    /* Header is
        0	8	 "BSDIFF40"
        8	8	length of bzip2ed ctrl block
        16	8	length of bzip2ed diff block
        24	8	length of new file */
    /* File is
        0	32	Header
        32	??	Bzip2ed ctrl block
        ??	??	Bzip2ed diff block
        ??	??	Bzip2ed extra block */
    memcpy( header, "BSDIFF40", 8 );
    offtout( 0, header + 8 );
    offtout( 0, header + 16 );
    offtout( varSize, header + 24 );

    scan = 0;
    len = 0;
    lastscan = 0;
    lastpos = 0;
    lastoffset = 0;
    while ( scan < varSize ) {
        oldscore = 0;

        for ( scsc = scan += len; scan < varSize; scan++ ) {
            len = search( I, base, baseSize, variant + scan, varSize - scan,
                          0, baseSize, &pos );

            for ( ; scsc < scan + len; scsc++ )
                if ((scsc + lastoffset < baseSize) &&
                    (base[scsc + lastoffset] == variant[scsc]))
                    oldscore++;

            if (((len == oldscore) && (len != 0)) ||
                (len > oldscore + 8))
                break;

            if ((scan + lastoffset < baseSize) &&
                (base[scan + lastoffset] == variant[scan]))
                oldscore--;
        };

        if ((len != oldscore) || (scan == varSize)) {
            s = 0;
            Sf = 0;
            lenf = 0;
            for ( i = 0; (lastscan + i < scan) && (lastpos + i < baseSize); ) {
                if ( base[lastpos + i] == variant[lastscan + i] ) s++;
                i++;
                if ( s * 2 - i > Sf * 2 - lenf ) {
                    Sf = s;
                    lenf = i;
                };
            };

            lenb = 0;
            if ( scan < varSize ) {
                s = 0;
                Sb = 0;
                for ( i = 1; (scan >= lastscan + i) && (pos >= i); i++ ) {
                    if ( base[pos - i] == variant[scan - i] ) s++;
                    if ( s * 2 - i > Sb * 2 - lenb ) {
                        Sb = s;
                        lenb = i;
                    };
                };
            };

            if ( lastscan + lenf > scan - lenb ) {
                overlap = (lastscan + lenf) - (scan - lenb);
                s = 0;
                Ss = 0;
                lens = 0;
                for ( i = 0; i < overlap; i++ ) {
                    if ( variant[lastscan + lenf - overlap + i] ==
                         base[lastpos + lenf - overlap + i] )
                        s++;
                    if ( variant[scan - lenb + i] ==
                         base[pos - lenb + i] )
                        s--;
                    if ( s > Ss ) {
                        Ss = s;
                        lens = i + 1;
                    };
                };

                lenf += lens - overlap;
                lenb -= lens;
            };

            for ( i = 0; i < lenf; i++ )
                diffBuff[diffDataSize + i] =
                        variant[lastscan + i] - base[lastpos + i];
            for ( i = 0; i < (scan - lenb) - (lastscan + lenf); i++ )
                extraBuff[extraDataSize + i] = variant[lastscan + lenf + i];

            diffDataSize += lenf;
            extraDataSize += (scan - lenb) - (lastscan + lenf);

            //append to ctrl buffer
            uint8_t buff[8 * 3];
            offtout( lenf, buff );
            offtout((scan - lenb) - (lastscan + lenf), buff + 8 );
            offtout((pos - lenb) - (lastpos + lenf), buff + 16 );
            if (ctrlDataSize + 8 * 3 > ctrlBuffSize) {
                fprintf(stdout, "stackoverflow in ctrl buffer detected.\n");
                fprintf(stdout, "extending the ctrl data buffer\n");
                uint8_t *oldBuff = ctrlBuff;
                ctrlBuff = (uint8_t *) malloc(ctrlBuffSize * 2);
                memcpy(ctrlBuff, oldBuff, ctrlBuffSize);
                fprintf(stdout, "old buffer size: %zu, new size: %zu\n", ctrlBuffSize, ctrlBuffSize * 2);
                ctrlBuffSize *= 2;
                free(oldBuff);
            }
            memcpy( ctrlBuff + ctrlDataSize, buff, 8 * 3 );
            ctrlDataSize += 8 * 3;

            lastscan = scan - lenb;
            lastpos = pos - lenb;
            lastoffset = pos - scan;
        };
    };

    // set the value for the header size
    stream->headerSize = sizeof( header );

    estimatedSize = stream->headerSize + ctrlDataSize + diffDataSize + extraDataSize;

    stream->init( stream, estimatedSize );

    // write ctrl buffer
    fprintf(stdout, "writing ctrl buffer: size: %zu\n", ctrlDataSize);
    bzCtrlBuffSize = writeDataBlock( stream, ctrlBuff, ctrlDataSize);
    if ( bzCtrlBuffSize == 0 ) {
        fprintf( stderr, "writeDataBlock: ctrl buffer failed\n" );
        goto fail;
    }

    // write the diff buffer
    fprintf(stdout, "writing diff buffer\n");
    bzDiffBuffSize = writeDataBlock( stream, diffBuff, diffDataSize);
    if ( bzDiffBuffSize == 0 ) {
        fprintf( stderr, "writeDataBlock: diff buffer failed\n" );
        goto fail;
    }

    // write the extra data
    fprintf(stdout, "writing extra buffer");
    bzExtraBuffSize = writeDataBlock( stream, extraBuff, extraDataSize);
    if ( bzExtraBuffSize == 0 ) {
        fprintf( stderr, "writeDataBlock: extra buffer failed\n" );
        goto fail;
    }

    // update header with ctrl buffer length
    offtout( bzCtrlBuffSize, header + 8 );

    // update the header with diff buffer length
    offtout( bzDiffBuffSize, header + 16 );

    if ( writeHeader( stream, header ) == 0 ) {
        fprintf( stderr, "writeHeader: header failed\n" );
        goto fail;
    }

    fprintf( stdout, "bsdiff header: %zu\tctrl: %zu\tdiff: %zu\textra: %zu\n",
             stream->headerSize, bzCtrlBuffSize, bzDiffBuffSize, bzExtraBuffSize );
    stream->end( stream );

    free( ctrlBuff );
    free( diffBuff );
    free( extraBuff );
    free( I );

    patchSize = stream->headerSize + bzCtrlBuffSize + bzDiffBuffSize + bzExtraBuffSize;

    fprintf(stdout, "patch size: %zu\n", patchSize);

    return patchSize;

    fail:
    if ( ctrlBuff != NULL ) {
        free( ctrlBuff );
    }
    if ( diffBuff != NULL ) {
        free( diffBuff );
    }
    if ( extraBuff != NULL ) {
        free( extraBuff );
    }
    if ( I != NULL ) {
        free( I );
    }
    return 0;

}
