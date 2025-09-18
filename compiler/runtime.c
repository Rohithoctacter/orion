#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

// Orion-specific memory allocation wrappers to avoid symbol collision
void* orion_malloc(size_t size) {
    return malloc(size);
}

void orion_free(void* ptr) {
    free(ptr);
}

void* orion_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

// Enhanced list structure for dynamic operations
typedef struct {
    int64_t size;        // Current number of elements
    int64_t capacity;    // Total allocated space
    int64_t* data;       // Pointer to element array (8 bytes per element)
} OrionList;

// Create a new empty list with initial capacity
OrionList* list_new(int64_t initial_capacity) {
    if (initial_capacity < 4) initial_capacity = 4; // Minimum capacity
    
    OrionList* list = (OrionList*)orion_malloc(sizeof(OrionList));
    if (!list) {
        fprintf(stderr, "Error: Failed to allocate memory for list\n");
        exit(1);
    }
    
    list->size = 0;
    list->capacity = initial_capacity;
    list->data = (int64_t*)orion_malloc(sizeof(int64_t) * initial_capacity);
    if (!list->data) {
        fprintf(stderr, "Error: Failed to allocate memory for list data\n");
        exit(1);
    }
    
    return list;
}

// Create a list from existing data (used by list literals)
OrionList* list_from_data(int64_t* elements, int64_t count) {
    OrionList* list = list_new(count > 4 ? count : 4);
    list->size = count;
    
    // Copy elements
    for (int64_t i = 0; i < count; i++) {
        list->data[i] = elements[i];
    }
    
    return list;
}

// Get list length
int64_t list_len(OrionList* list) {
    if (!list) {
        fprintf(stderr, "Error: Cannot get length of null list\n");
        exit(1);
    }
    return list->size;
}

// Normalize negative index to positive (Python-style)
int64_t normalize_index(OrionList* list, int64_t index) {
    if (!list) {
        fprintf(stderr, "Error: Cannot normalize index on null list\n");
        exit(1);
    }
    
    if (index < 0) {
        index += list->size;
    }
    
    if (index < 0 || index >= list->size) {
        fprintf(stderr, "Error: List index out of range\n");
        exit(1);
    }
    
    return index;
}

// Get element at index (supports negative indexing)
int64_t list_get(OrionList* list, int64_t index) {
    if (!list) {
        fprintf(stderr, "Error: Cannot access null list\n");
        exit(1);
    }
    
    index = normalize_index(list, index);
    return list->data[index];
}

// Set element at index (supports negative indexing)
void list_set(OrionList* list, int64_t index, int64_t value) {
    if (!list) {
        fprintf(stderr, "Error: Cannot modify null list\n");
        exit(1);
    }
    
    index = normalize_index(list, index);
    list->data[index] = value;
}

// Resize list capacity (internal function)
void list_resize(OrionList* list, int64_t new_capacity) {
    if (!list) return;
    
    // Protect against integer overflow
    if (new_capacity > INT64_MAX / sizeof(int64_t)) {
        fprintf(stderr, "Error: List capacity too large\n");
        exit(1);
    }
    
    int64_t* new_data = (int64_t*)orion_realloc(list->data, sizeof(int64_t) * new_capacity);
    if (!new_data) {
        fprintf(stderr, "Error: Failed to resize list\n");
        exit(1);
    }
    
    list->data = new_data;
    list->capacity = new_capacity;
}

// Append element to end of list
void list_append(OrionList* list, int64_t value) {
    if (!list) {
        fprintf(stderr, "Error: Cannot append to null list\n");
        exit(1);
    }
    
    // Resize if needed (double capacity)
    if (list->size >= list->capacity) {
        int64_t new_capacity = list->capacity * 2;
        list_resize(list, new_capacity);
    }
    
    list->data[list->size] = value;
    list->size++;
}

