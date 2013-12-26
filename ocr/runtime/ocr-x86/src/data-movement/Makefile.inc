
noinst_LTLIBRARIES += libocr_data_movement.la
libocr_la_LIBADD += libocr_data_movement.la

libocr_data_movement_la_SOURCES = data-movement/data-movement.c
libocr_data_movement_la_CFLAGS = $(AM_CFLAGS)
