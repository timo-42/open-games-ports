#include <ctype.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
/* cod2_prototypes.h was generated from the pre-migration chunk_024.json whose FS_LoadPackFile signature names the
 * old local tag fs_pack_s (now canonical iwd_s); map it until the merge lane regenerates the header. */
#define fs_pack_s iwd_s
#include "cod2_prototypes.h"
#undef fs_pack_s
#include "chunk_024_types.h"

/*
 * chunk_024: filesystem core (FS_*) of cod2_lnxded 1.3.
 * Functions are in address order. Each provenance line names the Ghidra label
 * so the merge lane can match it back to the binary.
 */

// 0x0809eb76  size=341  FUN_0809eb76  status=translated  naming=observed
// naming evidence: rodata "Short read in FS_CopyFile()" and "Short write in FS_CopyFile()" in this body.
void FS_CopyFile(const char *fromOSPath, const char *toOSPath)
{
    FILE *in;
    FILE *out;
    size_t size;
    size_t nread;
    size_t nwritten;
    void *buf;

    in = fopen(fromOSPath, "rb");
    if (in == NULL) {
        return;
    }
    fseek(in, 0, COD2_FS_SEEK_END);
    size = (size_t)ftell(in);
    fseek(in, 0, COD2_FS_SEEK_SET);
    buf = malloc(size);
    nread = fread(buf, 1, size, in);
    if (nread != size) {
        Com_Error(0, "\x15" "Short read in FS_CopyFile()\n");
    }
    fclose(in);

    if (FS_CreatePath((char *)toOSPath) == 0) {
        out = fopen(toOSPath, "wb");
        if (out == NULL) {
            free(buf);
        } else {
            nwritten = fwrite(buf, 1, size, out);
            if (nwritten != size) {
                Com_Error(0, "\x15" "Short write in FS_CopyFile()\n");
            }
            fclose(out);
            free(buf);
        }
    } else {
        free(buf);
    }
}

// 0x0809eccc  size=19  FUN_0809eccc  status=translated  naming=inferred
// naming evidence: body is a single remove() on an OS path; used as the delete step of FS_Rename.
void FS_Remove(const char *osPath)
{
    remove(osPath);
}

// 0x0809ece0  size=122  FUN_0809ece0  status=translated  naming=inferred
// naming evidence: builds a homepath OS path with fs_gamedir and probes it with fopen("rb"); Q3 FS_FileExists shape.
qboolean FS_FileExists(const char *filename)
{
    char testpath[268];
    FILE *f;
    qboolean exists;

    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, filename, testpath);
    f = fopen(testpath, "rb");
    exists = (f != NULL);
    if (exists) {
        fclose(f);
    }
    return exists;
}

// 0x0809ed5a  size=249  FUN_0809ed5a  status=translated  naming=observed
// naming evidence: rodata "FS_Rename: %s --> %s\n" in this body.
void FS_Rename(const char *from, const char *to)
{
    char fromOSPath[264];
    char toOSPath[256];
    int ret;

    FS_Nop();
    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, from, fromOSPath);
    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, to, toOSPath);
    if (fs_debug->current.integer != 0) {
        Com_Printf("FS_Rename: %s --> %s\n", fromOSPath, toOSPath);
    }
    ret = rename(fromOSPath, toOSPath);
    if (ret != 0) {
        FS_Remove(toOSPath);
        ret = rename(fromOSPath, toOSPath);
        if (ret != 0) {
            FS_CopyFile(fromOSPath, toOSPath);
            FS_Remove(fromOSPath);
        }
    }
}

// 0x0809ee54  size=233  FUN_0809ee54  status=translated  naming=inferred
// naming evidence: closes a handle slot (stream or pack entry), clears the slot; Q3 FS_FCloseFile shape.
void FS_FCloseFile(int f)
{
    FS_Nop();
    if (fsh[f].streamed != 0) {
        Sys_NoOp_080d4754(); /* original pushes f; callee is an empty stub */
    }
    if (fsh[f].iwd == NULL) {
        if (f != 0) {
            FILE *fp = (FILE *)FS_FileForHandle(f);
            fclose(fp);
        }
        Com_Memset(&fsh[f], 0, COD2_FS_HANDLE_SIZE);
    } else {
        unzCloseCurrentFile((struct unz_s *)fsh[f].handleFiles);
        if (fsh[f].zipFile != 0) {
            unzClose((struct unz_s *)fsh[f].handleFiles);
        }
        Com_Memset(&fsh[f], 0, COD2_FS_HANDLE_SIZE);
    }
}

// 0x0809ef3e  size=162  FUN_0809ef3e  status=translated  naming=unknown
// naming evidence: no string; fopen on a caller-supplied OS path, then records the slot. Descriptive name.
int FS_OpenOSFileHandle(const char *qpath, const char *osPath, const char *mode, int uniqueFile)
{
    FILE *fp;
    int f;

    fp = fopen(osPath, mode);
    if (fp == NULL) {
        return 0;
    }
    f = FS_HandleForFile(uniqueFile);
    fsh[f].iwd = NULL;
    fsh[f].handleFiles = fp;
    Q_strncpyz(fsh[f].name, qpath, 0x100);
    fsh[f].handleSync = 0;
    return f;
}

// 0x0809efe0  size=169  FUN_0809efe0  status=translated  naming=observed
// naming evidence: rodata "FS_FOpenFileWrite: %s\n" in this body.
int FS_FOpenFileWrite(const char *filename)
{
    char ospath[264];
    int f;

    FS_Nop();
    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, filename, ospath);
    if (fs_debug->current.integer != 0) {
        Com_Printf("FS_FOpenFileWrite: %s\n", ospath);
    }
    if (FS_CreatePath(ospath) == 0) {
        f = FS_OpenOSFileHandle(filename, ospath, "wb", 0);
    } else {
        f = 0;
    }
    return f;
}

// 0x0809f08a  size=338  FUN_0809f08a  status=translated  naming=inferred
// naming evidence: same debug string as FS_FOpenFileWrite, but opens with "wt" and allocates the slot before the path is built.
int FS_FOpenFileWriteText(const char *filename)
{
    char ospath[264];
    int f;

    FS_Nop();
    f = FS_HandleForFile(0);
    fsh[f].iwd = NULL;
    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, filename, ospath);
    if (fs_debug->current.integer != 0) {
        Com_Printf("FS_FOpenFileWrite: %s\n", ospath);
    }
    if (FS_CreatePath(ospath) == 0) {
        fsh[f].handleFiles = fopen(ospath, "wt");
        Q_strncpyz(fsh[f].name, filename, 0x100);
        fsh[f].handleSync = 0;
        if (fsh[f].handleFiles == NULL) {
            f = 0;
        }
        return f;
    }
    return 0;
}

// 0x0809f1dc  size=335  FUN_0809f1dc  status=translated  naming=observed
// naming evidence: rodata "FS_FOpenFileAppend: %s\n" in this body.
int FS_FOpenFileAppend(const char *filename)
{
    char ospath[264];
    int f;

    FS_Nop();
    f = FS_HandleForFile(0);
    fsh[f].iwd = NULL;
    Q_strncpyz(fsh[f].name, filename, 0x100);
    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, filename, ospath);
    if (fs_debug->current.integer != 0) {
        Com_Printf("FS_FOpenFileAppend: %s\n", ospath);
    }
    if (FS_CreatePath(ospath) == 0) {
        fsh[f].handleFiles = fopen(ospath, "at");
        fsh[f].handleSync = 0;
        if (fsh[f].handleFiles == NULL) {
            f = 0;
        }
        return f;
    }
    return 0;
}