// Remove and return last element
int64_t list_pop(OrionList* list) {
    if (!list) {
        fprintf(stderr, "Error: Cannot pop from null list\n");
        exit(1);
    }
    
    if (list->size == 0) {
        fprintf(stderr, "Error: Cannot pop from empty list\n");
        exit(1);
    }
    
    list->size--;
    int64_t value = list->data[list->size];
    
    // Shrink capacity if list becomes much smaller (optional optimization)
    if (list->size < list->capacity / 4 && list->capacity > 8) {
        list_resize(list, list->capacity / 2);
    }
    
    return value;
}

// Insert element at specific index
void list_insert(OrionList* list, int64_t index, int64_t value) {
    if (!list) {
        fprintf(stderr, "Error: Cannot insert into null list\n");
        exit(1);
    }
    
    // Allow inserting at end (index == size)
    if (index < 0) {
        index += list->size;
    }
    if (index < 0 || index > list->size) {
        fprintf(stderr, "Error: Insert index out of range\n");
        exit(1);
    }
    
    // Resize if needed
    if (list->size >= list->capacity) {
        int64_t new_capacity = list->capacity * 2;
        list_resize(list, new_capacity);
    }
    
    // Shift elements to make room
    memmove(&list->data[index + 1], &list->data[index], 
            sizeof(int64_t) * (list->size - index));
    
    list->data[index] = value;
    list->size++;
}

// Concatenate two lists (returns new list)
OrionList* list_concat(OrionList* list1, OrionList* list2) {
    if (!list1 || !list2) {
        fprintf(stderr, "Error: Cannot concatenate null lists\n");
        exit(1);
    }
    
    int64_t total_size = list1->size + list2->size;
    OrionList* result = list_new(total_size);
    result->size = total_size;
    
    // Copy elements from both lists
    memcpy(result->data, list1->data, sizeof(int64_t) * list1->size);
    memcpy(&result->data[list1->size], list2->data, sizeof(int64_t) * list2->size);
    
    return result;
}

// Repeat list n times (returns new list)
OrionList* list_repeat(OrionList* list, int64_t count) {
    if (!list) {
        fprintf(stderr, "Error: Cannot repeat null list\n");
        exit(1);
    }
    
    if (count < 0) {
        fprintf(stderr, "Error: Cannot repeat list negative times\n");
        exit(1);
    }
    
    if (count == 0 || list->size == 0) {
        return list_new(4);
    }
    
    // Protect against overflow
    if (list->size > INT64_MAX / count) {
        fprintf(stderr, "Error: Repeated list would be too large\n");
        exit(1);
    }
    
    int64_t total_size = list->size * count;
    OrionList* result = list_new(total_size);
    result->size = total_size;
    
    // Copy the list data count times
    for (int64_t i = 0; i < count; i++) {
        memcpy(&result->data[i * list->size], list->data, sizeof(int64_t) * list->size);
    }
    
    return result;
}

// Extend list with elements from another list (modifies first list)
void list_extend(OrionList* list1, OrionList* list2) {
    if (!list1 || !list2) {
        fprintf(stderr, "Error: Cannot extend null lists\n");
        exit(1);
    }
    
    // Resize if needed
    int64_t new_size = list1->size + list2->size;
    if (new_size > list1->capacity) {
        int64_t new_capacity = list1->capacity;
        while (new_capacity < new_size) {
            new_capacity *= 2;
        }
        list_resize(list1, new_capacity);
    }
    
    // Copy elements from list2
    memcpy(&list1->data[list1->size], list2->data, sizeof(int64_t) * list2->size);
    list1->size = new_size;
}

// Print list for debugging (optional)
void list_print(OrionList* list) {
    if (!list) {
        printf("null\n");
        return;
    }
    
    printf("[");
    for (int64_t i = 0; i < list->size; i++) {
        if (i > 0) printf(", ");
        printf("%ld", list->data[i]);
    }
    printf("]\n");
}

