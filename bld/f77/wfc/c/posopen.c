/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2020 The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  POSIX level i/o support
*
****************************************************************************/


#include "ftnstd.h"
#if defined( __NT__ )
    #include <windows.h>
#endif
#include "posio.h"
#include "sopen.h"
#include "posopen.h"
#include "posput.h"
#include "poserr.h"
#include "posflush.h"
#include "iomode.h"
#include "posdat.h"
#include "fmemmgr.h"

#include "clibext.h"


static  int     IOBufferSize = { IO_BUFFER };

void    InitStd( void )
{
// Initialize standard i/o.
#if !defined( __UNIX__ ) && !defined( __NETWARE__ ) && defined( __WATCOMC__ )
    // don't call setmode() since we don't want to affect higher level
    // i/o so that if C function gets called, printf() works ok
    __set_binary( STDOUT_FILENO );
#endif
}

void    SetIOBufferSize( uint buff_size )
{
    if( buff_size < MIN_BUFFER ) {
        buff_size = MIN_BUFFER;
    }
    IOBufferSize = buff_size;
}

static b_file  *_AllocFile( int h, f_attrs attrs )
// Allocate file structure.
{
    b_file      *io;
    struct stat info;
    int         buff_size;

    if( fstat( h, &info ) == -1 ) {
        FSetSysErr( NULL );
        return( NULL );
    }
    attrs &= ~CREATION_MASK;
    if( S_ISCHR( info.st_mode ) ) {
        io = FMemAlloc( offsetof( b_file, read_len ) );
        // Turn off truncate just in case we turned it on by accident due to
        // a buggy NT dos box.  We NEVER want to truncate a device.
//        attrs &= ~TRUNC_ON_WRITE;
//        attrs |= CHAR_DEVICE;
    } else {
        attrs |= BUFFERED;
        buff_size = IOBufferSize;
        io = FMemAlloc( sizeof( b_file ) + IOBufferSize - MIN_BUFFER );
        if( ( io == NULL ) && ( IOBufferSize > MIN_BUFFER ) ) {
            // buffer is too big (low on memory) so use small buffer
            buff_size = MIN_BUFFER;
            io = FMemAlloc( sizeof( b_file ) );
        }
    }
    if( io == NULL ) {
        close( h );
        FSetErr( POSIO_NO_MEM, NULL );
    } else {
        io->attrs = attrs;
        io->handle = h;
        io->phys_offset = 0;
        if( attrs & BUFFERED ) {
            io->b_curs = 0;
            io->read_len = 0;
            io->buff_size = buff_size;
            io->high_water = 0;
        }
        FSetIOOk( io );
    }
    return( io );
}

b_file  *Openf( const char *f, f_attrs attrs )
// Open a file.
{
    int         h;
#if defined( __WATCOMC__ ) || !defined( __UNIX__ )
    int         share;

    share = SH_COMPAT;
    if( attrs & S_DENYRW ) {
        share = SH_DENYRW;
    } else if( attrs & S_DENYWR ) {
        share = SH_DENYWR;
    } else if( attrs & S_DENYRD ) {
        share = SH_DENYRD;
    } else if( attrs & S_DENYNO ) {
        share = SH_DENYNO;
    }
#endif
    if( attrs & WRONLY ) {
        attrs |= WRITE_ONLY;
        h = sopen4( f, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, share, PMODE_RW );
    } else if( attrs & RDONLY ) {
        h = sopen3( f, O_RDONLY | O_BINARY, share );
    } else { // if( attrs & RDWR ) {
        h = sopen4( f, O_RDWR | O_BINARY | O_CREAT, share, PMODE_RW );
    }
    if( h < 0 ) {
        FSetSysErr( NULL );
        return( NULL );
    }
    return( _AllocFile( h, attrs ) );
}

void    Closef( b_file *io )
// Close a file.
{
    if( FlushBuffer( io ) < 0 )
        return;
    if( close( io->handle ) < 0 ) {
        FSetSysErr( io );
        return;
    }
    FMemFree( io );
    FSetIOOk( NULL );
}