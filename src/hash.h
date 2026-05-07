#ifndef HASH_H
#define HASH_H

#include <stdio.h>
#include <stdlib.h>

#define HASH_TABLE_SIZE 10000
#define TABLE_IDX "../data/index/spotify_idx.bin"
#define ENTRIES_BIN "../data/index/spotify_entries.bin"
#define CSV_FILE "../data/raw/spotify_data.csv"
typedef struct hash_node{
    char title[512];
    char artist[2048];
    long offset;
    long next_entry;
} Hash_node;

unsigned int hash(char *title);
int node_exists(long *table, FILE *entries, char *normalized_title, char *normalized_artist);
int insert_node(long *table, FILE *entries, char *normalized_title, char *normalized_artist, long offset);
int build_index(const char *csv_path, const char *idx_path, const char *entries_path);
long search_node(long *table, FILE *entries, char *title, char *artist);
int search_range_node(long *table, FILE *entries, char *title, Hash_node *list, int size);
int load_table(const char *idx_path, long *table);

int normalize_string(char *input, char *output, size_t size);


#endif