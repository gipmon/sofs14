/**
 *  \file soReadFileCluster.c (implementation file)
 *
 *  \author
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "sofs_probe.h"
#include "sofs_buffercache.h"
#include "sofs_superblock.h"
#include "sofs_inode.h"
#include "sofs_datacluster.h"
#include "sofs_basicoper.h"
#include "sofs_basicconsist.h"
#include "sofs_ifuncs_1.h"
#include "sofs_ifuncs_2.h"

/** \brief operation get the physical number of the referenced data cluster */
#define GET         0
/** \brief operation allocate a new data cluster and associate it to the inode which describes the file */
#define ALLOC       1
/** \brief operation free the referenced data cluster */
#define FREE        2
/** \brief operation free the referenced data cluster and dissociate it from the inode which describes the file */
#define FREE_CLEAN  3
/** \brief operation dissociate the referenced data cluster from the inode which describes the file */
#define CLEAN       4

/* allusion to internal function */

int soHandleFileCluster (uint32_t nInode, uint32_t clustInd, uint32_t op, uint32_t *p_outVal);

/**
 *  \brief Read a specific data cluster.
 *
 *  Data is read from a specific data cluster which is supposed to belong to an inode associated to a file (a regular
 *  file, a directory or a symbolic link). Thus, the inode must be in use and belong to one of the legal file types.
 *
 *  If the cluster has not been allocated yet, the returned data will consist of a cluster whose byte stream contents
 *  is filled with the character null (ascii code 0).
 *
 *  \param nInode number of the inode associated to the file
 *  \param clustInd index to the list of direct references belonging to the inode where data is to be read from
 *  \param buff pointer to the buffer where data must be read into
 *
 *  \return <tt>0 (zero)</tt>, on success
 *  \return -\c EINVAL, if the <em>inode number</em> or the <em>index to the list of direct references</em> are out of
 *                      range or the <em>pointer to the buffer area</em> is \c NULL
 *  \return -\c EIUININVAL, if the inode in use is inconsistent
 *  \return -\c ELDCININVAL, if the list of data cluster references belonging to an inode is inconsistent
 *  \return -\c EDCINVAL, if the data cluster header is inconsistent
 *  \return -\c ELIBBAD, if some kind of inconsistency was detected at some internal storage lower level
 *  \return -\c EBADF, if the device is not already opened
 *  \return -\c EIO, if it fails reading or writing
 *  \return -<em>other specific error</em> issued by \e lseek system call
 */

int soReadFileCluster (uint32_t nInode, uint32_t clustInd, SODataClust *buff)
{
  soColorProbe (411, "07;31", "soReadFileCluster (%"PRIu32", %"PRIu32", %p)\n", nInode, clustInd, buff);

  SOInode ind;
  SODataClust clust;
  SOSuperBlock *p_sosb;
  int stat, i;
  uint32_t nLogicClust, nFisicClust;


  //Load do SuperBlock
  if((stat = soLoadSuperBlock())!=0){
    return stat;
  }

  //Obter o ponteiro para o conteudo do SuperBlock
  p_sosb = soGetSuperBlock();

  //Validacoes
  if(((nInode >= p_sosb->iTotal)|| nInode <= 0) || (clustInd >= MAX_FILE_CLUSTERS) || (buff == NULL)){
    return -EINVAL;
  }

  //Obter o numero logico do cluster
  if((stat = soHandleFileCluster(nInode, clustInd, GET, &nLogicClust)) != 0){
    return stat;
  }

  if(nLogicClust == NULL_CLUSTER){
      for(i = 0; i < BSLPC; i++)
      {
        buff->info.data[i] = '\0';   
      }
  }
  else{
    nFisicClust = p_sosb->dZoneStart + nLogicClust * BLOCKS_PER_CLUSTER; 

    if( (stat = soReadCacheCluster(nFisicClust, &clust)) != 0){
      return stat;
    }

    /*copiar a informaçao para o buffer*/
    memcpy(buff, &clust, sizeof(SODataClust));

    if((stat = soWriteCacheCluster(nFisicClust, &clust)) != 0){
      return stat;
    }
  }

  /*gravar superBlock*/
  if( (stat = soStoreSuperBlock()) != 0){
    return stat;
  }

  return 0;

}

