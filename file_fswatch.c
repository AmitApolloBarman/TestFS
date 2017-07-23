#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include "file_fswatch.h"

void add_file(file_paths_t *file_paths, char *path) {
    printf("adding %s length %lu size %lu\n", path, (unsigned long)file_paths->len, (unsigned long)file_paths->size);
    if (file_paths->len == file_paths->size) {
        file_paths->size = file_paths->size * 1.5;
        file_paths->paths = realloc(file_paths->paths, file_paths->size * sizeof(char *));
    }
    file_paths->paths[file_paths->len] = strdup(path);
    file_paths->len++;
}

void eval_event(long flag, char *name, char *path)
{
    if (flag & kFSEventStreamEventFlagItemChangeOwner)
    {
        printf("%s CHOWN %s flags:%lu\n", name, path, flag);
    }
    
    if (flag & kFSEventStreamEventFlagItemRemoved)
    {
        printf("%s REMOVED %s flags:%lu\n", name, path, flag);
    }

    if (flag & kFSEventStreamEventFlagItemModified)
    {
        printf("%s MODIFIED %s flags:%lu\n", name, path, flag);
    }
    
    if (flag & kFSEventStreamEventFlagItemCreated)
    {
        printf("%s CREATE %s flags:%lu\n", name, path, flag);
    }
    
    if (flag & kFSEventStreamEventFlagItemRenamed)
    {
        printf("%s RENAMED %s flags:%lu\n", name, path, flag);
    }
    
    if (flag & kFSEventStreamEventFlagMount)
    {
        printf("%s MOUNT %s flags:%lu\n", name, path, flag);
    }
    
    if (flag & kFSEventStreamEventFlagUnmount)
    {
        printf("%s UNMOUNT %s flags:%lu\n", name, path, flag);
    }
}

void event_cb(ConstFSEventStreamRef streamRef,
              void *ctx,
              size_t count,
              void *paths,
              const FSEventStreamEventFlags flags[],
              const FSEventStreamEventId ids[]) {
    size_t i;

    for (i = 0; i < count; i++) {
        //printf("________ %zu ________\n",count);
        char *path = ((char **)paths)[i];
        /* flags are unsigned long, IDs are uint64_t */
        //printf("\nChange %llu in %s, flags %lu", ids[i], path, (long)flags[i]);
        char *name = "FILE ";
        if ((long)flags[i] & kFSEventStreamEventFlagItemIsFile)
            name = "FILE";
        else
            name = "DIR";
        eval_event((long)flags[i], name, path);
    }
}


int do_eventfs(int argc, char **argv) {
    if (argc < 2) {
        argv[1] = "/";
        argc = 2;
    }

    CFMutableArrayRef paths = CFArrayCreateMutable(NULL, argc, NULL);
    CFStringRef cfs_path;
    char *dir_path;
    char *file_name;
    char *path;
    int i;
    int rv;
    struct stat s;

    file_paths_t *file_paths = malloc(sizeof(file_paths_t));
    file_paths->len = 0;
    file_paths->size = 2;
    file_paths->paths = malloc(file_paths->size * sizeof(char *));

    for (i = 1; i < argc; i++) {
        dir_path = NULL;
        path = realpath(argv[i], NULL);
        if (path == NULL) {
            path = dirname(argv[i]);
            if (strcmp(path, ".") == 0) {
                dir_path = realpath("./", NULL);
            } else {
                dir_path = realpath(path, NULL);
            }
            if (dir_path == NULL) {
                fprintf(stderr, "Error %i in realpath(\"%s\"): %s\n", errno, path, strerror(errno));
                exit(1);
            }
            file_name = basename(argv[i]);
            asprintf(&path, "%s/%s", dir_path, file_name);
        }

        printf("Path is %s\n", path);
        rv = stat(path, &s);
        if (rv < 0) {
            if (errno != 2) {
                fprintf(stderr, "Error %i stat()ing %s: %s\n", errno, path, strerror(errno));
                goto cleanup;
            }
            s.st_mode = S_IFREG;
        }

        if (s.st_mode & S_IFREG) {
            dir_path = dirname(path);
            file_name = basename(path);
            asprintf(&path, "%s/%s", dir_path, file_name);
            add_file(file_paths, path);
        } else if (s.st_mode & S_IFDIR) {
           dir_path = path;
        } else {
            fprintf(stderr, "I don't know what to do with %u\n", s.st_mode);
            goto cleanup;
        }

        cfs_path = CFStringCreateWithCString(NULL, dir_path, kCFStringEncodingUTF8);
        CFArrayAppendValue(paths, cfs_path);

    cleanup:;
        if (dir_path != path) {
            free(dir_path);
        }
        free(path);
    }

    FSEventStreamContext ctx = {
        0,
        file_paths,
        NULL,
        NULL,
        NULL
    };
    FSEventStreamRef stream;
    FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagFileEvents;

    stream = FSEventStreamCreate(NULL, &event_cb, &ctx, paths, kFSEventStreamEventIdSinceNow, 0, flags);
    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);

    CFRunLoopRun();

    return (0);
}