// Input function - read a line from stdin
char* orion_input() {
    const int BUFFER_SIZE = 1024;
    char* buffer = (char*)orion_malloc(BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate memory for input\n");
        exit(1);
    }
    
    // Read line from stdin
    if (!fgets(buffer, BUFFER_SIZE, stdin)) {
        // Handle EOF or error
        orion_free(buffer);
        buffer = (char*)orion_malloc(1);
        buffer[0] = '\0';
        return buffer;
    }
    
    // Remove trailing newline if present
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    // Resize buffer to actual string length to save memory
    size_t actual_len = strlen(buffer);
    char* result = (char*)orion_malloc(actual_len + 1);
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate memory for input result\n");
        exit(1);
    }
    strcpy(result, buffer);
    orion_free(buffer);
    
    return result;
}

// Input function with prompt - display prompt then read input
char* orion_input_prompt(const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);  // Ensure prompt is displayed before reading
    }
    
    return orion_input();
}

// Helper function to convert integer to string and append to buffer
// Returns pointer to the end of the buffer for chaining
char* sprintf_int(char* buffer, int64_t value) {
    if (!buffer) return buffer;
    
    // Find end of current string
    while (*buffer) buffer++;
    
    // Convert integer to string and append
    sprintf(buffer, "%ld", value);
    
    // Return pointer to new end of string
    while (*buffer) buffer++;
    return buffer;
}

// Simple string concatenation function
// Appends src to the end of dest and returns pointer to new end
char* strcat_simple(char* dest, const char* src) {
    if (!dest || !src) return dest;
    
    // Find end of dest string
    while (*dest) dest++;
    
    // Copy src to end of dest
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    
    return dest;
}

// Convert integer to string (returns dynamically allocated string)
char* int_to_string(int64_t value) {
    char* buffer = (char*)orion_malloc(32);  // Enough for 64-bit int
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate memory for int_to_string\n");
        exit(1);
    }
    sprintf(buffer, "%ld", value);
    return buffer;
}

// Convert float to string (returns dynamically allocated string)
char* float_to_string(double value) {
    char* buffer = (char*)orion_malloc(64);  // Enough for double precision
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate memory for float_to_string\n");
        exit(1);
    }
    sprintf(buffer, "%.2f", value);
    return buffer;
}

// Convert boolean to string (returns dynamically allocated string)
char* bool_to_string(int64_t value) {
    char* buffer = (char*)orion_malloc(8);  // "True" or "False"
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate memory for bool_to_string\n");
        exit(1);
    }
    strcpy(buffer, value ? "True" : "False");
    return buffer;
}

// Copy string (for consistency with other conversion functions)
char* string_to_string(const char* value) {
    if (!value) {
        char* buffer = (char*)orion_malloc(1);
        if (buffer) buffer[0] = '\0';
        return buffer;
    }
    
    size_t len = strlen(value);
    char* buffer = (char*)orion_malloc(len + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate memory for string_to_string\n");
        exit(1);
    }
    strcpy(buffer, value);
    return buffer;
}

// String concatenation for interpolated strings
// Takes an array of string pointers and concatenates them
char* string_concat_parts(char** parts, int count) {
    if (!parts || count <= 0) {
        char* empty = (char*)orion_malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    // Calculate total length needed
    size_t total_len = 0;
    for (int i = 0; i < count; i++) {
        if (parts[i]) {
            total_len += strlen(parts[i]);
        }
    }
    
    // Allocate result buffer
    char* result = (char*)orion_malloc(total_len + 1);
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate memory for string_concat_parts\n");
        exit(1);
    }
    
    // Concatenate all parts
    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (parts[i]) {
            strcat(result, parts[i]);
        }
    }
    
    return result;
}

// Range object structure for Python-style range() function
typedef struct {
    int64_t start;
    int64_t stop;
    int64_t step;
    int64_t size;        // Number of elements in range
} OrionRange;