// 0x0809f32c  size=156  FUN_0809f32c  status=translated  naming=inferred
// naming evidence: case-folds, maps '\\' and ':' to '/', returns 0 on equality; Q3 FS_FilenameCompare shape.
int FS_FilenameCompare(const char *s1, const char *s2)
{
    int c1;
    int c2;

    for (;;) {
        c1 = *s1++;
        c2 = *s2++;
        if (Q_islower(c1) != 0) {
            c1 -= 0x20;
        }
        if (Q_islower(c2) != 0) {
            c2 -= 0x20;
        }
        if (c1 == '\\' || c1 == ':') {
            c1 = '/';
        }
        if (c2 == '\\' || c2 == ':') {
            c2 = '/';
        }
        if (c1 != c2) {
            break;
        }
        if (c1 == 0) {
            return 0;
        }
    }
    return -1;
}

// 0x0809f3c8  size=606  FUN_0809f3c8  status=translated  naming=observed
// naming evidence: rodata "FS_FileCompare: %s does not exist" and "Short read in FS_FileCompare()" in this body.
// note: the two comparison buffers come from Com_MallocZeroed and are released with libc free(); the pairing is unverified.
int FS_FileCompare(const char *file1, const char *file2)
{
    FILE *s1;
    FILE *s2;
    long pos1;
    long pos2;
    long size1;
    long size2;
    long i;
    char *buf1;
    char *buf2;
    char *p1;
    char *p2;
    int nread;

    s1 = fopen(file1, "rb");
    if (s1 == NULL) {
        Com_Error(0, "\x15" "FS_FileCompare: %s does not exist\n", file1);
    }
    s2 = fopen(file2, "rb");
    if (s2 == NULL) {
        fclose(s1);
        return 0;
    }

    pos1 = ftell(s1);
    fseek(s1, 0, COD2_FS_SEEK_END);
    size1 = ftell(s1);
    fseek(s1, pos1, COD2_FS_SEEK_SET);

    pos2 = ftell(s2);
    fseek(s2, 0, COD2_FS_SEEK_END);
    size2 = ftell(s2);
    fseek(s2, pos2, COD2_FS_SEEK_SET);

    if (size1 != size2) {
        fclose(s1);
        fclose(s2);
        return 0;
    }

    buf1 = (char *)Com_MallocZeroed((int)size1);
    nread = (int)fread(buf1, 1, size1, s1);
    if (nread != size1) {
        Com_Error(0, "\x15" "Short read in FS_FileCompare()\n");
    }
    fclose(s1);

    buf2 = (char *)Com_MallocZeroed((int)size2);
    nread = (int)fread(buf2, 1, size2, s2);
    if (nread != size2) {
        Com_Error(0, "\x15" "Short read in FS_FileCompare()\n");
    }
    fclose(s2);

    p1 = buf1;
    p2 = buf2;
    for (i = 0; i < size1; i++) {
        if (*p1 != *p2) {
            free(buf1);
            free(buf2);
            return 0;
        }
        p1++;
        p2++;
    }
    free(buf1);
    free(buf2);
    return 1;
}

// 0x0809f626  size=116  FUN_0809f626  status=translated  naming=unknown
// naming evidence: no string; builds a copy of key with every byte offset by shift and strstr()s it, result discarded.
// note: the key copy has no bound check against its 264-byte buffer (preserved from the binary).
void FS_ShiftedKeyProbe(char *haystack, const char *key, char shift)
{
    char shifted[264];
    int i;

    for (i = 0; key[i] != '\0'; i++) {
        shifted[i] = (char)(shift + key[i]);
    }
    shifted[i] = '\0';
    strstr(haystack, shifted);
}

// 0x0809f69a  size=125  FUN_0809f69a  status=translated  naming=inferred
// naming evidence: compares the extension against "cfg" (case-insensitive), "menu", and ".dm_NETWORK_PROTOCOL_VERSION" (rodata).
// note: the last literal keeps its leading dot, so it can only match after a leading dot was not stripped (preserved).
qboolean FS_IsUnrestrictedExtension(const char *ext)
{
    if (*ext == '.') {
        ext++;
    }
    if (strcasecmp(ext, "cfg") == 0) {
        return 1;
    }
    if (Q_stricmp(ext, "menu") == 0) {
        return 1;
    }
    if (Q_stricmp(ext, ".dm_NETWORK_PROTOCOL_VERSION") == 0) {
        return 1;
    }
    return 0;
}

// 0x0809f718  size=70  FUN_0809f718  status=translated  naming=unknown
// naming evidence: no string; true when the text starts with ".." or "::". Descriptive name.
qboolean FS_IsParentPathPrefix(const char *s)
{
    if (s[0] == '.' && s[1] == '.') {
        return 1;
    }
    if (s[0] == ':' && s[1] == ':') {
        return 1;
    }
    return 0;
}

// 0x0809f75e  size=66  FUN_0809f75e  status=translated  naming=unknown
// naming evidence: no string; true for '/' and '\\'. Descriptive name.
qboolean FS_IsPathSeparator(char c)
{
    return (c == '/' || c == '\\') ? 1 : 0;
}

// 0x0809f7a0  size=259  FUN_0809f7a0  status=translated  naming=unknown
// naming evidence: no string; collapses separators, drops "." components and rejects ".." or "::" (returns 0). Descriptive name.
qboolean FS_SanitizeQpath(const char *in, char *out)
{
    int src;
    int dst;

    src = 0;
    while (FS_IsPathSeparator(in[src])) {
        src++;
    }
    dst = 0;
    for (;;) {
        if (in[src] == '\0') {
            out[dst] = '\0';
            return 1;
        }
        if (FS_IsParentPathPrefix(in + src)) {
            break;
        }
        if (in[src] != '.' || (in[src + 1] != '\0' && !FS_IsPathSeparator(in[src + 1]))) {
            if (!FS_IsPathSeparator(in[src])) {
                out[dst] = in[src];
            } else {
                out[dst] = '/';
                while (FS_IsPathSeparator(in[src + 1])) {
                    src++;
                }
            }
            dst++;
        }
        src++;
    }
    return 0;
}

// 0x0809f8a4  size=180  FUN_0809f8a4  status=translated  naming=unknown
// naming evidence: rodata list ".hlsl", ".txt", ".cfg", ".levelshots", ".menu", ".arena", ".str" terminated by "" in this body.
qboolean FS_HasTextAssetExtension(const char *name)
{
    const char *exts[8];
    int len;
    int i;
    size_t extLen;

    exts[0] = ".hlsl";
    exts[1] = ".txt";
    exts[2] = ".cfg";
    exts[3] = ".levelshots";
    exts[4] = ".menu";
    exts[5] = ".arena";
    exts[6] = ".str";
    exts[7] = "";

    len = ScrStr_Strlen(name);
    for (i = 0;; i++) {
        if (exts[i][0] == '\0') {
            return 0;
        }
        extLen = strlen(exts[i]);
        if (Q_stricmp(name + len - extLen, exts[i]) == 0) {
            break;
        }
    }
    return 1;
}

