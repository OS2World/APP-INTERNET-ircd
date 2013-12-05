#ifndef __setup_include__
#define __setup_include__
#define	PARAMH
#define	UNISTDH
#undef		STRINGH
#undef		STRINGSH
#define	STDLIBH
#define	STDDEFH
#undef		SYSSYSLOGH
#define	NOINDEX
#undef		NEED_STRERROR
#define		NEED_STRTOKEN
#undef		NEED_STRTOK
#undef		NEED_INET_ADDR
#undef		NEED_INET_NTOA
#undef		NEED_INET_NETOF
#undef		GETTIMEOFDAY
#undef		LRAND48
#undef		MALLOCH
#define	bzero(a,b)	memset(a,0,b)
#define	bcopy(a,b,c)	memcpy(b,a,c)
#define	bcmp	memcmp
#define	NBLOCK_BSD
#define	BSD_RELIABLE_SIGNALS
#define	TIMES_2
#undef		GETRUSAGE_2
#endif
