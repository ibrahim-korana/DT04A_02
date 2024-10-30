#ifndef _STORAGE_H
#define _STORAGE_H
#include "esp_spiffs.h"

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include <errno.h>
#include <sys/time.h>
#include "esp_vfs.h"
#include <sys/fcntl.h>
#include <unistd.h>
#include <ctype.h>

#define FORMAT_SPIFFS_IF_FAILED true

class Storage {
    public:
      Storage() {};
      ~Storage(){};

      bool init(void);
      bool format(void);

      bool file_search(const char *name);
      bool file_empty(const char *name);
      bool file_control(const char *name);
      int file_size(const char *name);
      bool file_create(const char *name, uint16_t size);
      
      bool write_file(const char *name, void *flg, uint16_t size, uint8_t obj_num);
      bool read_file(const char *name, void *flg, uint16_t size, uint8_t obj_num);

      
      
      void file_format(void);

      static const char *rangematch(const char *pattern, char test, int flags);
      static int fnmatch(const char *pattern, const char *string, int flags);
      static void list(const char *path, const char *match);
/*
      bool write_dev_config(remote_reg_t *stat, uint8_t obj_num, bool start=false);
      bool read_dev_config(remote_reg_t *stat, uint8_t obj_num);
*/   


};

#endif