// 0x0809f958  size=2269  FUN_0809f958  status=translated  naming=observed
// naming evidence: rodata "FS_FOpenFileRead: %s (found in '%s/%s')", "FS_FOpenFileRead: %s (found in '%s')", "Can't find %s" in this body.
// note: when file is NULL this is an existence probe that returns 1 or -1 without opening a handle.
// note: the "EXE_UNPURECLIENTDETECTED" error is raised after the search if a restricted pack entry matched.
int FS_FOpenFileReadInternal(const char *filename, int *file, int uniqueFile, int quiet)
{
    char qpath[268];
    char ospath[256];
    char copypath[271];
    struct searchpath_s *search;
    struct iwd_s *pack;
    struct fileInIwd_s *entry;
    struct directory_s *dir;
    struct unz_s *state;
    struct iwd_s *unpurePack;
    const char *ext;
    FILE *fp;
    int hashIndex;
    int restrictedFound;
    int h;
    int savedFirstWord;
    char *msg;

    hashIndex = 0;
    restrictedFound = 0;
    unpurePack = NULL;

    FS_Nop();
    if (FS_SanitizeQpath(filename, qpath) == 0) {
        if (file != NULL) {
            *file = 0;
        }
        return -1;
    }

    if (file == NULL) {
        /* existence probe */
        for (search = fs_searchpaths; search != NULL; search = search->next) {
            if (FS_SearchPathAllowed(search) == 0) {
                continue;
            }
            if (search->iwd != NULL) {
                hashIndex = FS_HashFileName(qpath, search->iwd->hashSize);
            }
            if (search->iwd == NULL || search->iwd->hashTable[hashIndex] == NULL) {
                if (search->dir != NULL) {
                    dir = search->dir;
                    FS_BuildOSPath(dir->path, dir->gamedir, qpath, ospath, quiet);
                    fp = fopen(ospath, "rb");
                    if (fp != NULL) {
                        fclose(fp);
                        return 1;
                    }
                }
            } else {
                entry = search->iwd->hashTable[hashIndex];
                do {
                    if (FS_FilenameCompare(entry->name, qpath) == 0) {
                        return 1;
                    }
                    entry = entry->next;
                } while (entry != NULL);
            }
        }
        return -1;
    }

    /* open a handle slot and search the paths */
    h = FS_HandleForFile(quiet);
    *file = h;
    fsh[h].zipFile = uniqueFile;

    for (search = fs_searchpaths; search != NULL; search = search->next) {
        if (FS_SearchPathAllowed(search) == 0) {
            continue;
        }
        pack = search->iwd;
        if (pack != NULL) {
            hashIndex = FS_HashFileName(qpath, pack->hashSize);
        }

        if (pack == NULL || pack->hashTable[hashIndex] == NULL) {
            /* loose directory (or a pack bucket with no entries) */
            if (search->dir != NULL) {
                dir = search->dir;
                ext = COM_GetExtension(qpath);
                if ((fs_restrict->current.enabled == 0 && fs_numServerIwds == 0) || search->bLocalized != 0 ||
                    FS_IsUnrestrictedExtension(ext) != 0) {
                    FS_BuildOSPath(dir->path, dir->gamedir, qpath, ospath, quiet);
                    fsh[h].handleFiles = fopen(ospath, "rb");
                    if (fsh[h].handleFiles != NULL) {
                        if (search->bLocalized == 0 && FS_IsUnrestrictedExtension(ext) == 0) {
                            fs_unpureFileToken = rand() + 1;
                        }
                        Q_strncpyz(fsh[h].name, qpath, 0x100);
                        fsh[h].iwd = NULL;
                        if (fs_debug->current.integer != 0 && quiet == 0) {
                            Com_Printf("FS_FOpenFileRead: %s (found in '%s/%s')\n", qpath, dir->path, dir->gamedir);
                        }
                        if (fs_copyfiles->current.enabled != 0 && Q_stricmp(dir->path, fs_cdpath->current.string) == 0) {
                            FS_BuildOSPath(fs_basepath->current.string, dir->gamedir, qpath, copypath, quiet);
                            FS_CopyFile(ospath, copypath);
                        }
                        return FS_filelength(h);
                    }
                } else if (restrictedFound == 0) {
                    FS_BuildOSPath(dir->path, dir->gamedir, qpath, ospath, quiet);
                    fp = fopen(ospath, "rb");
                    if (fp != NULL) {
                        restrictedFound = 1;
                        fclose(fp);
                    }
                }
            }
        } else {
            /* pack: walk the hash bucket for this name */
            entry = pack->hashTable[hashIndex];
            do {
                if (FS_FilenameCompare(entry->name, qpath) == 0) {
                    if (search->bLocalized != 0 || FS_ChecksumAllowed(pack) != 0) {
                        if (pack->referenced == 0 && FS_HasTextAssetExtension(qpath) == 0) {
                            pack->referenced = 1;
                        }
                        if (uniqueFile == 0) {
                            fsh[h].handleFiles = pack->handle;
                        } else {
                            fsh[h].handleFiles = unzReOpen(pack->iwdFilename, pack->handle);
                            if (fsh[h].handleFiles == NULL) {
                                if (quiet != 0) {
                                    FS_FCloseFile(h);
                                    *file = 0;
                                    return -1;
                                }
                                Com_Error(0, "Couldn't reopen %s", pack->iwdFilename);
                            }
                        }
                        Q_strncpyz(fsh[h].name, qpath, 0x100);
                        fsh[h].iwd = pack;
                        state = (struct unz_s *)fsh[h].handleFiles;
                        savedFirstWord = state->firstWord;
                        unzSetOffset(pack->handle, entry->filePos);
                        if ((void *)state != pack->handle) {
                            Com_Memcpy(state, pack->handle, 0x80);
                        }
                        state->firstWord = savedFirstWord;
                        unzOpenCurrentFile(state);
                        fsh[h].zipFilePos = entry->filePos;
                        if (fs_debug->current.integer != 0 && quiet == 0) {
                            Com_Printf("FS_FOpenFileRead: %s (found in '%s')\n", qpath, pack->iwdFilename);
                        }
                        return state->fileSize;
                    }
                    unpurePack = pack;
                    break;
                }
                entry = entry->next;
            } while (entry != NULL);
        }
    }

    if (fs_debug->current.integer != 0 && quiet == 0) {
        Com_Printf("Can't find %s\n", filename);
    }
    *file = 0;
    if (unpurePack != NULL) {
        msg = va("EXE_UNPURECLIENTDETECTED\x15\n%s", unpurePack->iwdFilename);
        Com_Error(1, msg);
    }
    if (restrictedFound == 0) {
        return -1;
    }
    return -2;
}

// 0x080a0266  size=57  FUN_080a0266  status=translated  naming=inferred
// naming evidence: sets fs_openFileReadCalled then forwards to FS_FOpenFileReadInternal with quiet=0; Q3 FS_FOpenFileRead wrapper shape.
int FS_OpenFileRead(const char *filename, int *file, int uniqueFile)
{
    fs_openFileReadCalled = 1;
    return FS_FOpenFileReadInternal(filename, file, uniqueFile, 0);
}

// 0x080a02a0  size=70  FUN_080a02a0  status=translated  naming=inferred
// naming evidence: opens through FS_OpenFileRead and closes the handle; returns whether the file is found.
qboolean FS_SearchFileExists(const char *filename)
{
    int h;

    FS_OpenFileRead(filename, &h, 0);
    if (h != 0) {
        FS_FCloseFile(h);
    }
    return h != 0;
}

// 0x080a02e6  size=245  FUN_080a02e6  status=translated  naming=unknown
// naming evidence: returns "%s/%s" (gamedir, filename) for the first loose directory that holds the file. Descriptive name.
char *FS_LooseFileGamePath(const char *filename)
{
    struct searchpath_s *search;
    struct directory_s *dir;
    char ospath[268];
    FILE *fp;

    for (search = fs_searchpaths; search != NULL; search = search->next) {
        if (FS_SearchPathAllowed(search) != 0 && search->dir != NULL) {
            dir = search->dir;
            FS_BuildOSPath_Fatal(dir->path, dir->gamedir, filename, ospath);
            fp = fopen(ospath, "rb");
            if (fp != NULL) {
                fclose(fp);
                return va("%s/%s", dir->gamedir, filename);
            }
        }
    }
    return NULL;
}

