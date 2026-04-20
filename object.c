// object.c — Content-addressable object store
//
// Every piece of data (file contents, directory listings, commits) is stored
// as an "object" named by its SHA-256 hash. Objects are stored under
// .pes/objects/XX/YYYYYY... where XX is the first two hex characters of the
// hash (directory sharding).
//
// PROVIDED functions: compute_hash, object_path, object_exists, hash_to_hex, hex_to_hash
// TODO functions:     object_write, object_read

#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/evp.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    unsigned int hash_len;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, id_out->hash, &hash_len);
    EVP_MD_CTX_free(ctx);
}

// Get the filesystem path where an object should be stored.
// Format: .pes/objects/XX/YYYYYYYY...
// The first 2 hex chars form the shard directory; the rest is the filename.
void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─── TODO: Implement these ──────────────────────────────────────────────────

// Write an object to the store.
//
// Object format on disk:
//   "<type> <size>\0<data>"
//   where <type> is "blob", "tree", or "commit"
//   and <size> is the decimal string of the data length
//
// Steps:
//   1. Build the full object: header ("blob 16\0") + data
//   2. Compute SHA-256 hash of the FULL object (header + data)
//   3. Check if object already exists (deduplication) — if so, just return success
//   4. Create shard directory (.pes/objects/XX/) if it doesn't exist
//   5. Write to a temporary file in the same shard directory
//   6. fsync() the temporary file to ensure data reaches disk
//   7. rename() the temp file to the final path (atomic on POSIX)
//   8. Open and fsync() the shard directory to persist the rename
//   9. Store the computed hash in *id_out

// HINTS - Useful syscalls and functions for this phase:
//   - sprintf / snprintf : formatting the header string
//   - compute_hash       : hashing the combined header + data
//   - object_exists      : checking for deduplication
//   - mkdir              : creating the shard directory (use mode 0755)
//   - open, write, close : creating and writing to the temp file
//                          (Use O_CREAT | O_WRONLY | O_TRUNC, mode 0644)
//   - fsync              : flushing the file descriptor to disk
//   - rename             : atomically moving the temp file to the final path
//

//
// Returns 0 on success, -1 on error.
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    const char *type_str = NULL;
    char header[64];
    int header_len;
    size_t full_len;
    unsigned char *full_obj = NULL;
    ObjectID id;
    char final_path[512];
    char dir_path[512];
    char temp_path[512];
    char hex[HASH_HEX_SIZE + 1];
    int fd = -1, dirfd = -1;
    ssize_t written = 0;
    size_t total_written = 0;

    if (!data || !id_out) return -1;

    switch (type) {
        case OBJ_BLOB:   type_str = "blob";   break;
        case OBJ_TREE:   type_str = "tree";   break;
        case OBJ_COMMIT: type_str = "commit"; break;
        default: return -1;
    }

    header_len = snprintf(header, sizeof(header), "%s %zu", type_str, len) + 1;
    if (header_len <= 0 || (size_t)header_len > sizeof(header)) return -1;

    full_len = (size_t)header_len + len;
    full_obj = malloc(full_len);
    if (!full_obj) return -1;

    memcpy(full_obj, header, (size_t)header_len);
    memcpy(full_obj + header_len, data, len);

    compute_hash(full_obj, full_len, &id);
    *id_out = id;

    if (object_exists(&id)) {
        free(full_obj);
        return 0;
    }

    object_path(&id, final_path, sizeof(final_path));
    hash_to_hex(&id, hex);
    snprintf(dir_path, sizeof(dir_path), "%s/%.2s", OBJECTS_DIR, hex);
    snprintf(temp_path, sizeof(temp_path), "%s/.tmp-%d", dir_path, getpid());

    mkdir(PES_DIR, 0755);
    mkdir(OBJECTS_DIR, 0755);
    if (mkdir(dir_path, 0755) < 0 && access(dir_path, F_OK) != 0) {
        free(full_obj);
        return -1;
    }

    fd = open(temp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        free(full_obj);
        return -1;
    }

    while (total_written < full_len) {
        written = write(fd, full_obj + total_written, full_len - total_written);
        if (written <= 0) {
            close(fd);
            unlink(temp_path);
            free(full_obj);
            return -1;
        }
        total_written += (size_t)written;
    }

    if (fsync(fd) < 0) {
        close(fd);
        unlink(temp_path);
        free(full_obj);
        return -1;
    }

    if (close(fd) < 0) {
        unlink(temp_path);
        free(full_obj);
        return -1;
    }
    fd = -1;

    if (rename(temp_path, final_path) < 0) {
        unlink(temp_path);
        free(full_obj);
        return -1;
    }

    dirfd = open(dir_path, O_RDONLY | O_DIRECTORY);
    if (dirfd >= 0) {
        fsync(dirfd);
        close(dirfd);
    }

    free(full_obj);
    return 0;
}

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    char path[512];
    FILE *fp = NULL;
    long file_size;
    unsigned char *buf = NULL;
    unsigned char *nul_pos;
    char type_str[16];
    size_t data_len;
    ObjectID actual_id;

    if (!id || !type_out || !data_out || !len_out) return -1;

    object_path(id, path, sizeof(path));

    fp = fopen(path, "rb");
    if (!fp) return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return -1;
    }

    rewind(fp);

    buf = malloc((size_t)file_size);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    if (fread(buf, 1, (size_t)file_size, fp) != (size_t)file_size) {
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    compute_hash(buf, (size_t)file_size, &actual_id);
    if (memcmp(actual_id.hash, id->hash, HASH_SIZE) != 0) {
        free(buf);
        return -1;
    }

    nul_pos = memchr(buf, '\0', (size_t)file_size);
    if (!nul_pos) {
        free(buf);
        return -1;
    }

    if (sscanf((char *)buf, "%15s %zu", type_str, &data_len) != 2) {
        free(buf);
        return -1;
    }

    if (strcmp(type_str, "blob") == 0) {
        *type_out = OBJ_BLOB;
    } else if (strcmp(type_str, "tree") == 0) {
        *type_out = OBJ_TREE;
    } else if (strcmp(type_str, "commit") == 0) {
        *type_out = OBJ_COMMIT;
    } else {
        free(buf);
        return -1;
    }

    size_t header_len = (size_t)(nul_pos - buf) + 1;
    if (header_len + data_len != (size_t)file_size) {
        free(buf);
        return -1;
    }

    *data_out = malloc(data_len);
    if (!*data_out) {
        free(buf);
        return -1;
    }

    memcpy(*data_out, buf + header_len, data_len);
    *len_out = data_len;

    free(buf);
    return 0;
}
