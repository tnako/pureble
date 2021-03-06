/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#ifndef HAVE_VIO /* is Vio suppored by the Vio lib ? */

#include <my_global.h>
#include <errno.h>
#include <assert.h>
#include <violite.h>
#include <my_sys.h>
#include <my_net.h>
#include <m_string.h>
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_OPENSSL
#include <ma_secure.h>
#endif

#ifdef _WIN32
#define socklen_t int
#pragma comment (lib, "ws2_32")
#endif

#if !defined(MSDOS) && !defined(_WIN32) && !defined(HAVE_BROKEN_NETINET_INCLUDES) && !defined(__BEOS__) && !defined(__FreeBSD__)
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#if !defined(alpha_linux_port)
#include <netinet/tcp.h>
#endif
#endif

#if defined(__EMX__) || defined(OS2)
#define ioctlsocket ioctl
#endif /* defined(__EMX__) */

#if defined(MSDOS) || defined(_WIN32)
#define O_NONBLOCK 1    /* For emulation of fcntl() */
#endif
#ifndef EWOULDBLOCK
#define SOCKET_EWOULDBLOCK SOCKET_EAGAIN
#endif


typedef void *vio_ptr;
typedef char *vio_cstring;

/*
 * Helper to fill most of the Vio* with defaults.
 */

void vio_reset(Vio* vio, enum enum_vio_type type,
               my_socket sd, HANDLE hPipe,
               my_bool localhost)
{
  uchar *save_cache= vio->cache;
  bzero((char*) vio, sizeof(*vio));
  vio->type= type;
  vio->sd= sd;
  vio->hPipe= hPipe;
  vio->localhost= localhost;
  /* do not clear cache */
  vio->cache= vio->cache_pos= save_cache;
  vio->cache_size= 0;
}

void vio_timeout(Vio *vio, int type, uint seconds)
{
#ifdef _WIN32
  uint timeout= seconds * 1000; /* milli secs */
#else
  struct timeval timeout;
  timeout.tv_sec= seconds;
  timeout.tv_usec= 0;
#endif

  if (setsockopt(vio->sd, SOL_SOCKET, type,
#ifdef _WIN32
                (const char *)&timeout,
#else
                (const void *)&timeout,
#endif
                sizeof(timeout)))
  {
    DBUG_PRINT("error", ("setsockopt failed. Errno: %d", errno));
  }
}

void vio_read_timeout(Vio *vio, uint seconds)
{
  vio_timeout(vio, SO_RCVTIMEO, seconds);
}

void vio_write_timeout(Vio *vio, uint seconds)
{
  vio_timeout(vio, SO_SNDTIMEO, seconds);
}

/* Open the socket or TCP/IP connection and read the fnctl() status */

Vio *vio_new(my_socket sd, enum enum_vio_type type, my_bool localhost)
{
  Vio *vio;
  DBUG_ENTER("vio_new");
  DBUG_PRINT("enter", ("sd=%d", sd));
  if ((vio = (Vio*) my_malloc(sizeof(*vio),MYF(MY_WME))))
  {
    vio_reset(vio, type, sd, 0, localhost);
    sprintf(vio->desc,
            (vio->type == VIO_TYPE_SOCKET ? "socket (%d)" : "TCP/IP (%d)"),
             vio->sd);
#if !defined(__WIN32) && !defined(__EMX__) && !defined(OS2)
#if !defined(NO_FCNTL_NONBLOCK)
    vio->fcntl_mode = fcntl(sd, F_GETFL);
#elif defined(HAVE_SYS_IOCTL_H) /* hpux */
    /* Non blocking sockets doesn't work good on HPUX 11.0 */
    (void) ioctl(sd,FIOSNBIO,0);
#endif
#else /* !defined(_WIN32) && !defined(__EMX__) */
    {
      /* set to blocking mode by default */
      ulong arg=0, r;
      r = ioctlsocket(vio->sd,FIONBIO,(void*) &arg/*, sizeof(arg)*/);
    }
#endif
  }
  if (!(vio->cache= my_malloc(VIO_CACHE_SIZE, MYF(MY_WME))))
  {
    my_free((gptr)vio, MYF(0));
    vio= NULL;
  }
  vio->cache_size= 0;
  vio->cache_pos= vio->cache;
  DBUG_RETURN(vio);
}