// 0x080a03dc  size=124  FUN_080a03dc  status=translated  naming=inferred
// naming evidence: remove() on the homepath OS path; returns 0 for an empty name or a failed remove.
qboolean FS_DeleteFile(const char *filename)
{
    char ospath[264];

    FS_Nop();
    if (*filename == '\0') {
        return 0;
    }
    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, filename, ospath);
    if (remove(ospath) == -1) {
        return 0;
    }
    return 1;
}

// 0x080a0458  size=209  FUN_080a0458  status=translated  naming=unknown
// naming evidence: stat, then chmod with owner-read bit (0x100) set (clear=0) or cleared (clear!=0). Descriptive name.
qboolean FS_SetOwnerReadBit(const char *filename, int clear)
{
    char ospath[264];
    struct stat st;
    mode_t mode;

    FS_Nop();
    FS_BuildOSPath_Fatal(fs_homepath->current.string, fs_gamedir, filename, ospath);
    if (FS_StatPath(ospath, &st) == -1) {
        return 0;
    }
    if (clear == 0) {
        mode = st.st_mode | 0x100;
    } else {
        mode = st.st_mode & 0xfffffeff;
    }
    st.st_mode = mode;
    if (chmod(ospath, mode) == -1) {
        return 0;
    }
    return 1;
}

// 0x080a052a  size=299  FUN_080a052a  status=translated  naming=observed
// naming evidence: rodata "FS_Read: -1 bytes read" in this body.
int FS_Read(void *buffer, int len, int f)
{
    char *dst;
    FILE *fp;
    int remaining;
    int n;
    int sawZero;

    FS_Nop();
    if (f == 0) {
        return 0;
    }
    if (fsh[f].iwd != NULL) {
        return unzReadCurrentFile((struct unz_s *)fsh[f].handleFiles, buffer, len);
    }

    fp = (FILE *)FS_FileForHandle(f);
    dst = (char *)buffer;
    sawZero = 0;
    for (remaining = len; remaining != 0; remaining -= n) {
        n = (int)fread(dst, 1, remaining, fp);
        if (n == 0) {
            if (sawZero != 0) {
                return len - remaining;
            }
            sawZero = 1;
        }
        if (n == -1) {
            if (f > 0x32 && f < 0x40) {
                return -1;
            }
            Com_Error(0, "\x15" "FS_Read: -1 bytes read");
        }
        dst += n;
    }
    return len;
}

// 0x080a0656  size=213  FUN_080a0656  status=translated  naming=inferred
// naming evidence: fwrite loop with a flush when the slot has handleSync set; Q3 FS_Write shape.
int FS_Write(const void *buffer, int len, int f)
{
    FILE *fp;
    const char *src;
    int remaining;
    int n;
    int sawZero;

    FS_Nop();
    if (f == 0) {
        return 0;
    }
    fp = (FILE *)FS_FileForHandle(f);
    src = (const char *)buffer;
    sawZero = 0;
    for (remaining = len; remaining != 0; remaining -= n) {
        n = (int)fwrite(src, 1, remaining, fp);
        if (n == 0) {
            if (sawZero != 0) {
                return 0;
            }
            sawZero = 1;
        }
        if (n == -1) {
            return 0;
        }
        src += n;
    }
    if (fsh[f].handleSync != 0) {
        fflush(fp);
    }
    return len;
}

// 0x080a072c  size=94  FUN_080a072c  status=translated  naming=inferred
// naming evidence: vsnprintf into a 4 KiB buffer then FS_Write of the formatted length; Q3 FS_Printf shape.
void FS_Printf(int f, const char *fmt, ...)
{
    char msg[4108];
    va_list args;
    int len;

    va_start(args, fmt);
    vsnprintf(msg, 0x1000, fmt, args);
    va_end(args);
    len = ScrStr_Strlen(msg);
    FS_Write(msg, len, f);
}

// 0x080a078a  size=785  FUN_080a078a  status=translated  naming=inferred
// naming evidence: origin codes 0=CUR, 1=END, 2=SET (Q3 fsOrigin_t values); pack branch re-opens the zip entry to rewind.
int FS_Seek(int f, int offset, int origin)
{
    FILE *fp;
    struct unz_s *zip;
    int pos;
    int size;
    int whence;

    FS_Nop();
    if (fsh[f].streamed != 0) {
        fsh[f].streamed = 0;
        Sys_Fseek(f, offset, origin);
        fsh[f].streamed = 1;
    }

    if (fsh[f].iwd == NULL) {
        fp = (FILE *)FS_FileForHandle(f);
        if (origin == COD2_FS_SEEK_END) {
            whence = SEEK_END;
        } else if (origin == COD2_FS_SEEK_CUR) {
            whence = SEEK_CUR;
        } else if (origin == COD2_FS_SEEK_SET) {
            whence = SEEK_SET;
        } else {
            return 0;
        }
        return fseek(fp, offset, whence);
    }

    zip = (struct unz_s *)fsh[f].handleFiles;
    if (offset == 0 && origin == COD2_FS_SEEK_SET) {
        unzSetOffset(zip, fsh[f].zipFilePos);
        return unzOpenCurrentFile(zip);
    }
    if (offset == 0 && origin == COD2_FS_SEEK_CUR) {
        return 0;
    }

    pos = unzTellCurrentFile(zip);
    if (origin == COD2_FS_SEEK_CUR) {
        if (offset < 0) {
            unzSetOffset(zip, fsh[f].zipFilePos);
            unzOpenCurrentFile(zip);
            pos = offset + pos;
        } else {
            pos = offset;
        }
    } else if (origin == COD2_FS_SEEK_END) {
        size = FS_filelength(f);
        if (size + offset < pos) {
            unzSetOffset(zip, fsh[f].zipFilePos);
            unzOpenCurrentFile(zip);
            size = FS_filelength(f);
            pos = size + offset;
        } else {
            size = FS_filelength(f);
            pos = (size + offset) - pos;
        }
    } else {
        if (origin != COD2_FS_SEEK_SET) {
            return -1;
        }
        if (offset < pos) {
            unzSetOffset(zip, fsh[f].zipFilePos);
            unzOpenCurrentFile(zip);
            pos = offset;
        } else {
            pos = offset - pos;
        }
    }
    if (unzReadCurrentFile(zip, NULL, pos) == 0) {
        return -1;
    }
    return 0;
}

// 0x080a0a9c  size=221  FUN_080a0a9c  status=translated  naming=observed
// naming evidence: rodata "FS_ReadFile with empty name" in this body.
int FS_ReadFile(const char *qpath, void **buffer)
{
    int h;
    int len;
    char *buf;

    FS_Nop();
    if (qpath == NULL || *qpath == '\0') {
        Com_Error(0, "\x15" "FS_ReadFile with empty name\n");
    }

    len = FS_OpenFileRead(qpath, &h, 0);
    if (h == 0) {
        if (buffer != NULL) {
            *buffer = NULL;
        }
        len = -1;
    } else if (buffer == NULL) {
        FS_FCloseFile(h);
    } else {
        fs_loadStack++;
        buf = (char *)Hunk_AllocateTempMemory(len + 1);
        *buffer = buf;
        FS_Read(buf, len, h);
        buf[len] = '\0';
        FS_FCloseFile(h);
    }
    return len;
}

// 0x080a0b7a  size=15  FUN_080a0b7a  status=translated  naming=unknown
// naming evidence: zeroes fs_loadStack (fs_loadStack) and nothing else. Descriptive name.
void FS_ResetLoadStack(void)
{
    fs_loadStack = 0;
}

