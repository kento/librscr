# TODO: move -lz to m4 script
LIBZ_LDFLAGS = -lz

AM_CFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/config

# headers to be installed in /include subdirectory
include_HEADERS = \
	scr.h \
	scrf.h

# headers that should not be installed into /include
noinst_HEADERS = \
	scr_conf.h \
	scr_err.h \
	scr_io.h \
	scr_util.h \
	scr_hash.h \
	scr_hash_util.h \
	scr_hash_mpi.h \
	tv_data_display.h \
	scr_filemap.h \
	scr_index_api.h \
	scr_meta.h \
	scr_dataset.h \
	scr_config.h \
	scr_param.h \
	scr_env.h \
	scr_log.h \
	scr_halt.h \
	queue.h

lib_LTLIBRARIES = \
	libscr_base.la \
	libscr_cmds.la \
	libscr.la \
	libscrf.la \
	libscr_interpose.la

CMDS_INCLUDES = \
	$(DEFS) \
	$(DEFAULT_INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	$(AM_CFLAGS) $(CFLAGS) \
	$(MYSQL_CPPFLAGS)

# source files that are used by scr commands and runtime library
libscr_base_la_SOURCES = \
	scr_io.c \
	scr_util.c \
	scr_hash.c \
	scr_hash_util.c \
	tv_data_display.c \
	scr_filemap.c \
	scr_index_api.c \
	scr_meta.c \
	scr_dataset.c \
	scr_config.c \
	scr_param.c \
	scr_env.c \
	scr_log.c \
	scr_halt.c
libscr_base_la_CFLAGS = $(CMDS_INCLUDES)
libscr_base_la_LIBADD = $(MYSQL_LDFLAGS) $(MYSQL_LIBS) $(TV_LIBS) $(LIBZ_LDFLAGS)
libscr_base_la_LDFLAGS = -avoid-version

# source files that are only used for commands
libscr_cmds_la_SOURCES = \
	scr_err_serial.c \
	scr_config_serial.c
libscr_cmds_la_CFLAGS = $(CMDS_INCLUDES)
libscr_cmds_la_LIBADD = libscr_base.la
libscr_cmds_la_LDFLAGS = -avoid-version

INCLUDES = \
	$(DEFS) \
	$(DEFAULT_INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	$(AM_CFLAGS) $(CFLAGS) \
	$(YOGRT_CPPFLAGS) \
	$(MYSQL_CPPFLAGS)

# source files that are used by libraries (include MPI)
libscr_la_SOURCES = \
	scr_hash_mpi.c \
	scr.c
libscr_la_CFLAGS = $(INCLUDES) $(GCS_CFLAGS) $(MPI_CFLAGS)
libscr_la_LIBADD = $(YOGRT_LDFLAGS) $(YOGRT_LIBS) $(GCS_LDFLAGS) $(GCS_LIBS) $(MPI_CLDFLAGS) libscr_base.la
#(avoids numbers at end of library name)
libscr_la_LDFLAGS = -avoid-version
#libscr_la_LDFLAGS = -rpath $(DESTDIR)$(libdir)

libscrf_la_SOURCES = scrf.c
libscrf_la_CFLAGS = $(INCLUDES) $(MPI_CFLAGS)
libscrf_la_LIBADD = libscr.la
libscrf_la_LDFLAGS = -avoid-version

libscr_interpose_la_SOURCES = scr_interpose.c
libscr_interpose_la_CFLAGS = $(INCLUDES) $(MPI_CFLAGS)
libscr_interpose_la_LIBADD = libscr.la
libscr_interpose_la_LDFLAGS = -avoid-version

bin_PROGRAMS = \
	scr_inspect_cache \
	scr_copy \
	scr_rebuild_xor \
	scr_halt_cntl \
	scr_flush_file \
	scr_nodes_file \
	scr_retries_halt \
	scr_crc32 \
	scr_print_hash_file \
	scr_log_event \
	scr_log_transfer \
	scr_transfer \
	scr_rdma_transfer \
	scr_rdma_transfer_st \
	scr_index

scr_copy_SOURCES = scr_copy.c
scr_copy_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_copy_CFLAGS = $(AM_CFLAGS)

scr_inspect_cache_SOURCES = scr_inspect_cache.c
scr_inspect_cache_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_inspect_cache_CFLAGS = $(AM_CFLAGS)

scr_rebuild_xor_SOURCES = scr_rebuild_xor.c
scr_rebuild_xor_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_rebuild_xor_CFLAGS = $(AM_CFLAGS)

scr_halt_cntl_SOURCES = scr_halt_cntl.c
scr_halt_cntl_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_halt_cntl_CFLAGS = $(AM_CFLAGS)

scr_flush_file_SOURCES = scr_flush_file.c
scr_flush_file_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_flush_file_CFLAGS = $(AM_CFLAGS)

scr_nodes_file_SOURCES = scr_nodes_file.c
scr_nodes_file_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_nodes_file_CFLAGS = $(AM_CFLAGS)

scr_retries_halt_SOURCES = scr_retries_halt.c
scr_retries_halt_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_retries_halt_CFLAGS = $(AM_CFLAGS)

scr_crc32_SOURCES = scr_crc32.c
scr_crc32_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_crc32_CFLAGS = $(AM_CFLAGS)

scr_print_hash_file_SOURCES = scr_print_hash_file.c
scr_print_hash_file_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_print_hash_file_CFLAGS = $(AM_CFLAGS)

scr_log_event_SOURCES = scr_log_event.c
scr_log_event_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la $(MYSQL_LDFLAGS) $(MYSQL_LIBS)
scr_log_event_CFLAGS = $(AM_CFLAGS)

scr_log_transfer_SOURCES = scr_log_transfer.c
scr_log_transfer_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la $(MYSQL_LDFLAGS) $(MYSQL_LIBS)
scr_log_transfer_CFLAGS = $(AM_CFLAGS)

scr_transfer_SOURCES = scr_transfer.c
scr_transfer_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_transfer_CFLAGS = $(AM_CFLAGS)

scr_rdma_transfer_SOURCES = scr_rdma_transfer.c
scr_rdma_transfer_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la 
scr_rdma_transfer_CFLAGS = $(AM_CFLAG) -I/home/usr2/11D37048/usr/tools/libibtls/include -L/home/usr2/11D37048/usr/tools/libibtls/lib -lrdmacm -lpthread -libtls -lrdmacm -lpthread -libtls

scr_rdma_transfer_st_SOURCES = scr_rdma_transfer_st.c pgz.c yarn.c scr_list_queue.c
scr_rdma_transfer_st_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la 
scr_rdma_transfer_st_CFLAGS = $(AM_CFLAG) -I./ -I/home/usr2/11D37048/usr/tools/libibtls/include -L/home/usr2/11D37048/usr/tools/libibtls/lib -lrdmacm -lpthread -libtls -lrdmacm -lpthread -libtls -g


scr_index_SOURCES = scr_index.c
scr_index_LDADD = $(LIBZ_LDFLAGS) libscr_cmds.la
scr_index_CFLAGS = $(AM_CFLAGS)