// Create a range object with start, stop, and step
OrionRange* range_new(int64_t start, int64_t stop, int64_t step) {
    if (step == 0) {
        fprintf(stderr, "Error: Range step cannot be zero\n");
        exit(1);
    }
    
    OrionRange* range = (OrionRange*)orion_malloc(sizeof(OrionRange));
    if (!range) {
        fprintf(stderr, "Error: Failed to allocate memory for range\n");
        exit(1);
    }
    
    range->start = start;
    range->stop = stop;
    range->step = step;
    
    // Calculate size of range
    if ((step > 0 && start >= stop) || (step < 0 && start <= stop)) {
        range->size = 0;
    } else {
        // Formula: ceil((stop - start) / step)
        int64_t diff = stop - start;
        if (step > 0) {
            range->size = (diff + step - 1) / step;
        } else {
            range->size = (diff + step + 1) / step;
        }
        if (range->size < 0) range->size = 0;
    }
    
    return range;
}

// Create range with just stop (start=0, step=1)
OrionRange* range_new_stop(int64_t stop) {
    return range_new(0, stop, 1);
}

// Create range with start and stop (step=1)
OrionRange* range_new_start_stop(int64_t start, int64_t stop) {
    return range_new(start, stop, 1);
}

// Get range length
int64_t range_len(OrionRange* range) {
    if (!range) {
        fprintf(stderr, "Error: Cannot get length of null range\n");
        exit(1);
    }
    return range->size;
}

// Get element at index for range
int64_t range_get(OrionRange* range, int64_t index) {
    if (!range) {
        fprintf(stderr, "Error: Cannot access null range\n");
        exit(1);
    }
    
    if (index < 0 || index >= range->size) {
        fprintf(stderr, "Error: Range index out of range\n");
        exit(1);
    }
    
    return range->start + (index * range->step);
}

// Convert range to list (for debugging/compatibility)
OrionList* range_to_list(OrionRange* range) {
    if (!range) {
        fprintf(stderr, "Error: Cannot convert null range to list\n");
        exit(1);
    }
    
    OrionList* list = list_new(range->size);
    list->size = range->size;
    
    for (int64_t i = 0; i < range->size; i++) {
        list->data[i] = range_get(range, i);
    }
    
    return list;
}

// Free range object
void range_free(OrionRange* range) {
    if (range) {
        orion_free(range);
    }
}

// =====================================================
// Built-in Type Conversion Functions
// =====================================================

// String conversion functions
char* __orion_int_to_string(int64_t value) {
    char* result = (char*)orion_malloc(32);  // Enough for any 64-bit integer
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate memory for string conversion\n");
        exit(1);
    }
    snprintf(result, 32, "%ld", value);
    return result;
}

char* __orion_float_to_string(double value) {
    char* result = (char*)orion_malloc(64);  // Enough for most float representations
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate memory for string conversion\n");
        exit(1);
    }
    snprintf(result, 64, "%.15g", value);  // Use 'g' format for clean output
    return result;
}

char* __orion_bool_to_string(int value) {
    const char* bool_str = value ? "true" : "false";
    size_t len = strlen(bool_str) + 1;
    char* result = (char*)orion_malloc(len);
    if (!result) {
        fprintf(stderr, "Error: Failed to allocate memory for string conversion\n");
        exit(1);
    }
    strcpy(result, bool_str);
    return result;
}

// Integer conversion functions
int64_t __orion_float_to_int(double value) {
    return (int64_t)value;  // Truncate towards zero
}

int64_t __orion_bool_to_int(int value) {
    return value ? 1 : 0;
}

int64_t __orion_string_to_int(const char* str) {
    if (!str) {
        fprintf(stderr, "Error: Cannot convert null string to integer\n");
        exit(1);
    }
    
    char* endptr;
    errno = 0;
    int64_t result = strtoll(str, &endptr, 10);
    
    // Check for conversion errors
    if (errno == ERANGE) {
        fprintf(stderr, "Error: Integer overflow in string conversion: '%s'\n", str);
        exit(1);
    }
    if (endptr == str || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid integer format: '%s'\n", str);
        exit(1);
    }
    
    return result;
}

