//
// Created by kdehairy on 9/19/15.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "test_patch.h"
#include "bspatch.h"


static void initStream( PatchStream *stream, size_t variantSize ) {

}

static size_t writeStream( PatchStream *stream, const void *buff,
		size_t size ) {
	FILE *fvariant = (FILE *) stream->opaque;
	fwrite( buff, sizeof( uint8_t ), size, fvariant );
	return size;
}

static void endStream( PatchStream *stream ) {

}

static int getFileStartLength( int fd, off_t *start, size_t *length ) {
	off_t start_, end_;
	size_t length_;

	start_ = lseek( fd, 0L, SEEK_CUR );
	end_ = lseek( fd, 0L, SEEK_END );
	(void) lseek( fd, start_, SEEK_SET );

	if ( start_ == (off_t) -1 || end_ == (off_t) -1 ) {
		return -1;
	}

	length_ = end_ - start_;
	if ( length_ == 0 ) {
		return -1;
	}

	*start = start_;
	*length = length_;

	return 0;
}

void testPatch( const char *baseFile, const char *variantFile,
		const char *patchFile ) {
	uint8_t *base = NULL;
	size_t baseSize;
	uint8_t *patch = NULL;
	size_t patchSize;

	FILE *fvariant = NULL;

	int base_fd = open( baseFile, O_RDONLY );
	if ( base_fd < 0 ) {
		goto fail;
	}

	//mmap the base file
	off_t base_start;
	if ( getFileStartLength( base_fd, &base_start, &baseSize )) {
		goto fail;
	}
	base = mmap(NULL, baseSize, PROT_READ, MAP_PRIVATE, base_fd, base_start );
	if ( base == MAP_FAILED) {
		goto fail;
	}

	int patch_fd = open( patchFile, O_RDONLY );
	if ( patch_fd < 0 ) {
		fprintf( stderr, "Failed to open patch file\n" );
		goto fail;
	}

	//mmap the patch file
	off_t patch_start;
	if ( getFileStartLength( patch_fd, &patch_start, &patchSize )) {
		goto fail;
	}
	patch = mmap(NULL, patchSize, PROT_READ, MAP_PRIVATE, patch_fd,
			patch_start );
	if ( patch == MAP_FAILED) {
		goto fail;
	}

	fvariant = fopen( variantFile, "w" );

	PatchStream stream;
	stream.write = writeStream;
	stream.init = initStream;
	stream.end = endStream;
	stream.opaque = fvariant;

	bspatch( base, baseSize, patch, patchSize, &stream );

	fclose( fvariant );

	fail:
	if ( base_fd >= 0 ) {
		close( base_fd );
	}
	if ( base != NULL) {
		munmap( base, baseSize );
	}

	if ( patch != NULL) {
		munmap( patch, patchSize );
	}
	if ( patch_fd >= 0 ) {
		close( patch_fd );
	}
}