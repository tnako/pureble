#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "pcore/hash"

#include "tommy.h"

#include "plog/log.h"
#include "pcore/hash.h"
#include "pcore/internal.h"


typedef struct s_phash_pool {
    puint8 type;
    void *hash_struct;
    tommy_list *list;
} *phash_pool;

typedef struct s_phash_object {
    tommy_node list_node;
    tommy_node hash_node;
    puint32 value;
    void *data;
} *phash_object;


static int compare_hash_uint32(const void* arg, const void* obj)
{
    return (*(const puint32*)arg != ((const phash_object)obj)->value);
}

phash_pool pcore_hash_init(const puint8 type)
{
    plog_dbg("%s(): Инициализация пула типа %d", __PRETTY_FUNCTION__, type);

    phash_pool pool = pmalloc_check(__PRETTY_FUNCTION__, sizeof(struct s_phash_pool));
    if (!pool) {
        return NULL;
    }

    pool->type = type;

    tommy_list *list = pmalloc_check(__PRETTY_FUNCTION__, sizeof(tommy_list));
    if (!list) {
        free(pool);
        return NULL;
    }

    void *hash_struct;

    switch (type) {
    case PHASH_FAST_SEARCH:
        hash_struct = pmalloc_check(__PRETTY_FUNCTION__, sizeof(tommy_hashdyn));
        if (!hash_struct) {
            free(pool);
            free(list);
            return NULL;
        }
        tommy_hashdyn_init(hash_struct);
        break;
    case PHASH_FAST_INSERT:
    default:
        hash_struct = pmalloc_check(__PRETTY_FUNCTION__, sizeof(tommy_hashtable));
        if (!hash_struct) {
            free(pool);
            free(list);
            return NULL;
        }
        tommy_hashtable_init(hash_struct, 1024);
        break;
    }

    tommy_list_init(list);

    pool->list = list;
    pool->hash_struct = hash_struct;

    plog_dbg("%s(): Пул 0x%08X успешно создан", __PRETTY_FUNCTION__, pool);

    return pool;
}

static phash_object pcore_hash_newObject(const puint32 value, void* const data)
{
    if (!data) {
        plog_error("%s(): Нет данных для сопоставления!", __PRETTY_FUNCTION__);
        return NULL;
    }

    plog_dbg("%s(): Подготовка нового объекта со значение '%d' и указателем на данные 0x%08X", __PRETTY_FUNCTION__, value, data);

    phash_object object = pmalloc_check(__PRETTY_FUNCTION__, sizeof(struct s_phash_object));

    if (!object) {
        return NULL;
    }

    object->value = value;
    object->data = data;

    return object;
}

static bool pcore_hash_deleteObject(phash_object *object)
{
    if (!object || !(*object)) {
        plog_error("%s(): Нет phash_object!", __PRETTY_FUNCTION__);
        return false;
    }

    plog_dbg("%s(): Очистка ресурсов 0x%08X со значением '%d'", __PRETTY_FUNCTION__, *object, (*object)->value);

    void *adr = *object;
    pfree(&adr);

    return true;
}


void pcore_hash_insert(phash_pool pool, const puint32 value, void* const data)
{
    if (!pool) {
        plog_error("%s(): Нет phash_pool!", __PRETTY_FUNCTION__);
        return;
    }

    plog_dbg("%s(): Вставка ресурсов со значением '%d'", __PRETTY_FUNCTION__, value);

    phash_object object = pcore_hash_newObject(value, data);

    tommy_list_insert_tail(pool->list, &object->list_node, object);

    switch (pool->type) {
    case PHASH_FAST_SEARCH:
        tommy_hashdyn_insert((tommy_hashdyn *)pool->hash_struct, &object->hash_node, object, tommy_inthash_u32(object->value));
        break;
    case PHASH_FAST_INSERT:
    default:
        tommy_hashtable_insert((tommy_hashtable *)pool->hash_struct, &object->hash_node, object, tommy_inthash_u32(object->value));
        break;
    }
}


void* pcore_hash_search(phash_pool pool, const puint32 value)
{
    if (!pool) {
        plog_error("%s(): Нет phash_pool!", __PRETTY_FUNCTION__);
        return NULL;
    }

    plog_dbg("%s(): Поиск в пуле 0x%08X значения '%d'", __PRETTY_FUNCTION__, pool, value);

    phash_object ret = NULL;

    switch (pool->type) {
    case PHASH_FAST_SEARCH:
        ret = tommy_hashdyn_search((tommy_hashdyn *)pool->hash_struct, compare_hash_uint32, &value, tommy_inthash_u32(value));
        break;
    case PHASH_FAST_INSERT:
    default:
        ret = tommy_hashtable_search((tommy_hashtable *)pool->hash_struct, compare_hash_uint32, &value, tommy_inthash_u32(value));
        break;
    }

    return ret->data;
}


void pcore_hash_done(phash_pool *ptr)
{
    phash_pool pool = (*ptr);

    if (!pool) {
        plog_error("%s(): Нет phash_pool!", __PRETTY_FUNCTION__);
        return;
    }

    plog_dbg("%s(): Очистка пула 0x%08X", __PRETTY_FUNCTION__, pool);

    tommy_node* i = tommy_list_head(pool->list);
    while (i) {
        tommy_node* i_next = i->next;
        if (!pcore_hash_deleteObject((phash_object*)&i->data)) {
            break;
        }
        i = i_next;
    }

    switch (pool->type) {
    case PHASH_FAST_SEARCH:
        tommy_hashdyn_done((tommy_hashdyn *)pool->hash_struct);
        break;
    case PHASH_FAST_INSERT:
    default:
        tommy_hashtable_done((tommy_hashtable *)pool->hash_struct);
        break;
    }

    pfree((void **)&pool->hash_struct);
    pfree((void **)&pool->list);

    pfree((void **)&pool);
}


void pcore_hash_foreach(phash_pool pool, void (*ret_func)(const puint32 value,
                                                          void* const data))
{
    if (!pool) {
        plog_error("%s(): Нет phash_pool!", __PRETTY_FUNCTION__);
        return;
    }

    if (!ret_func) {
        plog_error("%s(): Нет функции для вызова!", __PRETTY_FUNCTION__);
        return;
    }

    plog_dbg("%s(): Перебор пула 0x%08X и обращение к 0x%08X", __PRETTY_FUNCTION__, pool, &ret_func);

    tommy_node* i = tommy_list_head(pool->list);
    while (i) {
        phash_object obj = i->data;
        i = i->next;
        ret_func(obj->value, obj->data);
    }
}


puint32 pcore_hash_size(phash_pool pool)
{
    if (!pool) {
        plog_error("%s(): Нет phash_pool!", __PRETTY_FUNCTION__);
        return -1;
    }

    puint32 ret = 0;

    plog_dbg("%s(): Подсчёт количества элементов у пула 0x%08X", __PRETTY_FUNCTION__, pool);

    switch (pool->type) {
    case PHASH_FAST_SEARCH:
        ret = tommy_hashdyn_count((tommy_hashdyn *)pool->hash_struct);
        break;
    case PHASH_FAST_INSERT:
    default:
        ret = tommy_hashtable_count((tommy_hashtable *)pool->hash_struct);
        break;
    }

    return ret;
}