// Float conversion functions
double __orion_int_to_float(int64_t value) {
    return (double)value;
}

double __orion_bool_to_float(int value) {
    return value ? 1.0 : 0.0;
}

double __orion_string_to_float(const char* str) {
    if (!str) {
        fprintf(stderr, "Error: Cannot convert null string to float\n");
        exit(1);
    }
    
    char* endptr;
    errno = 0;
    double result = strtod(str, &endptr);
    
    // Check for conversion errors
    if (errno == ERANGE) {
        fprintf(stderr, "Error: Float overflow in string conversion: '%s'\n", str);
        exit(1);
    }
    if (endptr == str || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid float format: '%s'\n", str);
        exit(1);
    }
    
    return result;
}

// ===== Dictionary Implementation =====

// Hash table entry for storing key-value pairs
typedef struct DictEntry {
    int64_t key;                  // Key (stored as int64_t for simplicity)
    int64_t value;                // Value (stored as int64_t for simplicity)
    struct DictEntry* next;       // For handling collisions with chaining
} DictEntry;

// Dictionary structure with hash table
typedef struct {
    int64_t size;                 // Number of key-value pairs
    int64_t capacity;             // Size of hash table (number of buckets)
    DictEntry** buckets;          // Array of bucket pointers
} OrionDict;

// Simple hash function for 64-bit integers
uint64_t dict_hash(int64_t key, int64_t capacity) {
    // Use a simple multiplicative hash
    uint64_t hash = (uint64_t)key;
    hash *= 2654435761ULL;  // Knuth's multiplicative constant
    return hash % capacity;
}

// Create a new empty dictionary with initial capacity
OrionDict* dict_new(int64_t initial_capacity) {
    if (initial_capacity < 8) initial_capacity = 8; // Minimum capacity
    
    OrionDict* dict = (OrionDict*)orion_malloc(sizeof(OrionDict));
    if (!dict) {
        fprintf(stderr, "Error: Failed to allocate memory for dictionary\n");
        exit(1);
    }
    
    dict->size = 0;
    dict->capacity = initial_capacity;
    dict->buckets = (DictEntry**)orion_malloc(sizeof(DictEntry*) * initial_capacity);
    if (!dict->buckets) {
        fprintf(stderr, "Error: Failed to allocate memory for dictionary buckets\n");
        exit(1);
    }
    
    // Initialize all buckets to NULL
    for (int64_t i = 0; i < initial_capacity; i++) {
        dict->buckets[i] = NULL;
    }
    
    return dict;
}

// Get dictionary length (number of key-value pairs)
int64_t dict_len(OrionDict* dict) {
    if (!dict) {
        fprintf(stderr, "Error: Cannot get length of null dictionary\n");
        exit(1);
    }
    return dict->size;
}

