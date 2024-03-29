/*
 * Copyright (c) 2009, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Adam Moody <moody20@llnl.gov>.
 * LLNL-CODE-411039.
 * All rights reserved.
 * This file is part of The Scalable Checkpoint / Restart (SCR) library.
 * For details, see https://sourceforge.net/projects/scalablecr/
 * Please also read this file: LICENSE.TXT.
*/

/* Implements an interface to read and write index files. */

#include "scr.h"
#include "scr_io.h"
#include "scr_err.h"
#include "scr_hash.h"
#include "scr_hash_util.h"
#include "scr_index_api.h"
#include "scr_dataset.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* strdup */
#include <string.h>

/* strftime */
#include <time.h>

/* basename/dirname */
#include <unistd.h>
#include <libgen.h>

#define SCR_INDEX_FILENAME "index.scr"

/* read the index file from given directory and merge its contents into the given hash */
int scr_index_read(const char* dir, scr_hash* index)
{
  /* build the file name for the index file */
  char index_file[SCR_MAX_FILENAME];
  if (scr_build_path(index_file, sizeof(index_file), dir, SCR_INDEX_FILENAME) != SCR_SUCCESS) {
    scr_err("Failed to build filename to read index file in dir %s @ %s:%d",
            dir, __FILE__, __LINE__
    );
    return SCR_FAILURE;
  }

  /* if we can access it, read the index file */
  if (scr_file_exists(index_file) == SCR_SUCCESS) {
    int rc = scr_hash_read(index_file, index);
    return rc;
  } else {
    return SCR_FAILURE;
  }
}

/* overwrite the contents of the index file in given directory with given hash */
int scr_index_write(const char* dir, scr_hash* index)
{
  /* build the file name for the index file */
  char index_file[SCR_MAX_FILENAME];
  if (scr_build_path(index_file, sizeof(index_file), dir, SCR_INDEX_FILENAME) != SCR_SUCCESS) {
    scr_err("Failed to build filename to write index file in dir %s @ %s:%d",
            dir, __FILE__, __LINE__
    );
    return SCR_FAILURE;
  }

  /* set the index file version key if it's not set already */
  scr_hash* version = scr_hash_get(index, SCR_INDEX_KEY_VERSION);
  if (version == NULL) {
    scr_hash_util_set_int(index, SCR_INDEX_KEY_VERSION, SCR_INDEX_FILE_VERSION_1);
  }

  /* write out the file */
  int rc = scr_hash_write(index_file, index);
  return rc;
}

/* this adds an entry to the index that maps a directory name to a dataset id */
static int scr_index_set_directory(scr_hash* hash, const char* name, int id)
{
  /* add entry to directory index */
  scr_hash* dir  = scr_hash_set_kv(hash, SCR_INDEX_1_KEY_DIR, name);
  scr_hash* dset = scr_hash_set_kv_int(dir, SCR_INDEX_1_KEY_DATASET, id);
  if (dset == NULL) {
    return SCR_FAILURE;
  }
  return SCR_SUCCESS;
}

/* add given dataset id and directory name to given hash */
int scr_index_add_dir(scr_hash* index, int id, const char* name)
{
  /* set the directory */
  scr_hash* dset_hash = scr_hash_set_kv_int(index, SCR_INDEX_1_KEY_DATASET, id);
  scr_hash* dir_hash  = scr_hash_set_kv(dset_hash, SCR_INDEX_1_KEY_DIR, name);

  /* add entry to directory index (maps directory name to dataset id) */
  scr_index_set_directory(index, name, id);

  return SCR_SUCCESS;
}

/* write completeness code (0 or 1) for given dataset id and directory in given hash */
int scr_index_set_complete(scr_hash* index, int id, const char* name, int complete)
{
  /* mark the dataset as complete or incomplete */
  scr_hash* dset_hash = scr_hash_set_kv_int(index, SCR_INDEX_1_KEY_DATASET, id);
  scr_hash* dir_hash  = scr_hash_set_kv(dset_hash, SCR_INDEX_1_KEY_DIR, name);
  scr_hash_util_set_int(dir_hash, SCR_INDEX_1_KEY_COMPLETE, complete);

  /* add entry to directory index (maps directory name to dataset id) */
  scr_index_set_directory(index, name, id);

  return SCR_SUCCESS;
}

