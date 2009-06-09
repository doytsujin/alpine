/*
 * $Id: mimetype.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_MIMETYPE_INCLUDED
#define PITH_MIMETYPE_INCLUDED


/*
 * Struct passed mime.types search functions
 */
typedef struct {
    union {
	char	 *ext;
	char	 *mime_type;
    } from;
    union {
	struct {
	    int	  type;
	    char *subtype;
	} mime;
	char	 *ext;
    } to;
} MT_MAP_T;


/* exported protoypes */
int	    set_mime_type_by_extension(BODY *, char *);
int	    set_mime_extension_by_type(char *, char *);
int         mt_srch_by_ext(MT_MAP_T *, FILE *);


#endif /* PITH_MIMETYPE_INCLUDED */