// Get value by key (returns -1 if key not found, for now)
int64_t dict_get(OrionDict* dict, int64_t key) {
    if (!dict) {
        fprintf(stderr, "Error: Cannot access null dictionary\n");
        exit(1);
    }
    
    uint64_t bucket_index = dict_hash(key, dict->capacity);
    DictEntry* entry = dict->buckets[bucket_index];
    
    // Search through the chain for the key
    while (entry) {
        if (entry->key == key) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    // Key not found
    fprintf(stderr, "Error: Key %ld not found in dictionary\n", key);
    exit(1);
}

// Check if key exists in dictionary
int dict_has_key(OrionDict* dict, int64_t key) {
    if (!dict) {
        fprintf(stderr, "Error: Cannot check null dictionary\n");
        exit(1);
    }
    
    uint64_t bucket_index = dict_hash(key, dict->capacity);
    DictEntry* entry = dict->buckets[bucket_index];
    
    // Search through the chain for the key
    while (entry) {
        if (entry->key == key) {
            return 1; // Found
        }
        entry = entry->next;
    }
    
    return 0; // Not found
}

// Resize dictionary when load factor gets too high
void dict_resize(OrionDict* dict) {
    if (!dict) return;
    
    // Save old data
    DictEntry** old_buckets = dict->buckets;
    int64_t old_capacity = dict->capacity;
    
    // Create new larger hash table
    dict->capacity *= 2;
    dict->buckets = (DictEntry**)orion_malloc(sizeof(DictEntry*) * dict->capacity);
    if (!dict->buckets) {
        fprintf(stderr, "Error: Failed to allocate memory for dictionary resize\n");
        exit(1);
    }
    
    // Initialize new buckets
    for (int64_t i = 0; i < dict->capacity; i++) {
        dict->buckets[i] = NULL;
    }
    
    // Reset size (will be recalculated as we re-insert)
    dict->size = 0;
    
    // Rehash all existing entries
    for (int64_t i = 0; i < old_capacity; i++) {
        DictEntry* entry = old_buckets[i];
        while (entry) {
            DictEntry* next = entry->next;
            
            // Rehash and insert into new table
            uint64_t new_bucket = dict_hash(entry->key, dict->capacity);
            entry->next = dict->buckets[new_bucket];
            dict->buckets[new_bucket] = entry;
            dict->size++;
            
            entry = next;
        }
    }
    
    // Free old bucket array
    orion_free(old_buckets);
}

// Set key-value pair in dictionary
void dict_set(OrionDict* dict, int64_t key, int64_t value) {
    if (!dict) {
        fprintf(stderr, "Error: Cannot modify null dictionary\n");
        exit(1);
    }
    
    // Check if we need to resize (load factor > 0.75)
    if (dict->size >= dict->capacity * 3 / 4) {
        dict_resize(dict);
    }
    
    uint64_t bucket_index = dict_hash(key, dict->capacity);
    DictEntry* entry = dict->buckets[bucket_index];
    
    // Search for existing key in the chain
    while (entry) {
        if (entry->key == key) {
            // Update existing key
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Key not found, create new entry
    DictEntry* new_entry = (DictEntry*)orion_malloc(sizeof(DictEntry));
    if (!new_entry) {
        fprintf(stderr, "Error: Failed to allocate memory for dictionary entry\n");
        exit(1);
    }
    
    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = dict->buckets[bucket_index];  // Insert at head of chain
    dict->buckets[bucket_index] = new_entry;
    dict->size++;
}

// Remove key from dictionary
void dict_remove(OrionDict* dict, int64_t key) {
    if (!dict) {
        fprintf(stderr, "Error: Cannot modify null dictionary\n");
        exit(1);
    }
    
    uint64_t bucket_index = dict_hash(key, dict->capacity);
    DictEntry* entry = dict->buckets[bucket_index];
    DictEntry* prev = NULL;
    
    // Search for the key in the chain
    while (entry) {
        if (entry->key == key) {
            // Remove the entry
            if (prev) {
                prev->next = entry->next;
            } else {
                dict->buckets[bucket_index] = entry->next;
            }
            orion_free(entry);
            dict->size--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
    
    // Key not found
    fprintf(stderr, "Error: Cannot remove key %ld - not found in dictionary\n", key);
    exit(1);
}

// Free dictionary and all its entries
void dict_free(OrionDict* dict) {
    if (!dict) return;
    
    // Free all entries
    for (int64_t i = 0; i < dict->capacity; i++) {
        DictEntry* entry = dict->buckets[i];
        while (entry) {
            DictEntry* next = entry->next;
            orion_free(entry);
            entry = next;
        }
    }
    
    // Free buckets array and dictionary struct
    orion_free(dict->buckets);
    orion_free(dict);
}