/* write completeness code (0 or 1) for given dataset id and directory in given hash */
int scr_index_set_dataset(scr_hash* index, const scr_dataset* dataset, int complete)
{
  /* get the dataset id */
  int id;
  if (scr_dataset_get_id(dataset, &id) != SCR_SUCCESS) {
    scr_err("Failed to get dataset id for index file @ %s:%d",
            __FILE__, __LINE__
    );
    return SCR_FAILURE;
  }

  /* get the dataset name */
  char* name;
  if (scr_dataset_get_name(dataset, &name) != SCR_SUCCESS) {
    scr_err("Failed to get dataset name for index file @ %s:%d",
            __FILE__, __LINE__
    );
    return SCR_FAILURE;
  }

  /* copy contents of dataset hash */
  scr_hash* dataset_copy = scr_hash_new();
  scr_hash_merge(dataset_copy, dataset);

  /* get pointer to directory hash */
  scr_hash* dset_hash = scr_hash_set_kv_int(index, SCR_INDEX_1_KEY_DATASET, id);
  scr_hash* dir_hash  = scr_hash_set_kv(dset_hash, SCR_INDEX_1_KEY_DIR, name);

  /* record dataset hash in index */
  scr_hash_set(dir_hash, SCR_INDEX_1_KEY_DATASET, dataset_copy);

  /* mark the dataset as complete or incomplete */
  scr_hash_util_set_int(dir_hash, SCR_INDEX_1_KEY_COMPLETE, complete);

  /* add entry to directory index (maps directory name to dataset id) */
  scr_index_set_directory(index, name, id);

  return SCR_SUCCESS;
}

/* record fetch event for given dataset id and directory in given hash */
int scr_index_mark_fetched(scr_hash* index, int id, const char* name)
{
  /* format timestamp */
  time_t now = time(NULL);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));

  /* mark the dataset as fetched at current timestamp */
  scr_hash* dset_hash = scr_hash_set_kv_int(index, SCR_INDEX_1_KEY_DATASET, id);
  scr_hash* dir_hash  = scr_hash_set_kv(dset_hash, SCR_INDEX_1_KEY_DIR, name);
  scr_hash_set_kv(dir_hash, SCR_INDEX_1_KEY_FETCHED, timestamp);

  /* add entry to directory index (maps directory name to dataset id) */
  scr_index_set_directory(index, name, id);

  return SCR_SUCCESS;
}

/* record failed fetch event for given dataset id and directory in given hash */
int scr_index_mark_failed(scr_hash* index, int id, const char* name)
{
  /* format timestamp */
  time_t now = time(NULL);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));

  /* mark the dataset as failed at current timestamp */
  scr_hash* dset_hash = scr_hash_set_kv_int(index, SCR_INDEX_1_KEY_DATASET, id);
  scr_hash* dir_hash  = scr_hash_set_kv(dset_hash, SCR_INDEX_1_KEY_DIR, name);
  scr_hash_set_kv(dir_hash, SCR_INDEX_1_KEY_FAILED, timestamp);

  /* add entry to directory index (maps directory name to dataset id) */
  scr_index_set_directory(index, name, id);

  return SCR_SUCCESS;
}

/* record flush event for given dataset id and directory in given hash */
int scr_index_mark_flushed(scr_hash* index, int id, const char* name)
{
  /* format timestamp */
  time_t now = time(NULL);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", localtime(&now));

  /* mark the dataset as flushed at current timestamp */
  scr_hash* dset_hash = scr_hash_set_kv_int(index, SCR_INDEX_1_KEY_DATASET, id);
  scr_hash* dir_hash  = scr_hash_set_kv(dset_hash, SCR_INDEX_1_KEY_DIR, name);
  scr_hash_set_kv(dir_hash, SCR_INDEX_1_KEY_FLUSHED, timestamp);

  /* add entry to directory index (maps directory name to dataset id) */
  scr_index_set_directory(index, name, id);

  return SCR_SUCCESS;
}

/* get completeness code for given dataset id and directory in given hash,
 * sets complete=0 and returns SCR_FAILURE if key is not set */