// 0x080a0b8a  size=30  FUN_080a0b8a  status=translated  naming=inferred
// naming evidence: decrements the load stack and releases the temp block; Q3 FS_FreeFile shape.
void FS_FreeFile(void *buffer)
{
    FS_Nop();
    fs_loadStack--;
    Hunk_FreeTempMemory((int *)buffer);
}

// 0x080a0ba8  size=138  FUN_080a0ba8  status=translated  naming=inferred
// naming evidence: opens a homepath file, writes the buffer, deletes the file on a short write; Q3 FS_WriteFile shape.
qboolean FS_WriteFile(const char *filename, const void *buffer, int size)
{
    int f;
    int written;

    FS_Nop();
    f = FS_FOpenFileWrite(filename);
    if (f == 0) {
        Com_Printf("Failed to open %s\n", filename);
        return 0;
    }
    written = FS_Write(buffer, size, f);
    FS_FCloseFile(f);
    if (written == size) {
        return 1;
    }
    FS_DeleteFile(filename);
    return 0;
}

// 0x080a0c32  size=290  FUN_080a0c32  status=translated  naming=unknown
// naming evidence: no string; probes loose directory search paths for the file and reports the OS path. Descriptive name.
int FS_LocateLooseFile(const char *qpath, char *ospath)
{
    char sanitized[264];
    struct searchpath_s *search;
    FILE *fp;

    if (FS_SanitizeQpath(qpath, sanitized) != 0) {
        for (search = fs_searchpaths; search != NULL; search = search->next) {
            if (FS_SearchPathAllowed(search) != 0 && search->iwd == NULL) {
                FS_BuildOSPath(search->dir->path, search->dir->gamedir, sanitized, ospath, 0);
                fp = fopen(ospath, "rb");
                if (fp != NULL) {
                    fclose(fp);
                    return 0;
                }
            }
        }
    }
    return -1;
}

// 0x080a0d54  size=10  FUN_080a0d54  status=translated  naming=unknown
// naming evidence: returns 0 with no other effect. Descriptive name for a stub.
int FS_ReturnZeroStub(void)
{
    return 0;
}

// 0x080a0d5e  size=1351  FUN_080a0d5e  status=translated  naming=inferred
// naming evidence: opens a zip, reads the central directory, builds a hashed entry list and checksums; Q3 FS_LoadZipFile shape.
// note: the pack header is malloc-style (Com_MallocZeroed) and its referenced byte is not set here (preserved).
struct iwd_s *FS_LoadPackFile(const char *pakFilename, const char *pakBasename)
{
    struct unz_s *zip;
    uint32 numEntries[2];
    struct unz_file_info_s info;
    char name[256];
    struct iwd_s *pack;
    struct fileInIwd_s *entries;
    struct fileInIwd_s *entry;
    char *nameCursor;
    uint32 *crcs;
    uint32 hashSize;
    uint32 i;
    int nameBytes;
    int crcCount;
    int bucket;
    int err;
    int len;

    zip = unzOpen(pakFilename);
    err = unzGetGlobalInfo(zip, (struct unz_global_info_s *)numEntries);
    if (err != 0) {
        return NULL;
    }

    fs_iwdFileCount += (int)numEntries[0];

    nameBytes = 0;
    unzGoToFirstFile(zip);
    i = 0;
    while (i < numEntries[0] && (err = unzGetCurrentFileInfo(zip, &info, name, 0x100, 0, 0, 0, 0)) == 0) {
        len = ScrStr_Strlen(name);
        nameBytes = len + nameBytes + 1;
        unzGoToNextFile(zip);
        i++;
    }

    entries = (struct fileInIwd_s *)Com_MallocZeroed((int)(numEntries[0] * 0xc + nameBytes));
    nameCursor = (char *)entries + numEntries[0] * 0xc;

    crcs = (uint32 *)Com_MallocZeroed((int)(numEntries[0] << 2));

    for (hashSize = 1; hashSize < 0x401 && hashSize <= numEntries[0]; hashSize <<= 1) {
    }

    pack = (struct iwd_s *)Com_MallocZeroed((int)(hashSize * 4 + COD2_FS_PACK_HEADER_SIZE));
    pack->hashSize = (int)hashSize;
    pack->hashTable = (struct fileInIwd_s **)((char *)pack + COD2_FS_PACK_HEADER_SIZE);
    for (i = 0; i < hashSize; i++) {
        pack->hashTable[i] = NULL;
    }

    Q_strncpyz(pack->iwdFilename, pakFilename, 0x100);
    Q_strncpyz(pack->iwdBasename, pakBasename, 0x100);
    len = (int)strlen(pack->iwdBasename);
    if (len > 4) {
        len = (int)strlen(pack->iwdBasename);
        if (Q_stricmp(pack->iwdBasename + len - 4, ".iwd") == 0) {
            len = (int)strlen(pack->iwdBasename);
            pack->iwdBasename[len - 4] = '\0';
        }
    }

    pack->handle = zip;
    pack->numfiles = (int)numEntries[0];

    unzGoToFirstFile(zip);
    i = 0;
    crcCount = 0;
    while (i < numEntries[0] && (err = unzGetCurrentFileInfo(zip, &info, name, 0x100, 0, 0, 0, 0)) == 0) {
        if (info.uncompressedSize != 0) {
            crcs[crcCount] = (uint32)Int_Identity_080a3748((int)info.crc);
            crcCount++;
        }
        Q_strlwr(name);
        bucket = FS_HashFileName(name, pack->hashSize);
        entry = &entries[i];
        entry->name = nameCursor;
        strcpy(entry->name, name);
        len = ScrStr_Strlen(name);
        nameCursor = nameCursor + len + 1;
        unzGetOffset(zip, (uint32 *)entry);
        entry->next = pack->hashTable[bucket];
        pack->hashTable[bucket] = entry;
        unzGoToNextFile(zip);
        i++;
    }

    pack->checksum = (uint32)Com_BlockChecksum(crcs, crcCount << 2);
    pack->pure_checksum = (uint32)Com_BlockChecksumKey(crcs, crcCount << 2, Int_Identity_080a3748(fs_checksumFeed));
    pack->checksum = (uint32)Int_Identity_080a3748((int)pack->checksum);
    pack->pure_checksum = (uint32)Int_Identity_080a3748((int)pack->pure_checksum);
    Com_Free_A(crcs);

    pack->buildBuffer = entries;
    return pack;
}

// 0x080a12a6  size=142  FUN_080a12a6  status=translated  naming=unknown
// naming evidence: no string; copies the path, cuts it at the last separator and counts separators. Descriptive name.
int FS_SplitDirectory(const char *path, char *dest, int *depth)
{
    int count;
    int lastSep;
    int i;

    count = 0;
    lastSep = 0;
    dest[0] = '\0';
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' || path[i] == '\\') {
            lastSep = i;
            count++;
        }
    }
    strcpy(dest, path);
    dest[lastSep] = '\0';
    if (lastSep + 1 == i) {
        count--;
    }
    *depth = count;
    return lastSep;
}

// 0x080a1334  size=135  FUN_080a1334  status=translated  naming=inferred
// naming evidence: appends a duplicate-free copy of name to the list (cap 0xfff entries); Q3 FS_AddFileToList shape.
int FS_AddFileToList(const char *name, char **list, int count)
{
    int i;

    if (count == COD2_FS_MAX_FOUND_FILES - 1) {
        return count;
    }
    for (i = 0; i < count; i++) {
        if (Q_stricmp(name, list[i]) == 0) {
            return count;
        }
    }
    list[count] = CopyString(name);
    return count + 1;
}