#ifdef _WIN32

Vio *vio_new_win32pipe(HANDLE hPipe)
{
  Vio *vio;
  DBUG_ENTER("vio_new_handle");
  if ((vio = (Vio*) my_malloc(sizeof(Vio),MYF(MY_WME))))
  {
    vio_reset(vio, VIO_TYPE_NAMEDPIPE, 0, hPipe, TRUE);
    strmov(vio->desc, "named pipe");
  }
  DBUG_RETURN(vio);
}

#endif

void vio_delete(Vio * vio)
{
  /* It must be safe to delete null pointers. */
  /* This matches the semantics of C++'s delete operator. */
  if (vio)
  {
    if (vio->type != VIO_CLOSED)
      vio_close(vio);
    my_free((gptr) vio->cache, MYF(0));
    my_free((gptr) vio,MYF(0));
  }
}

int vio_errno(Vio *vio __attribute__((unused)))
{
  return socket_errno; /* On Win32 this mapped to WSAGetLastError() */
}

size_t vio_real_read(Vio *vio, gptr buf, size_t size)
{
  switch(vio->type) {
#ifdef HAVE_OPENSSL
  case VIO_TYPE_SSL:
    return my_ssl_read(vio, (char *)buf, size);
    break;
#endif
#ifdef _WIN32
  case VIO_TYPE_NAMEDPIPE:
    {
      DWORD length= 0;
      if (!ReadFile(vio->hPipe, buf, (DWORD)size, &length, NULL))
        return -1;
      return length;
    }
    break;
#endif
  default:
    return recv(vio->sd, buf,
#ifdef _WIN32
           (int)
#endif 
                size, 0);
    break;
  }
}


