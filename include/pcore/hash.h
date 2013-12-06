#ifndef CORE_HASH_H_
#define CORE_HASH_H_

#include "types.h"

enum {
    PHASH_FAST_INSERT = 0,  // hashtable
    PHASH_FAST_SEARCH       // hashdyn
};

typedef struct s_phash_pool *phash_pool;


phash_pool pcore_hash_init(const puint8 type);

void pcore_hash_insert(phash_pool pool,
                       const puint32 value,
                       void* const data);

void* pcore_hash_search(phash_pool pool,
                        const puint32 value);

//ToDo: Список phash_object с таким же значение, а не только первое
//ToDo: Удаление одного элемента

void pcore_hash_foreach(phash_pool pool,
                        void (*ret_func)(const puint32 value,
                                         void* const data));

void pcore_hash_done(phash_pool *pool);

puint32 pcore_hash_size(phash_pool pool);



#endif
