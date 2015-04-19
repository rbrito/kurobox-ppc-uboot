#define FIRMNAME_MAX   31
#define SUBVERSION_MAX 31
#define FIRMINFO_VER 1

struct firminfo {
		unsigned long info_ver;
		unsigned long  firmid;
		char           firmname[FIRMNAME_MAX+1];
		char           subver[SUBVERSION_MAX+1];
		unsigned short ver_major;
		unsigned short ver_minor;
		unsigned short build;
		char           year;
		char           mon;
		char           day;
		char           hour;
		char           min;
		char           sec;
		unsigned long size;
		unsigned long chksum;
		
		unsigned long kernel_offset;
		unsigned long kernel_size;
		unsigned long initrd_offset;
		unsigned long initrd_size;
	} __attribute((aligned(4)));
// ----------------------------------------------------