size_t vio_read(Vio * vio, gptr buf, size_t size)
{
  size_t r;
  DBUG_ENTER("vio_read");
  DBUG_PRINT("enter", ("sd=%d  size=%d", vio->sd, size));

  if (vio->cache + vio->cache_size > vio->cache_pos)
  {
    r= MIN(size, vio->cache + vio->cache_size - vio->cache_pos);
    memcpy(buf, vio->cache_pos, r);
    vio->cache_pos+= r;
  }
  else if (size >= VIO_CACHE_MIN_SIZE)
  {
    r= vio_real_read(vio, buf, size); 
  }
  else
  {
    r= vio_real_read(vio, vio->cache, VIO_CACHE_SIZE);
    if (r > 0)
    {
      if (size < r)
      {
        vio->cache_size= r; /* might be < VIO_CACHE_SIZE */
        vio->cache_pos= vio->cache + size;
        r= size;
      }
      memcpy(buf, vio->cache, r);
    }
  } 

#ifndef DBUG_OFF
  if ((size_t)r == -1)
  {
    DBUG_PRINT("vio_error", ("Got error %d during read",socket_errno));
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%u", (uint)r));
  DBUG_RETURN(r);
}

/*
 Return data from the beginning of the receive queue without removing 
 that data from the queue. A subsequent receive call will return the same data.
*/
my_bool vio_read_peek(Vio *vio, size_t *bytes)
{
#ifdef _WIN32
  if (ioctlsocket(vio->sd, FIONREAD, (unsigned long*)bytes))
    return TRUE;
#else
  char buffer[1024];
  ssize_t length;

  vio_blocking(vio, 0);
  length= recv(vio->sd, &buffer, sizeof(buffer), MSG_PEEK);
  if (length < 0)
    return TRUE;
  *bytes= length; 
#endif 
  return FALSE;
}


size_t vio_write(Vio * vio, const gptr buf, size_t size)
{
  size_t r;
  DBUG_ENTER("vio_write");
  DBUG_PRINT("enter", ("sd=%d  size=%d", vio->sd, size));
#ifdef HAVE_OPENSSL
  if (vio->type == VIO_TYPE_SSL)
  {
    r= my_ssl_write(vio, (uchar *)buf, size);
    DBUG_RETURN(r); 
  }
#endif
#if defined( _WIN32) || defined(OS2)
  if ( vio->type == VIO_TYPE_NAMEDPIPE)
  {
    DWORD length;
#ifdef OS2
    if (!DosWrite((HFILE)vio->hPipe, (char*) buf, size, &length))
      DBUG_RETURN(-1);
#else
    if (!WriteFile(vio->hPipe, (char*) buf, (DWORD)size, &length, NULL))
      DBUG_RETURN(-1);
#endif
    DBUG_RETURN(length);
  }
  r = send(vio->sd, buf, (int)size,0);
#else
  r = write(vio->sd, buf, size);
#endif /* _WIN32 */
#ifndef DBUG_OFF
  if ((size_t)r == -1)
  {
    DBUG_PRINT("vio_error", ("Got error on write: %d",socket_errno));
  }
#endif /* DBUG_OFF */
  DBUG_PRINT("exit", ("%u", (uint)r));
  DBUG_RETURN(r);
}


int vio_blocking(Vio * vio, my_bool set_blocking_mode)
{
  int r=0;
  DBUG_ENTER("vio_blocking");
  DBUG_PRINT("enter", ("set_blocking_mode: %d", (int) set_blocking_mode));

#if !defined(__WIN32) && !defined(__EMX__) && !defined(OS2)
#if !defined(NO_FCNTL_NONBLOCK)

  if (vio->sd >= 0)
  {
    int old_fcntl=vio->fcntl_mode;
    if (set_blocking_mode)
      vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    else
      vio->fcntl_mode |= O_NONBLOCK; /* set bit */
    if (old_fcntl != vio->fcntl_mode)
      r = fcntl(vio->sd, F_SETFL, vio->fcntl_mode);
  }
#endif /* !defined(NO_FCNTL_NONBLOCK) */
#else /* !defined(_WIN32) && !defined(__EMX__) */
#ifndef __EMX__
  if (vio->type != VIO_TYPE_NAMEDPIPE)  
#endif
  { 
    ulong arg;
    int old_fcntl=vio->fcntl_mode;
    if (set_blocking_mode)
    {
      arg = 0;
      vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    }
    else
    {
      arg = 1;
      vio->fcntl_mode |= O_NONBLOCK; /* set bit */
    }
    if (old_fcntl != vio->fcntl_mode)
      r = ioctlsocket(vio->sd,FIONBIO,(void*) &arg);
  }
#endif /* !defined(_WIN32) && !defined(__EMX__) */
  DBUG_RETURN(r);
}

my_bool
vio_is_blocking(Vio * vio)
{
  my_bool r;
  DBUG_ENTER("vio_is_blocking");
  r = !(vio->fcntl_mode & O_NONBLOCK);
  DBUG_PRINT("exit", ("%d", (int) r));
  DBUG_RETURN(r);
}


int vio_fastsend(Vio * vio __attribute__((unused)))
{
  int r=0;
  DBUG_ENTER("vio_fastsend");

#ifdef IPTOS_THROUGHPUT
  {
#ifndef __EMX__
    int tos = IPTOS_THROUGHPUT;
    if (!setsockopt(vio->sd, IPPROTO_IP, IP_TOS, (void *) &tos, sizeof(tos)))
#endif /* !__EMX__ */
    {
      int nodelay = 1;
      if (setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY, (void *) &nodelay,
                     sizeof(nodelay))) {
        DBUG_PRINT("warning",
                   ("Couldn't set socket option for fast send"));
        r= -1;
      }
    }
  }
#endif /* IPTOS_THROUGHPUT */
  DBUG_PRINT("exit", ("%d", r));
  DBUG_RETURN(r);
}

