/*
   Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysys_priv.h"
#include "mysys_err.h"
#include <errno.h>


	/* Write a chunk of bytes to a file */

size_t my_write(int Filedes, const uchar *Buffer, size_t Count, myf MyFlags)
{
  size_t writenbytes, written;
  uint errors;
  DBUG_ENTER("my_write");
  DBUG_PRINT("my",("fd: %d  Buffer: 0x%lx  Count: %lu  MyFlags: %d",
		   Filedes, (long) Buffer, (ulong) Count, MyFlags));
  errors=0; written=0;

  /* The behavior of write(fd, buf, 0) is not portable */
  if (unlikely(!Count))
    DBUG_RETURN(0);
  
  for (;;)
  {
    writenbytes= write(Filedes, Buffer, Count);
    /**
       To simulate the write error set the errno = error code
       and the number pf written bytes to -1.
     */
    DBUG_EXECUTE_IF ("simulate_file_write_error",
                     if (!errors) {
                       errno= ENOSPC;
                       writenbytes= (size_t) -1;
                     });
    if (writenbytes == Count)
      break;
    if (writenbytes != (size_t) -1)
    {						/* Safeguard */
      written+=writenbytes;
      Buffer+=writenbytes;
      Count-=writenbytes;
    }
    my_errno=errno;
    DBUG_PRINT("error",("Write only %ld bytes, error: %d",
			(long) writenbytes, my_errno));
#ifndef NO_BACKGROUND
#ifdef THREAD
    if (my_thread_var->abort)
      MyFlags&= ~ MY_WAIT_IF_FULL;		/* End if aborted by user */
#endif
    if ((my_errno == ENOSPC || my_errno == EDQUOT) &&
        (MyFlags & MY_WAIT_IF_FULL))
    {
      wait_for_free_space(my_filename(Filedes), errors);
      errors++;
      continue;
    }

    if ((writenbytes == 0 || writenbytes == (size_t) -1))
    {
      if (my_errno == EINTR)
      {
        DBUG_PRINT("debug", ("my_write() was interrupted and returned %ld",
                             (long) writenbytes));
        continue;                               /* Interrupted */
      }

      if (!writenbytes && !errors++)		/* Retry once */
      {
        /* We may come here if the file quota is exeeded */
        errno=EFBIG;				/* Assume this is the error */
        continue;
      }
    }
    else
      continue;					/* Retry */
#endif
    if (MyFlags & (MY_NABP | MY_FNABP))
    {
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	my_error(EE_WRITE, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),my_errno);
      }
      DBUG_RETURN(MY_FILE_ERROR);		/* Error on read */
    }
    else
      break;					/* Return bytes written */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);			/* Want only errors */
  DBUG_RETURN(writenbytes+written);
} /* my_write */