// 0x080a13bc  size=1535  FUN_080a13bc  status=translated  naming=inferred
// naming evidence: walks the search-path list, lists loose directories with Sys_ListFiles and pack entries from their name buckets; Q3 FS_ListFilteredFiles shape.
// note: the trailing unusedArg is passed by the binary but never read (dead argument, preserved in the signature).
// note: extension "/" selects directory listing. The directory copy is unbounded and clears the last byte of the copy (preserved).
char **FS_ListFilteredFilesInList(struct searchpath_s *head, const char *path, const char *extension,
                                  const char *filter, int allowNonPure, int *numfiles, int unusedArg)
{
    char sanitized[264];
    char dirBuf[268];
    char osDir[255];
    char extTmp[65];
    char *list[COD2_FS_MAX_FOUND_FILES + 3];
    char **found;
    char **result;
    struct searchpath_s *node;
    struct directory_s *dir;
    struct iwd_s *pack;
    struct fileInIwd_s *entries;
    char *entryName;
    int wantDirs;
    int pathLen;
    int extLen;
    int depth;
    int prefixLen;
    int count;
    int foundCount;
    int sepIndex;
    int entryDepth;
    int entryLen;
    int i;

    FS_Nop();
    if (path == NULL) {
        *numfiles = 0;
        return NULL;
    }
    if (extension == NULL) {
        extension = "";
    }
    if (FS_SanitizeQpath(path, sanitized) == 0) {
        *numfiles = 0;
        return NULL;
    }

    wantDirs = (Q_stricmp(extension, "/") == 0);

    pathLen = (int)strlen(sanitized);
    if (pathLen != 0 && (sanitized[pathLen - 1] == '\\' || sanitized[pathLen - 1] == '/')) {
        pathLen--;
    }
    extLen = (int)strlen(extension);

    count = 0;
    FS_SplitDirectory(sanitized, dirBuf, &depth);
    if (sanitized[0] != '\0') {
        depth++;
    }

    for (node = head; node != NULL; node = node->next) {
        if (FS_SearchPathAllowed(node) == 0) {
            continue;
        }

        if (node->iwd == NULL) {
            if (node->dir != NULL && ((fs_restrict->current.enabled == 0 && fs_numServerIwds == 0) || allowNonPure != 0)) {
                dir = node->dir;
                FS_BuildOSPath_Fatal(dir->path, dir->gamedir, sanitized, osDir);
                found = Sys_ListFiles(osDir, extension, filter, &foundCount, wantDirs);
                for (i = 0; i < foundCount; i++) {
                    count = FS_AddFileToList(found[i], list, count);
                }
                Sys_FreeFileList(found);
            }
        } else if (node->bLocalized != 0 || FS_ChecksumAllowed(node->iwd) != 0) {
            pack = node->iwd;
            entries = pack->buildBuffer;
            for (i = 0; i < pack->numfiles; i++) {
                entryName = entries[i].name;
                if (filter == NULL) {
                    sepIndex = FS_SplitDirectory(entryName, dirBuf, &entryDepth);
                    if (entryDepth == depth && pathLen <= sepIndex &&
                        (pathLen < 1 || entryName[pathLen] == '/') &&
                        Q_stricmpn(entryName, sanitized, pathLen) == 0) {
                        if (wantDirs == 0) {
                            if (extLen == 0) {
                                prefixLen = (pathLen != 0) ? pathLen + 1 : 0;
                                count = FS_AddFileToList(entryName + prefixLen, list, count);
                            } else {
                                entryLen = (int)strlen(entryName);
                                if (extLen < entryLen && entryName[entryLen - extLen - 1] == '.' &&
                                    Q_stricmp(entryName + (entryLen - extLen), extension) == 0) {
                                    prefixLen = (pathLen != 0) ? pathLen + 1 : 0;
                                    count = FS_AddFileToList(entryName + prefixLen, list, count);
                                }
                            }
                        } else {
                            entryLen = (int)strlen(entryName);
                            if (entryName[entryLen - 1] == '/') {
                                prefixLen = (pathLen != 0) ? pathLen + 1 : 0;
                                strcpy(extTmp + 1, entryName + prefixLen);
                                entryLen = (int)strlen(extTmp + 1);
                                extTmp[entryLen] = '\0';
                                count = FS_AddFileToList(extTmp + 1, list, count);
                            }
                        }
                    }
                } else {
                    if (Com_FilterPath(filter, entryName, 0) != 0) {
                        count = FS_AddFileToList(entryName, list, count);
                    }
                }
            }
        }
    }

    *numfiles = count;
    if (count == 0) {
        return NULL;
    }
    result = (char **)Com_MallocZeroed(count * 4 + 4);
    for (i = 0; i < count; i++) {
        result[i] = list[i];
    }
    result[i] = NULL;
    return result;
}

// 0x080a19bc  size=64  FUN_080a19bc  status=translated  naming=inferred
// naming evidence: forwards the head search-path list with no filter; Q3 FS_ListFiles shape.
// note: the binary tail-forwards the result of 0x080a13bc, which the caller in 0x080a1d4a uses.
char **FS_ListFiles(const char *path, const char *extension, int allowNonPure, int *numfiles, int unusedArg)
{
    return FS_ListFilteredFilesInList(fs_searchpaths, path, extension, NULL, allowNonPure, numfiles, unusedArg);
}

char **FS_ListFilesFilteredByGame(const char *path, const char *extension, const char *filter,
                                  int allowNonPure, int *numfiles, unsigned int mask, int unusedArg);

// 0x080a19fc  size=62  FUN_080a19fc  status=translated  naming=unknown
// naming evidence: forwards to the game-mask filtered lister with no name filter. Descriptive name.
// note: the binary has no explicit return here; the forwarded result is returned (tail call).
char **FS_ListFilesByGameMask(const char *path, const char *extension, int allowNonPure, int *numfiles,
                              unsigned int mask, int unusedArg)
{
    return FS_ListFilesFilteredByGame(path, extension, NULL, allowNonPure, numfiles, mask, unusedArg);
}

// 0x080a1a3a  size=345  FUN_080a1a3a  status=translated  naming=inferred
// naming evidence: rodata "main", "dev", "temp", "raw", "raw_shared", "devraw" tested as prefixes per mask bit; mask 0x3f matches all.
qboolean FS_GameNameMatchesMask(const char *name, unsigned int mask)
{
    if (mask == 0x3f) {
        return 1;
    }
    if ((mask & 0x01) != 0 && Q_strncmp(name, "main", 4) == 0) {
        return 1;
    }
    if ((mask & 0x02) != 0 && Q_strncmp(name, "dev", 3) == 0) {
        return 1;
    }
    if ((mask & 0x04) != 0 && Q_strncmp(name, "temp", 4) == 0) {
        return 1;
    }
    if ((mask & 0x08) != 0 && Q_strncmp(name, "raw", 3) == 0) {
        return 1;
    }
    if ((mask & 0x10) != 0 && Q_strncmp(name, "raw_shared", 10) == 0) {
        return 1;
    }
    if ((mask & 0x20) != 0 && Q_strncmp(name, "devraw", 6) == 0) {
        return 1;
    }
    return 0;
}