int scr_index_get_complete(scr_hash* index, int id, const char* name, int* complete)
{
  int rc = SCR_FAILURE;
  *complete = 0;

  /* get the value of the COMPLETE key */
  scr_hash* dset_hash = scr_hash_get_kv_int(index, SCR_INDEX_1_KEY_DATASET, id);
  scr_hash* dir_hash  = scr_hash_get_kv(dset_hash, SCR_INDEX_1_KEY_DIR, name);
  scr_hash* comp_hash = scr_hash_get(dir_hash, SCR_INDEX_1_KEY_COMPLETE);
  int complete_tmp;
  if (scr_hash_util_get_int(dir_hash, SCR_INDEX_1_KEY_COMPLETE, &complete_tmp) == SCR_SUCCESS) {
    *complete = complete_tmp;
    rc = SCR_SUCCESS;
  }

  return rc;
}

/* lookup the dataset id corresponding to the given dataset directory name in given hash
 * (assumes a directory maps to a single dataset id) */
int scr_index_get_id_by_dir(const scr_hash* index, const char* name, int* id)
{
  /* assume that we won't find this dataset */
  *id = -1;

  /* attempt to lookup the dataset id */
  scr_hash* dir_hash = scr_hash_get_kv(index, SCR_INDEX_1_KEY_DIR, name);
  int id_tmp;
  if (scr_hash_util_get_int(dir_hash, SCR_INDEX_1_KEY_DATASET, &id_tmp) == SCR_SUCCESS) {
    /* found it, convert the string to an int and return */
    *id = id_tmp;
    return SCR_SUCCESS;
  }

  /* couldn't find it, return failure */
  return SCR_FAILURE;
}

/* lookup the most recent complete dataset id and directory whose id is less than earlier_than
 * setting earlier_than = -1 disables this filter */
int scr_index_get_most_recent_complete(const scr_hash* index, int earlier_than, int* id, char* name)
{
  /* assume that we won't find this dataset */
  *id = -1;

  /* search for the maximum dataset id which is complete and less than earlier_than
   * if earlier_than is set */
  int max_id = -1;
  scr_hash* dsets = scr_hash_get(index, SCR_INDEX_1_KEY_DATASET);
  scr_hash_elem* dset = NULL;
  for (dset = scr_hash_elem_first(dsets);
       dset != NULL;
       dset = scr_hash_elem_next(dset))
  {
    /* get the id for this dataset */
    char* key = scr_hash_elem_key(dset);
    if (key != NULL) {
      int current_id = atoi(key);
      /* if this dataset id is less than our limit and it's more than
       * our current max, check whether it's complete */
      if ((earlier_than == -1 || current_id <= earlier_than) && current_id > max_id) {
        /* alright, this dataset id is within range to be the most recent,
         * now scan the various dirs we have for this dataset looking for a complete */
        scr_hash* dset_hash = scr_hash_elem_hash(dset);
        scr_hash* dirs = scr_hash_get(dset_hash, SCR_INDEX_1_KEY_DIR);
        scr_hash_elem* dir = NULL;
        for (dir = scr_hash_elem_first(dirs);
             dir != NULL;
             dir = scr_hash_elem_next(dir))
        {
          char* dir_key = scr_hash_elem_key(dir);
          scr_hash* dir_hash = scr_hash_elem_hash(dir);

          int found_one = 1;

          /* look for the complete string */
          int complete;
          if (scr_hash_util_get_int(dir_hash, SCR_INDEX_1_KEY_COMPLETE, &complete) == SCR_SUCCESS) {
            if (complete != 1) {
              found_one = 0;
            }
          } else {
            found_one = 0;
          }

          /* check that there is no failed string */
          scr_hash* failed = scr_hash_get(dir_hash, SCR_INDEX_1_KEY_FAILED);
          if (failed != NULL) {
            found_one = 0;
          }

          /* TODO: also avoid dataset if we've tried to read it too many times */

          /* if we found one, copy the dataset id and directory name, and update our max */
          if (found_one) {
            *id = current_id;
            strcpy(name, dir_key);

            /* update our max */
            max_id = current_id;
            break;
          }
        }
      }
    }

  }

  return SCR_FAILURE;
}
