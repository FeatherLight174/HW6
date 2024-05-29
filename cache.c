#include "cache.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>


uint32_t log_2(uint32_t num){
    uint32_t result = 0;
    while(num!=1){
        num=num>>1;
        result+=1;
    }
    return result;
}



/* Create a cache simulator according to the config */
struct cache * cache_create(struct cache_config config,struct cache * lower_level){
    /*YOUR CODE HERE*/
    struct cache * new_cache = malloc(sizeof(struct cache));
    if(new_cache==NULL){
        return NULL;
    }
    else{
        new_cache->config = config;
        new_cache->lower_cache = lower_level;
        new_cache->index_bits = log_2(config.lines/config.ways);
        new_cache->offset_bits = log_2(config.line_size);
        new_cache->tag_bits = config.address_bits - new_cache->index_bits - new_cache->offset_bits;
        new_cache->offset_mask = (1<<(new_cache->offset_bits)) - 1;
        new_cache->index_mask = (1<<(new_cache->index_bits - 1))*(1<<(new_cache->offset_bits));
        new_cache->tag_mask = (1<<(new_cache->tag_bits - 1))*(1<<(new_cache->index_bits + new_cache->offset_bits));
        new_cache->lines = malloc(config.lines*sizeof(struct cache_line));
        for(uint32_t i = 0; i < config.lines; i++){
            new_cache->lines[i].data = malloc(config.line_size*sizeof(uint8_t * ));
            if((new_cache->lines[i].data)==NULL){
                return NULL;
            }
            new_cache->lines[i].dirty = 0;
            new_cache->lines[i].last_access = 0;
            new_cache->lines[i].tag = 0;
            new_cache->lines[i].valid = 0;
        }
    }
    return new_cache;
}

/* 
Release the resources allocated for the cache simulator. 
Also writeback dirty lines

The order in which lines are evicted is:
set0-slot0, set1-slot0, set2-slot0, (the 0th way)
set0-slot1, set1-slot1, set2-slot1, (the 1st way)
set0-slot2, set1-slot2, set2-slot2, (the 2nd way)
and so on.
*/
void cache_destroy(struct cache* cache){
    /*YOUR CODE HERE*/
    for(uint32_t i = 0; i < cache->config.ways; i++){
        for(int j = 0; j < 1<<(cache->index_bits); j++){
            uint32_t num = i*(1<<(cache->index_bits))+j;
            if(cache->lines[num].dirty*cache->lines[num].valid==1){
                uint32_t addr = (cache->lines[num].tag<<(cache->index_bits + cache->offset_bits))|(j << cache->offset_bits);
                mem_store(cache->lines[i].data, addr, cache->config.line_size);
            }
            if(cache->lines[num].data!=NULL){
                free(cache->lines[num].data);
            }
        }
    }
    free(cache->lines);
    free(cache);
}

/* Read one byte at a specific address. return hit=true/miss=false */
bool cache_read_byte(struct cache * cache, uint32_t addr, uint8_t *byte){
    /*YOUR CODE HERE*/
    uint32_t tag_read = (addr & cache->tag_mask)>>(cache->index_bits+cache->offset_bits);
    uint32_t index_read = (addr & cache->index_mask)>>(cache->offset_bits);
    if(cache->lower_cache==NULL){
        for(uint32_t i = index_read; i <= index_read + (cache->config.lines/cache->config.ways)*(cache->config.ways-1); i+=cache->config.lines/cache->config.ways){
            if((tag_read==cache->lines[i].tag)&&(cache->lines[i].valid==1)){
                cache->lines[i].last_access=get_timestamp();
                return true;
            }
            else if(cache->lines[i].valid==0){
                cache->lines[i].data = byte;
                cache->lines[i].last_access = get_timestamp();
                cache->lines[i].valid = 1;
            }
        }
        /*uint64_t access = cache->lines[(1 + index_read)*cache->config.ways-1].last_access;
        for(int i = index_read + (cache->config.lines/cache->config.ways)*(cache->config.ways-1); i >= index_read; i-=cache->config.lines/cache->config.ways){
            if(cache->lines[i-cache->config.lines/cache->config.ways].last_access<=access){
                access = cache->lines[i-cache->config.lines/cache->config.ways].last_access;
            }
        }
        for(int i = index_read; (i <= index_read + (cache->config.lines/cache->config.ways)*(cache->config.ways-1))&&(cache->lines[i].last_access==access); i+=cache->config.lines/cache->config.ways){
            if(cache->lines[i].dirty==1){
                uint32_t addr = (cache->lines[i].tag<<(cache->index_bits + cache->offset_bits))|(index_read<< cache->offset_bits);
                mem_store(cache->lines[i].data, addr, cache->config.line_size);
                free(cache->lines[i].data);

            }
        }*/
    }
    cache_write_byte(cache, addr, *byte);
    return false;
}
/* Write one byte into a specific address. return hit=true/miss=false*/
bool cache_write_byte(struct cache * cache, uint32_t addr, uint8_t byte){
    /*YOUR CODE HERE*/
    uint32_t tag_read = (addr & cache->tag_mask)>>(cache->index_bits+cache->offset_bits);
    uint32_t index_read = (addr & cache->index_mask)>>(cache->offset_bits);
    if(cache->lower_cache==NULL){
        for(uint32_t i = index_read; i <= index_read + (cache->config.lines/cache->config.ways)*(cache->config.ways-1); i+=cache->config.lines/cache->config.ways){
            if((tag_read==cache->lines[i].tag)&&(cache->lines[i].valid==1)){
                cache->lines[i].dirty=1;
                cache->lines[i].last_access=get_timestamp();
                return true;
            }
            else if(cache->lines[i].valid==0){
                cache->lines[i].data = &byte;
                cache->lines[i].last_access = get_timestamp();
                cache->lines[i].valid = 1;
                return false;
            }
        }
        uint8_t * load = malloc(cache->config.line_size);
        mem_load(load, addr, cache->config.line_size);
        uint64_t access = cache->lines[(1 + index_read)*cache->config.ways-1].last_access;

        for(uint32_t i = index_read + (cache->config.lines/cache->config.ways)*(cache->config.ways-1); i >= index_read; i-=cache->config.lines/cache->config.ways){
            if(cache->lines[i-cache->config.lines/cache->config.ways].last_access<=access){
                access = cache->lines[i-cache->config.lines/cache->config.ways].last_access;
            }
        }
        for(uint32_t i = index_read; (i <= index_read + (cache->config.lines/cache->config.ways)*(cache->config.ways-1))&&(cache->lines[i].last_access==access); i+=cache->config.lines/cache->config.ways){
            uint32_t addr = (cache->lines[i].tag<<(cache->index_bits + cache->offset_bits))|(index_read<< cache->offset_bits);
            if(cache->lines[i].dirty==1){                
                mem_store(cache->lines[i].data, addr, cache->config.line_size);
                cache->lines[i].dirty = 0;       
            }
            cache->lines[i].last_access = get_timestamp();
            cache->lines[i].tag = tag_read;
            cache->lines[i].valid = 1;
            return false;
        }

    }
    return 0;



}