// 0x080a1b94  size=345  FUN_080a1b94  status=translated  naming=unknown
// naming evidence: copies the search-path nodes whose game name matches the mask, lists them, frees the copies. Descriptive name.
char **FS_ListFilesFilteredByGame(const char *path, const char *extension, const char *filter,
                                  int allowNonPure, int *numfiles, unsigned int mask, int unusedArg)
{
    struct searchpath_s *copyHead;
    struct searchpath_s *copyTail;
    struct searchpath_s *node;
    struct searchpath_s *copy;
    struct searchpath_s *next;
    const char *gameName;
    char **result;

    copyHead = NULL;
    copyTail = NULL;
    for (node = fs_searchpaths; node != NULL; node = node->next) {
        if (node->dir == NULL) {
            gameName = (node->iwd == NULL) ? NULL : node->iwd->iwdGamename;
        } else {
            gameName = node->dir->gamedir;
        }
        if (FS_GameNameMatchesMask(gameName, mask) != 0) {
            copy = (struct searchpath_s *)Com_MallocZeroed(0x14);
            if (copyHead == NULL) {
                copyHead = copy;
            } else {
                copyTail->next = copy;
            }
            copyTail = copy;
            copy->next = NULL;
            copy->dir = node->dir;
            copy->language = node->language;
            copy->bLocalized = node->bLocalized;
            copy->iwd = node->iwd;
        }
    }

    result = FS_ListFilteredFilesInList(copyHead, path, extension, filter, allowNonPure, numfiles, unusedArg);
    while (copyHead != NULL) {
        next = copyHead->next;
        Com_Free_A(copyHead);
        copyHead = next;
    }
    return result;
}

// 0x080a1cee  size=91  FUN_080a1cee  status=translated  naming=inferred
// naming evidence: frees each string of a NULL-terminated list, then the list; Q3 FS_FreeFileList shape.
void FS_FreeFileList(char **list, int tag)
{
    int i;

    FS_Nop();
    if (list != NULL) {
        for (i = 0; list[i] != NULL; i++) {
            Com_Free_A(list[i]);
        }
        Com_Free_A(list);
    }
}

// 0x080a1d4a  size=269  FUN_080a1d4a  status=translated  naming=inferred
// naming evidence: rodata "$modlist" selects the mod-list mode, which packs NUL-separated names into the output buffer.
int FS_GetFileList(const char *path, const char *extension, int allowNonPure, char *out, int outSize)
{
    char **list;
    int numfiles;
    int used;
    int i;
    int len;
    int ret;

    numfiles = 0;
    used = 0;
    *out = '\0';
    if (Q_stricmp(path, "$modlist") == 0) {
        ret = FS_GetModList(out, outSize);
    } else {
        list = FS_ListFiles(path, extension, allowNonPure, &numfiles, 3);
        for (i = 0; i < numfiles; i++) {
            len = ScrStr_Strlen(list[i]) + 1;
            if (outSize <= len + used + 1) {
                numfiles = i;
                break;
            }
            strcpy(out, list[i]);
            out += len;
            used += len;
        }
        FS_FreeFileList(list, 3);
        ret = numfiles;
    }
    return ret;
}

// 0x080a1e58  size=45  FUN_080a1e58  status=translated  naming=inferred
// naming evidence: rewrites '\\' and ':' to '/' in place; Q3 FS_ReplaceSeparators shape.
void FS_NormalizeSeparators(char *path)
{
    for (; *path != '\0'; path++) {
        if (*path == '\\' || *path == ':') {
            *path = '/';
        }
    }
}

// 0x080a1e86  size=173  FUN_080a1e86  status=translated  naming=inferred
// naming evidence: case-folded ordering with '\\' and ':' as '/'; returns -1, 0 or 1; Q3 FS_PathCmp shape.
int FS_PathCmp(const char *s1, const char *s2)
{
    int c1;
    int c2;

    do {
        c1 = *s1++;
        c2 = *s2++;
        if (Q_islower(c1) != 0) {
            c1 -= 0x20;
        }
        if (Q_islower(c2) != 0) {
            c2 -= 0x20;
        }
        if (c1 == '\\' || c1 == ':') {
            c1 = '/';
        }
        if (c2 == '\\' || c2 == ':') {
            c2 = '/';
        }
        if (c1 < c2) {
            return -1;
        }
        if (c2 < c1) {
            return 1;
        }
    } while (c1 != 0);
    return 0;
}

// 0x080a1f34  size=282  FUN_080a1f34  status=translated  naming=inferred
// naming evidence: insertion sort by FS_PathCmp into a temporary array, then copied back; Q3 sort-file-list shape.
void FS_SortFileList(char **list, int count)
{
    char **sorted;
    int n;
    int c;
    int pos;
    int k;

    sorted = (char **)Com_MallocZeroed(count * 4 + 4);
    sorted[0] = NULL;
    n = 0;
    for (c = 0; c < count; c++) {
        pos = 0;
        while (pos < n && FS_PathCmp(list[c], sorted[pos]) >= 0) {
            pos++;
        }
        for (k = n; pos < k; k--) {
            sorted[k] = sorted[k - 1];
        }
        sorted[pos] = list[c];
        n++;
    }
    Com_Memcpy(list, sorted, count << 2);
    Com_Free_A(sorted);
}

// 0x080a204e  size=452  FUN_080a204e  status=translated  naming=inferred
// naming evidence: rodata "Current search path:", "Current language: %s", "File Handles:", "handle %i: %s"; reports the search path and handles.
void FS_PrintSearchPathInfo(int pureOnly)
{
    struct searchpath_s *node;
    int i;

    Com_Printf("Current language: %s\n", LocLanguage_Name(Loc_GetLanguage()));
    if (fs_ignoreLocalized->current.enabled != 0) {
        Com_Printf("    localized assets are being ignored\n");
    }
    Com_Printf("Current search path:\n");
    for (node = fs_searchpaths; node != NULL; node = node->next) {
        if (pureOnly == 0 || FS_SearchPathAllowed(node) != 0) {
            if (node->iwd == NULL) {
                Com_Printf("%s/%s\n", node->dir->path, node->dir->gamedir);
                if (node->bLocalized != 0) {
                    Com_Printf("    localized assets game folder for %s\n", LocLanguage_Name(node->language));
                }
            } else {
                Com_Printf("%s (%i files)\n", node->iwd->iwdFilename, node->iwd->numfiles);
                if (node->bLocalized != 0) {
                    Com_Printf("    localized assets iwd file for %s\n", LocLanguage_Name(node->language));
                }
                if (fs_numServerIwds != 0) {
                    if (FS_ChecksumAllowed(node->iwd) == 0) {
                        Com_Printf("    not on the pure list\n");
                    } else {
                        Com_Printf("    on the pure list\n");
                    }
                }
            }
        }
    }
    Com_Printf("\nFile Handles:\n");
    for (i = 1; i < COD2_FS_MAX_HANDLES; i++) {
        if (fsh[i].handleFiles != NULL) {
            Com_Printf("handle %i: %s\n", i, fsh[i].name);
        }
    }
}

// 0x080a2212  size=20  FUN_080a2212  status=translated  naming=inferred
// naming evidence: calls the search-path report with pureOnly=0; command-style wrapper.
void FS_PrintSearchPathsCmd(void)
{
    FS_PrintSearchPathInfo(0);
}

// 0x080a2226  size=20  FUN_080a2226  status=translated  naming=inferred
// naming evidence: calls the search-path report with pureOnly=1.
void FS_PrintPureSearchPaths(void)
{
    FS_PrintSearchPathInfo(1);
}

