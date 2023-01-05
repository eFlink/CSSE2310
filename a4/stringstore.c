#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Linked list node
struct StringStore {
    char *key;
    char *value;
    struct StringStore *nextStore;
};

typedef struct StringStore StringStore;

StringStore *stringstore_init(void);
StringStore *stringstore_free(StringStore *store);
int stringstore_add(StringStore *store, const char *key, const char *value);
const char *stringstore_retrieve(StringStore *store, const char *key);
int stringstore_delete(StringStore *store, const char *key);

// Create a new StringStore instance, and return a pointer to it.
StringStore *stringstore_init(void) {
    StringStore *store = malloc(sizeof(StringStore));
    store->key = NULL;
    store->value = NULL;
    store->nextStore = NULL;
    return store;
}

// Delete all memory associated with the given StringStore, and return NULL.
StringStore *stringstore_free(StringStore *store) {
    if (store->key == NULL) {
	free(store);
	return NULL;
    }
    StringStore *tmp;
    while (store != NULL) {
	tmp = store;
	store = store->nextStore;
	free(tmp->key);
	free(tmp->value);
	free(tmp);
    }
    return NULL;
}

/* Add the given 'key'/value' pair to the StringStore 'store'.
 * The 'key' and 'value' strings are copied with strdup() before being
 * added to the database. Returns 1 on success, 0 on failure (e.g. if
 * strdup() fails).
 */
int stringstore_add(StringStore *store, const char *key, const char *value) {
    char *newValue = strdup(value);

    // If strdup fails return 1.
    if (newValue == NULL) {
	return 0;
    }

    // Check if key is present in store
    char *oldValue = (char*) stringstore_retrieve(store, key);
    if (oldValue != NULL) {
	// Iterate through linked list till key-value is reached then update.
	while (store != NULL) {
	    if (!strcmp(store->key, key)) {
		store->value = newValue;
		return 1;
	    }
	    store = store->nextStore;
	}
	return 0;
    }
    char *newKey = strdup(key);
    // If strdup fails for key.
    if (newKey == NULL) {
	return 0;
    }

    // If head node is empty.
    if (store->key == NULL) {
	store->key = newKey;
	store->value = newValue;
	return 1;
    }
    // Iterate to the end
    while (store->nextStore != NULL) {
	store = store->nextStore;
    }
    // Add new StringStore node to the end of link list
    StringStore *newStore = malloc(sizeof(StringStore));
    newStore->key = newKey;
    newStore->value = newValue;
    store->nextStore = newStore;

    return 1;
}

/* Attempt to retrieve the value associated with a particular 'key' in the
 * StringStore 'store'.
 * If the key exists in the database, return a const pointer to corresponding
 * value string.
 * If the key does not exist, return NULL.
 */
const char *stringstore_retrieve(StringStore *store, const char *key) {
    // Check if first Node is empty or contains key-value pair.
    if (store->key == NULL) {
	return NULL;
    } else if (!strcmp(store->key, key)) {
	return store->value;
    }

    // Iterate through linked list till key-value pair is found then return.
    StringStore *currentNode = store;
    while (currentNode != NULL) {
	if (!strcmp(currentNode->key, key)) {
	    return currentNode->value;
	}
	currentNode = currentNode->nextStore;
    }

    return NULL;
}

/* Attempt to delete the key/value pair associated with a particular 'key' in
 * the StringStore 'store'.
 * If the key exists and deletion succeeds, return 1.
 * Otherwise, return 0.
 */
int stringstore_delete(StringStore *store, const char *key) {
    // Check if key-value pair is present
    const char *oldValue = stringstore_retrieve(store, key);
    if (oldValue == NULL) {
	return 0;
    }

    // If key-value pair is head node
    if (!strcmp(store->key, key)) {
	// Free memory
	free(store->key);
	free(store->value);
	if (store->nextStore == NULL) {
	    // If no other nodes are present set to NULL.
	    store->key = NULL;
	    store->value = NULL;
	} else {
	    // If other nodes are present replace head node with 2nd node.
	    StringStore *temp = store;
	    StringStore *nextStore = store->nextStore;
	    store->key = nextStore->key;
	    store->value = nextStore->value;
	    store->nextStore = nextStore->nextStore;
	    free(temp);
	}
    } else {
	// Iterate through linked list.
	StringStore *prevStore = store;
	store = store->nextStore;
	while (store != NULL) {
	    // if found, link the previous node to the next node.
	    if (!strcmp(store->key, key)) {
		prevStore->nextStore = store->nextStore;
		free(store->key);
		free(store->value);
		free(store);
		break;
	    }
	    prevStore = store;
	    store = store->nextStore;
	}
    }
    return 1;
}