int vio_keepalive(Vio* vio, my_bool set_keep_alive)
{
  int r=0;
  uint opt = 0;
  DBUG_ENTER("vio_keepalive");
  DBUG_PRINT("enter", ("sd=%d  set_keep_alive=%d", vio->sd, (int)
              set_keep_alive));
  if (vio->type != VIO_TYPE_NAMEDPIPE)
  {
    if (set_keep_alive)
      opt = 1;
    r = setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt,
                   sizeof(opt));
  }
  DBUG_RETURN(r);
}


my_bool
vio_should_retry(Vio * vio __attribute__((unused)))
{
  int en = socket_errno;
  return en == SOCKET_EAGAIN || en == SOCKET_EINTR || en == SOCKET_EWOULDBLOCK;
}


int vio_close(Vio * vio)
{
  int r;
  DBUG_ENTER("vio_close");
#ifdef HAVE_OPENSSL
  if (vio->type == VIO_TYPE_SSL)
  {
    r = my_ssl_close(vio);
  }
#endif
#ifdef _WIN32
  if (vio->type == VIO_TYPE_NAMEDPIPE)
  {
    r=CloseHandle(vio->hPipe);
  }
  else if (vio->type != VIO_CLOSED)
#endif /* _WIN32 */
  {
    r=0;
    if (shutdown(vio->sd,2))
      r= -1;
    if (closesocket(vio->sd))
      r= -1;
  }
  if (r)
  {
    DBUG_PRINT("vio_error", ("close() failed, error: %d",socket_errno));
    /* FIXME: error handling (not critical for MySQL) */
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;
  DBUG_RETURN(r);
}


const char *vio_description(Vio * vio)
{
  return vio->desc;
}

enum enum_vio_type vio_type(Vio* vio)
{
  return vio->type;
}

my_socket vio_fd(Vio* vio)
{
  return vio->sd;
}


my_bool vio_peer_addr(Vio * vio, char *buf)
{
  DBUG_ENTER("vio_peer_addr");
  DBUG_PRINT("enter", ("sd=%d", vio->sd));
  if (vio->localhost)
  {
    strmov(buf,"127.0.0.1");
  }
  else
  {
    socklen_t addrLen = sizeof(struct sockaddr);
    if (getpeername(vio->sd, (struct sockaddr *) (& (vio->remote)),
        &addrLen) != 0)
    {
      DBUG_PRINT("exit", ("getpeername, error: %d", socket_errno));
      DBUG_RETURN(1);
    }
    my_inet_ntoa(vio->remote.sin_addr,buf);
  }
  DBUG_PRINT("exit", ("addr=%s", buf));
  DBUG_RETURN(0);
}


void vio_in_addr(Vio *vio, struct in_addr *in)
{
  DBUG_ENTER("vio_in_addr");
  if (vio->localhost)
    bzero((char*) in, sizeof(*in)); /* This should never be executed */
  else
    *in=vio->remote.sin_addr;
  DBUG_VOID_RETURN;
}


/* Return 0 if there is data to be read */

my_bool vio_poll_read(Vio *vio,uint timeout)
{
#ifndef HAVE_POLL
  return 0;
#else
  struct pollfd fds;
  int res;
  DBUG_ENTER("vio_poll");
  fds.fd=vio->sd;
  fds.events=POLLIN;
  fds.revents=0;
  if ((res=poll(&fds,1,(int) timeout*1000)) <= 0)
  {
    DBUG_RETURN(res < 0 ? 0 : 1); /* Don't return 1 on errors */
  }
  DBUG_RETURN(fds.revents & POLLIN ? 0 : 1);
#endif
}

#endif /* HAVE_VIO */