// 0x080a223a  size=180  FUN_080a223a  status=translated  naming=inferred
// naming evidence: parses the language from an "localized_<language>_..." name (offset 10), into one of two alternating buffers.
char *FS_GetLocalizedLanguage(const char *name)
{
    size_t len;
    int i;

    fs_languageNameBufIndex ^= 1;
    len = strlen(name);
    if (len < 10) {
        fs_languageNameBuf[fs_languageNameBufIndex][0] = '\0';
    } else {
        memset(fs_languageNameBuf[fs_languageNameBufIndex], 0, 0x40);
        i = 10;
        while (i < 0x40 && name[i] != '\0' && isalpha((int)name[i])) {
            fs_languageNameBuf[fs_languageNameBufIndex][i - 10] = name[i];
            i++;
        }
    }
    return fs_languageNameBuf[fs_languageNameBufIndex];
}

// 0x080a22ee  size=229  FUN_080a22ee  status=translated  naming=inferred
// naming evidence: qsort comparator for localized iwd names; ignored-prefix names sort by language, "english" first, then FS_PathCmp.
int FS_CompareLocalizedIwd(const void *pa, const void *pb)
{
    const char *a;
    const char *b;
    const char *langA;
    const char *langB;

    a = *(const char *const *)pa;
    b = *(const char *const *)pb;
    if (Q_strncmp(a, "          ", 10) == 0 && Q_strncmp(b, "          ", 10) == 0) {
        langA = FS_GetLocalizedLanguage(a);
        langB = FS_GetLocalizedLanguage(b);
        if (Q_stricmp(langA, "english") == 0) {
            if (Q_stricmp(langB, "english") != 0) {
                return -1;
            }
        } else {
            if (Q_stricmp(langB, "english") == 0) {
                return 1;
            }
        }
    }
    return FS_PathCmp(a, b);
}

// 0x080a23d4  size=71  FUN_080a23d4  status=translated  naming=inferred
// naming evidence: inserts at the head, or after the leading non-localized nodes for a localized node; Q3 search-path insertion shape.
void FS_InsertSearchPath(struct searchpath_s *node)
{
    struct searchpath_s **link;

    link = &fs_searchpaths;
    if (node->bLocalized != 0) {
        while (*link != NULL && (*link)->bLocalized == 0) {
            link = &(*link)->next;
        }
    }
    node->next = *link;
    *link = node;
}

// 0x080a241c  size=914  FUN_080a241c  status=translated  naming=inferred
// naming evidence: rodata "WARNING: Exceeded max number of iwd files", "localized_" and the localized-name warnings; loads iwd packs from a folder.
// note: the trailing-slash cut is a byte write one past the string end, so it removes the last character (preserved).
// note: the binary's local name array is declared with 1023 slots but the loop writes index 1023; this uses 1024 slots.
void FS_AddIwdFiles(const char *base, const char *gamedir)
{
    char pathbuf[256];
    char *names[COD2_FS_MAX_IWD_FILES];
    char **list;
    char *langName;
    struct iwd_s *pack;
    struct searchpath_s *node;
    int count;
    int localized;
    int language;
    int add;
    int len;
    int i;
    int k;

    FS_BuildOSPath_Fatal(base, gamedir, "", pathbuf);
    len = (int)strlen(pathbuf);
    pathbuf[len - 1] = '\0';

    list = Sys_ListFiles(pathbuf, "iwd", NULL, &count, 0);
    if (count > COD2_FS_MAX_IWD_FILES) {
        Com_Printf("WARNING: Exceeded max number of iwd files in %s/%s (%1/%1)\n", base, gamedir,
                    count, COD2_FS_MAX_IWD_FILES);
        count = COD2_FS_MAX_IWD_FILES;
    }

    for (i = 0; i < count; i++) {
        names[i] = list[i];
        if (Q_strncmp(names[i], "localized_", 10) == 0) {
            memcpy(names[i], "          ", 10);
        }
    }
    qsort(names, count, 4, FS_CompareLocalizedIwd);

    for (i = 0; i < count; i++) {
        add = 0;
        localized = 0;
        language = 0;
        if (Q_strncmp(names[i], "          ", 10) == 0) {
            memcpy(names[i], "localized_", 10);
            localized = 1;
            langName = FS_GetLocalizedLanguage(names[i]);
            if (*langName == '\0') {
                Com_Printf("WARNING: Localized assets iwd file %s/%s/%s has invalid name (no language specified). Proper naming convention is: localized_[language]_iwd#.iwd\n",
                            base, gamedir, names[i]);
            } else if (LocLanguage_Find(langName, &language) != 0) {
                add = 1;
            } else {
                Com_Printf("WARNING: Localized assets iwd file %s/%s/%s has invalid name (bad language name specified). Proper naming convention is: localized_[language]_iwd#.iwd\n",
                            base, gamedir, names[i]);
                if (fs_supportedLanguagesPrinted == 0) {
                    Com_Printf("Supported languages are:\n");
                    for (k = 0; k < 0xf; k++) {
                        Com_Printf("    %s\n", LocLanguage_Name(k));
                    }
                    fs_supportedLanguagesPrinted = 1;
                }
            }
        } else {
            add = 1;
        }

        if (add != 0) {
            FS_BuildOSPath_Fatal(base, gamedir, names[i], pathbuf);
            pack = FS_LoadPackFile(pathbuf, names[i]);
            if (pack != NULL) {
                strcpy(pack->iwdGamename, gamedir);
                node = (struct searchpath_s *)Com_MallocZeroed(0x14);
                node->iwd = pack;
                node->bLocalized = localized;
                node->language = language;
                FS_InsertSearchPath(node);
            }
        }
    }
    Sys_FreeFileList(list);
}

// 0x080a27ae  size=610  FUN_080a27ae  status=translated  naming=inferred
// naming evidence: rodata "game folder ... added as both localized & non-localized", "game golder ... re-added"; adds a directory search path; Q3 FS_AddGameDirectory shape.
// note: the binary's "golder" spelling in the warning is preserved.
void FS_AddGameDirectory(const char *path, const char *dir, int localized, int language)
{
    char name[68];
    char ospath[260];
    struct searchpath_s *node;
    struct directory_s *newDir;
    const char *langName;
    const char *msg;
    size_t len;

    if (localized == 0) {
        Q_strncpyz(name, dir, 0x40);
    } else {
        langName = LocLanguage_Name(language);
        Com_sprintf(name, 0x40, "%s/%s", dir, langName);
    }

    for (node = fs_searchpaths; node != NULL; node = node->next) {
        if (node->dir != NULL && Q_stricmp(node->dir->path, path) == 0 &&
            Q_stricmp(node->dir->gamedir, name) == 0) {
            break;
        }
    }

    if (node == NULL) {
        if (localized == 0) {
            Q_strncpyz(fs_gamedir, name, 0x100);
        } else {
            FS_BuildOSPath_Fatal(path, name, "", ospath);
            len = strlen(ospath);
            ospath[len - 1] = '\0';
            if (Sys_DirectoryHasContent(ospath) == 0) {
                return;
            }
        }
        node = (struct searchpath_s *)Com_MallocZeroed(0x14);
        newDir = (struct directory_s *)Com_MallocZeroed(0x200);
        node->dir = newDir;
        Q_strncpyz(newDir->path, path, 0x100);
        Q_strncpyz(newDir->gamedir, name, 0x100);
        node->bLocalized = localized;
        node->language = language;
        FS_InsertSearchPath(node);
        FS_AddIwdFiles(path, name);
        return;
    }

    if (node->bLocalized != localized) {
        msg = (node->bLocalized == 0) ? "non-localized" : "localized";
        Com_Printf("WARNING: game folder %s/%s added as both localized & non-localized. Using folder as %s\n",
                    path, name, msg);
    }
    if (node->bLocalized == 0) {
        return;
    }
    if (node->language == language) {
        return;
    }
    Com_Printf("WARNING: game golder %s/%s re-added as localized folder with different language\n", path, name);
}
