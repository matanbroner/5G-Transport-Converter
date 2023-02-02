// An implementation of the "map" data structure
// The key is an integer, and the value is a string
// The map is implemented as an array of pointers to map_entry structs
// The map is initialized by calling map_init()
// The map is freed by calling map_free()
// The map is printed by calling map_print()
// The value associated with a key is set by calling map_set(key, value)
// The value associated with a key is retrieved by calling map_get(key)

#include <stdlib.h>
#include <string.h>

// Path: LD_PRELOAD/map.h

#define MAP_SIZE 1000

struct map_entry
{
    int key;
    char *value;
};

// map type is an array of pointers to map_entry structs
struct map_entry *map[MAP_SIZE];

void *map_init()
{
    // Initialize the map
    for (int i = 0; i < MAP_SIZE; i++)
        map[i] = NULL;
    return map;
}

void map_set(int key, char *value)
{
    // Find the entry with the given key
    // If it exists, update the value
    // If it doesn't exist, create a new entry
    // and insert it at the end of the list
    int idx = 0;
    while (idx < MAP_SIZE && map[idx] != NULL && map[idx]->key != key)
        idx++;
    if (idx == MAP_SIZE)
        return;
    if (map[idx] == NULL)
    {
        map[idx] = malloc(sizeof(struct map_entry));
        map[idx]->key = key;
        if (value != NULL)
            map[idx]->value = strdup(value);
        else
            map[idx]->value = NULL;
        return;
    }
    if (map[idx]->value != NULL)
    {
        map[idx]->key = key;
        if (value != NULL)
        {
            free(map[idx]->value);
            map[idx]->value = strdup(value);
        }
        else
            map[idx]->value = NULL;
        return;
    }
}

char *map_get(int key)
{
    // Find the entry with the given key
    // If it exists, return the value
    // If it doesn't exist, return NULL
    int idx = 0;
    while (idx < MAP_SIZE && map[idx] != NULL && map[idx]->key != key)
        idx++;
    if (idx == MAP_SIZE || map[idx] == NULL)
        return NULL;
    return map[idx]->value;
}

void map_print()
{
    // Print the map
    for (int i = 0; i < MAP_SIZE; i++)
        if (map[i] != NULL)
            printf("map[%d] = %d, %s\n", i, map[i]->key, map[i]->value);
}

void map_free()
{
    // Free the map
    for (int i = 0; i < MAP_SIZE; i++)
        if (map[i] != NULL)
        {
            free(map[i]->value);
            free(map[i]);
        }
}
