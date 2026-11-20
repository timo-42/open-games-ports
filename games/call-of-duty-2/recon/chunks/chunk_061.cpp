#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_061_types.h"

/* Chunk-local prototypes (defined later in this file). */

// 0x0813b8fe  size=11  FUN_0813b8fe  status=translated  naming=inferred
// Naming: descriptive accessor returning node+0x08, the left link of the tree node layout used by RbTree_InsertWithHint and the rotations; caller 0x0813b25c is outside this chunk.
int RbNode_Left(const struct cod2_str_map_s *m)
{
    return (int)m->node_count; /* reads dword at +0x08 of its argument (the map object in the caller) */
}

// 0x0813b90a  size=414  FUN_0813b90a  status=translated  naming=inferred
// Naming: libstdc++-style _Rb_tree::_M_insert(x, y, v) (old ABI, GCC 3.x): hint x, parent y, value v, header tree->header, comparator tree->key_compare. Result object written to out and returned.
void *RbTree_InsertWithHint(void *out, struct cod2_str_map_s *tree, struct cod2_rb_node_s *x,
                            struct cod2_rb_node_s *y, struct rb_pair_s *value)
{
    struct cod2_rb_node_s *header = tree->header;
    struct cod2_rb_node_s *z;
    char scratch = 0;
    int placed_before_y = 0;

    if (y != header && x == NULL) {
        struct cod2_string_s *key_y = LocNode_KeyOf(y);
        const struct cod2_string_s *key_v = LocKey_Passthrough(&scratch, (const struct cod2_string_s *)value);
        if (LocMap_KeyLess(&tree->key_compare, key_v, key_y) == 0) {
            z = RbTree_CreateNode(tree, value);
            *LocNode_RightAlt(y) = z;
            if (y == *LocMap_Rightmost(tree)) {
                *LocMap_Rightmost(tree) = z;
            }
            placed_before_y = 1;
        }
    }

    if (!placed_before_y) {
        z = RbTree_CreateNode(tree, value);
        *LocNode_Left(y) = z;
        if (y == header) {
            *LocMap_Root(tree) = z;
            *LocMap_Rightmost(tree) = z;
        } else if (y == *LocMap_Leftmost(tree)) {
            *LocMap_Leftmost(tree) = z;
        }
    }

    *RbNode_ParentSlot(z) = y;
    *LocNode_Left(z) = NULL;
    *LocNode_RightAlt(z) = NULL;
    RbTree_InsertFixup(z, &header->parent);
    tree->node_count += 1;
    LocIter_FromNode((struct cod2_rb_node_s **)out, z);
    return out;
}

// 0x0813baa8  size=11  FUN_0813baa8  status=translated  naming=inferred
// Naming: address of node->parent (node+0x04); used to link a new node to its parent in RbTree_InsertWithHint.
struct cod2_rb_node_s **RbNode_ParentSlot(struct cod2_rb_node_s *node)
{
    return &node->parent;
}

// 0x0813bab4  size=45  FUN_0813bab4  status=translated  naming=inferred
// Naming: begin() of the tree: copies the leftmost node into the iterator object out and returns out.
void *RbTree_Begin(void *out, struct cod2_str_map_s *tree)
{
    struct cod2_rb_node_s **leftmost = LocMap_Leftmost(tree);
    LocIter_FromNode((struct cod2_rb_node_s **)out, *leftmost);
    return out;
}

// 0x0813bae2  size=38  FUN_0813bae2  status=translated  naming=unknown
// Naming: unknown. Initializes an object through LocIter_Copy, then stores one byte from the flag pointer at offset +4. Caller is outside this chunk.
void Rb_InitWithFlag(void *self, unsigned int arg, const byte *flag)
{
    /* the binary calls the iterator copy-constructor (self, (iterator *)arg): *self = *arg */
    *(struct cod2_rb_node_s **)self = LocIter_Copy(*(struct cod2_rb_node_s **)(uintptr_t)arg);
    *((byte *)self + 4) = *flag;
}

// 0x0813bb08  size=439  FUN_0813bb08  status=translated  naming=inferred
// Naming: libstdc++ _Rb_tree_insert_and_rebalance (old ABI). Color 0 = red, 1 = black. root is the header's root slot (&header->parent). Rotations: Rb_RotateLeft / Rb_RotateRight.
void RbTree_InsertFixup(struct cod2_rb_node_s *x, struct cod2_rb_node_s **root)
{
    x->color = 0;
    while (x != *root && x->parent->color == 0) {
        if (x->parent == x->parent->parent->left) {
            struct cod2_rb_node_s *uncle = x->parent->parent->right;
            if (uncle == NULL || uncle->color != 0) {
                if (x == x->parent->right) {
                    x = x->parent;
                    Rb_RotateLeft(x, root);
                }
                x->parent->color = 1;
                x->parent->parent->color = 0;
                Rb_RotateRight(x->parent->parent, root);
            } else {
                x->parent->color = 1;
                uncle->color = 1;
                x->parent->parent->color = 0;
                x = x->parent->parent;
            }
        } else {
            struct cod2_rb_node_s *uncle = x->parent->parent->left;
            if (uncle == NULL || uncle->color != 0) {
                if (x == x->parent->left) {
                    x = x->parent;
                    Rb_RotateRight(x, root);
                }
                x->parent->color = 1;
                x->parent->parent->color = 0;
                Rb_RotateLeft(x->parent->parent, root);
            } else {
                x->parent->color = 1;
                uncle->color = 1;
                x->parent->parent->color = 0;
                x = x->parent->parent;
            }
        }
    }
    (*root)->color = 1;
}

// 0x0813bcc0  size=29  FUN_0813bcc0  status=translated  naming=inferred
// Naming: tree clear. Erases the subtree under the header through LocMap_FreeNode (libstdc++ _M_erase(_M_begin()) pattern).
void RbTree_Clear(struct cod2_str_map_s *tree)
{
    LocMap_FreeNode(tree, tree->header);
}

// 0x0813bcde  size=44  FUN_0813bcde  status=translated  naming=inferred
// Naming: allocates a node through LocNode_AllocHeader and copy-constructs the pair value at node+0x10.
struct cod2_rb_node_s *RbTree_CreateNode(struct cod2_str_map_s *tree, struct rb_pair_s *value)
{
    struct cod2_rb_node_s *node = (struct cod2_rb_node_s *)LocNode_AllocHeader(tree);
    Rb_ConstructValue((struct rb_pair_s *)&node->key, value);
    return node;
}

// 0x0813bd52  size=65  FUN_0813bd52  status=translated  naming=inferred
// Naming: placement-new copy of a std::pair<string,string> (8 bytes) into dst; a null placement result is skipped.
void Rb_ConstructValue(struct rb_pair_s *dst, const struct rb_pair_s *src)
{
    struct rb_pair_s *placed = (struct rb_pair_s *)Rb_PlacementNew(8, dst);
    if (placed != NULL) {
        Rb_PairCopyConstruct(placed, src);
    }
}

// 0x0813bdce  size=8  FUN_0813bdce  status=translated  naming=inferred
// Naming: operator new(size, void *place) behaviour: returns the placement address.
void *Rb_PlacementNew(unsigned int size, void *place)
{
    (void)size;
    return place;
}

// 0x0813bdd6  size=5  FUN_0813bdd6  status=translated  naming=unknown
// Naming: unknown. Empty function (frame setup and return only).
void Rb_EmptyHook(void)
{
}

// 0x0813bddc  size=140  FUN_0813bddc  status=translated  naming=inferred
// Naming: libstdc++ _Rb_tree_rotate_left(x, root) (old ABI). Root slot is updated when x is the root.
void Rb_RotateLeft(struct cod2_rb_node_s *x, struct cod2_rb_node_s **root)
{
    struct cod2_rb_node_s *y = x->right;
    x->right = y->left;
    if (y->left != NULL) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x == *root) {
        *root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

// 0x0813be68  size=140  FUN_0813be68  status=translated  naming=inferred
// Naming: libstdc++ _Rb_tree_rotate_right(x, root) (old ABI), mirror of Rb_RotateLeft.
void Rb_RotateRight(struct cod2_rb_node_s *x, struct cod2_rb_node_s **root)
{
    struct cod2_rb_node_s *y = x->left;
    x->left = y->right;
    if (y->right != NULL) {
        y->right->parent = x;
    }
    y->parent = x->parent;
    if (x == *root) {
        *root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }
    y->right = x;
    x->parent = y;
}

// 0x0813bef4  size=51  FUN_0813bef4  status=translated  naming=inferred
// Naming: copy constructor of std::pair<std::string,std::string>: first member copied, then second; if the second copy throws, the first is destroyed and the exception rethrown (landing pad 0x0813bf27).
void Rb_PairCopyConstruct(struct rb_pair_s *dst, const struct rb_pair_s *src)
{
    cod2_string_copy_ctor(&dst->first, &src->first);
    try {
        cod2_string_copy_ctor(&dst->second, &src->second);
    } catch (...) {
        cod2_string_dtor(&dst->first);
        throw;
    }
}

// 0x0813bf4c  size=56  FUN_0813bf4c  status=translated  naming=inferred
// Naming: PunkBuster event dispatch with three arguments (sixth argument zero) into g_08a6f2c0.
void PB_DispatchEvent3(int event, int a, int b, int c)
{
    PB_Dispatch(&g_08a6f2c0, event, a, b, c, 0);
}

// 0x0813bf84  size=168  FUN_0813bf84  status=translated  naming=inferred
// Naming: joins two strings with one space and sends them as PunkBuster event 0x0f. Text longer than 0x800 bytes is dropped. Evidence: strcat(" ") and the 2068-byte buffer.
void PB_SendJoinedText(int tag, const char *first, const char *second)
{
    size_t len_first = strlen(first);
    size_t len_second = strlen(second);
    char buf[2068];
    size_t len_buf;

    if (len_second + len_first < 0x801) {
        strcpy(buf, first);
        strcat(buf, " ");
        strcat(buf, second);
        len_buf = strlen(buf);
        PB_DispatchEvent3(0x0f, tag, (int)len_buf, (int)(intptr_t)buf);
    }
}

// 0x0813c02c  size=77  FUN_0813c02c  status=translated  naming=inferred
// Naming: PunkBuster server init. Installs handlers, sends event 0x10 with an empty string, and if the sb procedure is not loaded applies cvar "0" through SV_ApplyPunkbusterCvar (placeholder SV_ApplyPunkbusterCvar). String literals "" and "0" from rodata.
void PB_ServerInit(void)
{
    PB_InitHandlers(&g_08a6f2c0);
    PB_DispatchEvent3(0x10, -1, 0, (int)"");
    if (g_08a6f2c0.proc_sb == NULL) {
        SV_ApplyPunkbusterCvar("0");
    }
}

// 0x0813c07a  size=28  FUN_0813c07a  status=translated  naming=inferred
// Naming: queries the server procedure with event 0 (PB_QueryServer).
void PB_QueryServerEvent0(void)
{
    PB_QueryServer(&g_08a6f2c0, 0);
}

// 0x0813c096  size=28  FUN_0813c096  status=translated  naming=inferred
// Naming: queries the server procedure with event -1 (PB_QueryServer).
void PB_QueryServerEventNeg1(void)
{
    PB_QueryServer(&g_08a6f2c0, -1);
}

// 0x0813c0b2  size=58  FUN_0813c0b2  status=translated  naming=unknown
// Naming: unknown. Dispatches event 0x33 with arguments (-1, second, first, 0).
void PB_SendEvent33(int first, int second)
{
    PB_Dispatch(&g_08a6f2c0, 0x33, -1, second, first, 0);
}

// 0x0813c0ec  size=47  FUN_0813c0ec  status=translated  naming=unknown
// Naming: unknown. Invokes the callback at pb+0x158 with (pb, a, b) when it is set.
void PB_OnConnectPacket(int a, int b)
{
    if (g_08a6f2c0.cb_158 != NULL) {
        g_08a6f2c0.cb_158(&g_08a6f2c0, a, b);
    }
}

// 0x0813c11c  size=67  FUN_0813c11c  status=translated  naming=unknown
// Naming: unknown. Invokes the callback at pb+0x15c with (pb, a, b, c); returns 0 when the callback is unset.
const char *PB_CheckClientConnect(const char *addrText, int punkbuster, char *guidOut)
{
    const char *result = 0;
    if (g_08a6f2c0.cb_15c != NULL) {
        result = (const char *)g_08a6f2c0.cb_15c(&g_08a6f2c0, (int)addrText, punkbuster, (int)guidOut);
    }
    return result;
}

// 0x0813c19c  size=60  FUN_0813c19c  status=translated  naming=unknown
// Naming: unknown. Dispatches PunkBuster event 0x75 with no arguments.
void PB_SendMapStartEventEnabled(void)
{
    PB_Dispatch(&g_08a6f2c0, 0x75, -1, 0, 0, 0);
}

// 0x0813c1d8  size=60  FUN_0813c1d8  status=translated  naming=unknown
// Naming: unknown. Dispatches PunkBuster event 0x76 with no arguments.
void PB_SendMapStartEventDisabled(void)
{
    PB_Dispatch(&g_08a6f2c0, 0x76, -1, 0, 0, 0);
}

// 0x0813c214  size=120  FUN_0813c214  status=translated  naming=inferred
// Naming: console output hook. Forwards the text to the cb_160 callback, then appends it to the console capture buffer when the result fits the limit (con_cap_len).
void PB_ConsoleCapture(char *text, int n)
{
    if (g_08a6f2c0.cb_160 != NULL) {
        g_08a6f2c0.cb_160(&g_08a6f2c0, text, n);
    }
    if (g_08a6f2c0.con_cap_buf != NULL) {
        size_t len_buf = strlen(g_08a6f2c0.con_cap_buf);
        size_t len_text = strlen(text);
        if ((int)(len_text + len_buf) < g_08a6f2c0.con_cap_len) {
            strcpy(g_08a6f2c0.con_cap_buf + len_buf, text);
        }
    }
}

// 0x0813c28c  size=429  FUN_0813c28c  status=translated  naming=inferred
// Naming: PunkBuster console command handler installed at pb+0x13c. Recognized commands: set_sv_punkbuster, ConCapBufLen, ConCapBuf, Cmd_Exec, DropClient, Cvar_Set, Dvar_Set. Literal strings from rodata. Quirk kept: ConCapBufLen stores the argument pointer bits into the int limit.
int PB_HandleCommand(char *cmd, char *args)
{
    char *token_end;
    char *rest;

    if (strcasecmp(cmd, "set_sv_punkbuster") == 0) {
        SV_ApplyPunkbusterCvar(args);
    } else if (strcasecmp(cmd, "ConCapBufLen") == 0) {
        g_08a6f2c0.con_cap_len = (int)(intptr_t)args;
    } else if (strcasecmp(cmd, "ConCapBuf") == 0) {
        g_08a6f2c0.con_cap_buf = args;
    } else if (strcasecmp(cmd, "Cmd_Exec") == 0) {
        int is_pb_cmd = strncasecmp(args, "pb_", 3);
        Cmd_ExecuteString(args);
        if (is_pb_cmd == 0) {
            PB_QueryServerEventNeg1();
        }
    } else {
        /* Skip leading blanks, scan the first token, then skip the blanks after it.
         * token_end keeps the position where the first token stopped. */
        rest = args;
        while (*rest == ' ') {
            rest++;
        }
        token_end = rest;
        while (*rest != '\0' && *rest != ' ') {
            rest++;
            token_end = rest;
        }
        while (*rest == ' ') {
            rest++;
        }
        if (strcasecmp(cmd, "DropClient") == 0) {
            SV_DropClientByNum(atoi(args), rest);
        } else if (strcasecmp(cmd, "Cvar_Set") == 0 || strcasecmp(cmd, "Dvar_Set") == 0) {
            char saved = *token_end;
            *token_end = '\0';
            Dvar_SetIfExists(args, rest);
            *token_end = saved;
        }
    }
    return 0;
}

// 0x0813c5de  size=63  FUN_0813c5de  status=translated  naming=inferred
// Naming: GCC static-initialization guard (__static_initialization_and_destruction pattern, initialize_p == 1, priority 0xffff). Builds g_08a6f2c0 and registers PB_AtexitShutdown.
void PB_StaticInit(int initialize_p, int priority)
{
    if (priority == 0xffff && initialize_p == 1) {
        PB_Construct(&g_08a6f2c0);
        __cxa_atexit((void (*)(void *))PB_AtexitShutdown, NULL, &pb_dsoHandle);
    }
}

// 0x0813c61e  size=20  FUN_0813c61e  status=translated  naming=inferred
// Naming: atexit hook for the PunkBuster server object; runs PB_Shutdown.
void PB_AtexitShutdown(void)
{
    PB_Shutdown(&g_08a6f2c0);
}

// 0x0813c632  size=28  FUN_0813c632  status=translated  naming=inferred
// Naming: GCC _GLOBAL__sub_I entry: calls PB_StaticInit(1, 0xffff).
void PB_GlobalCtorEntry(void)
{
    PB_StaticInit(1, 0xffff);
}

// 0x0813c64e  size=293  FUN_0813c64e  status=translated  naming=unknown
// Naming: unknown. Integer to string in radix 2..36 (lowercase digits). Signed output only for radix 10. Any other input yields "0". Returns out, or NULL when out is NULL.
char *Int_ToStringRadix(unsigned int value, char *out, unsigned int radix)
{
    char digits[35];
    int pos;
    unsigned int n;

    if (out == NULL) {
        return NULL;
    }
    strcpy(out, "0");
    if (value != 0 && (int)radix > 1 && (int)radix <= 0x24) {
        n = value;
        pos = 34;
        digits[34] = '\0';
        if ((int)value < 0 && radix == 10) {
            n = (unsigned int)(-(int)value);
        }
        while (n != 0) {
            unsigned int digit = n % radix;
            pos--;
            digits[pos] = (char)(digit < 10 ? digit + '0' : digit + 'W');
            n = n / radix;
        }
        if ((int)value < 0 && radix == 10) {
            pos--;
            digits[pos] = '-';
        }
        strcpy(out, &digits[pos]);
    }
    return out;
}

// 0x0813c774  size=84  FUN_0813c774  status=translated  naming=inferred
// Naming: PunkBuster handler table setup. Clears the handler slots, then installs PB_HandleCommand at pb+0x13c and the cvar query and three callbacks.
void PB_InitHandlers(struct punkbuster_s *pb)
{
    PB_ClearHandlers(pb);
    pb->cmd_handler = PB_HandleCommand;
    pb->cvar_query = (pb_cvar_query_fn_t)PB_ServerQuery;
    pb->cb_144 = (pb_void_fn_t)PB_PrintCallback;
    pb->cb_148 = (pb_void_fn_t)PB_SendToClientsCallback;
    pb->cb_154 = (pb_void_fn_t)PB_SendPacketCallback;
}

// 0x0813c7c8  size=191  FUN_0813c7c8  status=translated  naming=inferred
// Naming: PunkBuster event dispatch. Lazily loads pbsv.so when the procedure is not loaded (or unloads it when a reload is pending), then calls proc_sb with the event and four arguments. Event 0x71 and 0x72 suppress load errors.
int PB_Dispatch(struct punkbuster_s *pb, int event, int a, int b, int c, int d)
{
    char *load_error;

    if (pb->cmd_handler == NULL) {
        return 0;
    }
    if (pb->dll_pending != 0 || pb->dll_sv == NULL) {
        if (pb->dll_sv != NULL) {
            PB_UnloadServerDll(pb);
            return 0;
        }
        load_error = PB_LoadServerDll(pb);
        if (load_error != NULL) {
            if (event == 0x71 || event == 0x72) {
                return 0;
            }
            return (int)(intptr_t)load_error;
        }
    }
    return pb->proc_sb(pb, event, a, b, c, d);
}

// 0x0813c888  size=175  FUN_0813c888  status=translated  naming=inferred
// Naming: PunkBuster server query through proc_sa (the "sa" symbol). Starts a pending load with event 0x10 when the DLL is absent.
int PB_QueryServer(struct punkbuster_s *pb, int event)
{
    if (pb->cmd_handler == NULL) {
        return 0;
    }
    if (pb->dll_sv == NULL) {
        if (pb->dll_pending != 0) {
            PB_Dispatch(pb, 0x10, -1, 0, (int)"", 0);
        }
        return 0;
    }
    if (pb->dll_pending != 0) {
        PB_UnloadServerDll(pb);
        return 0;
    }
    return pb->proc_sa(pb, event);
}

// 0x0813c938  size=161  FUN_0813c938  status=translated  naming=observed
// Naming: observed from the rodata string "PunkBuster Server" copied into the object at +0x14 (the string is written here, not elsewhere).
void PB_Construct(struct punkbuster_s *pb)
{
    pb->magic = 0x357afe24u;
    strcpy(pb->name, "PunkBuster Server");
    pb->dll_sv = NULL;
    pb->dll_pending = 1;
    PB_ClearHandlers(pb);
    pb->state_04 = 0;
    pb->proc_sb = NULL;
    pb->proc_sa = NULL;
    pb->cb_154 = NULL;
    pb->cb_158 = NULL;
    pb->cb_15c = NULL;
    pb->cb_160 = NULL;
}

// 0x0813c9da  size=57  FUN_0813c9da  status=translated  naming=inferred
// Naming: clears the command and cvar handler slots (pb+0x13c..0x148).
void PB_ClearHandlers(struct punkbuster_s *pb)
{
    pb->cmd_handler = NULL;
    pb->cvar_query = NULL;
    pb->cb_144 = NULL;
    pb->cb_148 = NULL;
}

// 0x0813ca14  size=116  FUN_0813ca14  status=translated  naming=inferred
// Naming: unloads the server procedure table and closes the pbsv.so handle.
void PB_UnloadServerDll(struct punkbuster_s *pb)
{
    pb->state_04 = 0;
    pb->proc_sa = NULL;
    pb->proc_sb = NULL;
    pb->cb_158 = NULL;
    pb->cb_15c = NULL;
    pb->cb_160 = NULL;
    if (pb->dll_sv != NULL) {
        dlclose(pb->dll_sv);
    }
    pb->dll_sv = NULL;
}

// 0x0813ca88  size=637  FUN_0813ca88  status=translated  naming=inferred
// Naming: loads pbsv.so from the PunkBuster directory, installing pbsvnew.so over it (old copy kept as pbsvold.so) when pbsvnew.so exists. Resolves "sa" and "sb". Returns the error string or NULL. Strings from rodata.
char *PB_LoadServerDll(struct punkbuster_s *pb)
{
    char path_src[516];
    char path_dst[512];
    const char *error = NULL;
    FILE *probe;
    void *handle;

    if (pb->dll_sv != NULL) {
        return NULL;
    }
    PB_UnloadServerDll(pb);

    /* Path arguments are built as separate statements in the binary's order
     * (destination buffer first, then source), so side effects of the first
     * PB_BuildPath call happen in the same sequence. */
    probe = fopen(PB_BuildPath(pb, path_src, "pbsvnew.so"), "rb");
    if (probe != NULL) {
        fclose(probe);
        PB_MakeExecutable(PB_BuildPath(pb, path_src, "pbsvold.so"));
        remove(PB_BuildPath(pb, path_src, "pbsvold.so"));
        {
            char *dst = PB_BuildPath(pb, path_dst, "pbsvold.so");
            char *src = PB_BuildPath(pb, path_src, "pbsv.so");
            rename(src, dst);
        }
        PB_MakeExecutable(PB_BuildPath(pb, path_src, "pbsv.so"));
        remove(PB_BuildPath(pb, path_src, "pbsv.so"));
        {
            char *dst = PB_BuildPath(pb, path_dst, "pbsv.so");
            char *src = PB_BuildPath(pb, path_src, "pbsvnew.so");
            rename(src, dst);
        }
    }

    handle = dlopen(PB_BuildPath(pb, path_src, "pbsv.so"), 1);
    pb->dll_sv = handle;
    if (handle == NULL) {
        error = "PB Error: Server DLL Load Failure";
    } else {
        pb->proc_sa = (pb_query_fn_t)dlsym(handle, "sa");
        pb->proc_sb = (pb_dispatch_fn_t)dlsym(handle, "sb");
        if (pb->proc_sa == NULL || pb->proc_sb == NULL) {
            PB_UnloadServerDll(pb);
            error = "PB Error: Server DLL Get Procedure Failure";
        } else {
            pb->dll_pending = 0;
            error = NULL;
        }
    }
    return (char *)error;
}

// 0x0813cd06  size=41  FUN_0813cd06  status=translated  naming=inferred
// Naming: PunkBuster shutdown: unloads pbsv.so, closes pbcl.so and pbag.so.
void PB_Shutdown(struct punkbuster_s *pb)
{
    PB_UnloadServerDll(pb);
    PB_CloseClDll(pb);
    PB_CloseAgDll(pb);
}

// 0x0813cd30  size=27  FUN_0813cd30  status=translated  naming=inferred
// Naming: chmod(path, 0777).
void PB_MakeExecutable(const char *path)
{
    chmod(path, 0x1ff);
}

// 0x0813cd4c  size=332  FUN_0813cd4c  status=translated  naming=inferred
// Naming: builds a path inside the PunkBuster directory (pb_dir + name). On the first call it resolves the home path, the fs_basepath source and creates the pb directory, copying pbsv.so, pbcl.so and pbag.so when missing.
char *PB_BuildPath(struct punkbuster_s *pb, char *out, char *name)
{
    if (pb->pb_dir[0] == '\0') {
        char base[0x100];

        PB_InitHomePath(pb);
        PB_GetFsBasePath(pb, base);
        if (base[strlen(base) - 1] != '/') {
            strcat(base, "/");
        }
        strcat(base, "pb/");
        if (strcasecmp(base, pb->pb_dir) != 0 && base[0] != '\0' && pb->pb_dir[0] != '\0') {
            mkdir(pb->pb_dir, 0x1ff);
            PB_CopyDllIfMissing(pb, "pbsv.so", base);
            PB_CopyDllIfMissing(pb, "pbcl.so", base);
            PB_CopyDllIfMissing(pb, "pbag.so", base);
        }
    }
    strcpy(out, pb->pb_dir);
    strcat(out, name);
    return out;
}

// 0x0813ce98  size=41  FUN_0813ce98  status=translated  naming=inferred
// Naming: closes the pbcl.so handle (pb+0x0c).
void PB_CloseClDll(struct punkbuster_s *pb)
{
    if (pb->dll_cl != NULL) {
        dlclose(pb->dll_cl);
    }
    pb->dll_cl = NULL;
}

// 0x0813cec2  size=54  FUN_0813cec2  status=translated  naming=inferred
// Naming: closes the pbag.so handle (pb+0x10) and clears pb+0x164.
void PB_CloseAgDll(struct punkbuster_s *pb)
{
    pb->field_164 = 0;
    if (pb->dll_ag != NULL) {
        dlclose(pb->dll_ag);
    }
    pb->dll_ag = NULL;
}

// 0x0813cef8  size=63  FUN_0813cef8  status=translated  naming=inferred
// Naming: queries the "fs_basepath" value through cvar_query (key 0x67) into out.
void PB_GetFsBasePath(struct punkbuster_s *pb, char *out)
{
    if (pb->cvar_query != NULL) {
        char *name = strcpy(out, "fs_basepath");
        pb->cvar_query(0x67, name);
    }
}

// 0x0813cf38  size=183  FUN_0813cf38  status=translated  naming=inferred
// Naming: copies name from src_dir into the PunkBuster directory when the destination file is missing.
void PB_CopyDllIfMissing(struct punkbuster_s *pb, const char *name, const char *src_dir)
{
    FILE *existing;
    char dst[512];
    char src[520];

    strcpy(dst, pb->pb_dir);
    strcat(dst, name);
    existing = fopen(dst, "rb");
    if (existing == NULL) {
        strcpy(src, src_dir);
        strcat(src, name);
        PB_CopyFile(src, dst, 0);
    } else {
        fclose(existing);
    }
}

// 0x0813cff0  size=189  FUN_0813cff0  status=translated  naming=inferred
// Naming: resolves the home directory through cvar_query("fs_homepath"), falls back to getcwd, adds a trailing slash and the "pb/" subdirectory.
void PB_InitHomePath(struct punkbuster_s *pb)
{
    if (pb->cvar_query != NULL) {
        char *name = strcpy(pb->pb_dir, "fs_homepath");
        pb->cvar_query(0x67, name);
        if (pb->pb_dir[0] == '\0') {
            getcwd(pb->pb_dir, 0xfb);
        }
        if (pb->pb_dir[0] != '\0') {
            size_t len = strlen(pb->pb_dir);
            if (pb->pb_dir[len - 1] != '/') {
                strcat(pb->pb_dir, "/");
            }
        }
        strcat(pb->pb_dir, "pb/");
    }
}

// 0x0813d0ae  size=319  FUN_0813d0ae  status=translated  naming=inferred
// Naming: copies the whole file src to dst through one heap buffer. max_size 0 means no limit, otherwise the file must be smaller than max_size. Returns 1 when all bytes were written. Quirk kept: the buffer comes from new[] and is released with operator delete.
int PB_CopyFile(const char *src, const char *dst, int max_size)
{
    int ok = 0;
    FILE *in = fopen(src, "rb");

    if (in != NULL) {
        FILE *out = fopen(dst, "wb");
        if (out != NULL) {
            unsigned int size;
            void *buf;

            fseek(in, 0, 2);
            size = (unsigned int)ftell(in);
            if ((int)size > 0 && (max_size == 0 || (int)size < max_size)) {
                buf = cod2_operator_new_array(size);
                if (buf != NULL) {
                    size_t got;
                    fseek(in, 0, 0);
                    got = fread(buf, 1, size, in);
                    got = fwrite(buf, 1, got, out);
                    cod2_operator_delete(buf);
                    if (got == size) {
                        ok = 1;
                    }
                }
            }
            fclose(out);
        }
        fclose(in);
    }
    return ok;
}

// 0x0813d1f0  size=3371  FUN_0813d1f0  status=translated  naming=inferred
// Naming: MD5 block transform (RFC 1321 constants and round functions verified against the disassembly: T[1..64], shifts 7/12/17/22, 5/9/14/20, 4/11/16/23, 6/10/15/21). The 64 unrolled steps are written as a loop with the same constants, shifts and word order; no other behaviour is added.
void MD5Transform(unsigned int *state, const unsigned int *x)
{
    static const unsigned int k[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
    };
    static const unsigned char s[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };
    unsigned int a = state[0];
    unsigned int b = state[1];
    unsigned int c = state[2];
    unsigned int d = state[3];
    unsigned int i;

    for (i = 0; i < 64; i++) {
        unsigned int f;
        unsigned int g;
        unsigned int temp;
        unsigned int v;
        unsigned int shift = s[i];

        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (b & d) | (c & ~d);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }
        temp = d;
        d = c;
        c = b;
        v = a + f + k[i] + x[g];
        b = b + ((v << shift) | (v >> (32 - shift)));
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

// 0x0813df1c  size=126  FUN_0813df1c  status=translated  naming=inferred
// Naming: MD5 init with a seed: count = 0 and state = the RFC 1321 IV perturbed by seed (A += 11*seed, B += 71*seed, C += 37*seed, D += 97*seed). seed 0 gives the standard IV.
void MD5_InitSeeded(struct md5_ctx_s *ctx, unsigned int seed)
{
    ctx->count[1] = 0;
    ctx->count[0] = 0;
    ctx->state[0] = seed * 0xbu + 0x67452301u;
    ctx->state[1] = seed * 0x47u + 0xefcdab89u;
    ctx->state[2] = seed * 0x25u + 0x98badcfeu;
    ctx->state[3] = seed * 0x61u + 0x10325476u;
}

// 0x0813df9a  size=281  FUN_0813df9a  status=translated  naming=inferred
// Naming: MD5 update (bit count split into count[0] and count[1]; 64-byte buffer at ctx+0x18; full blocks are transformed in little-endian word order).
void MD5_Update(struct md5_ctx_s *ctx, const byte *data, unsigned int len)
{
    unsigned int index = (ctx->count[0] >> 3) & 0x3f;
    unsigned int block[16];
    unsigned int i;

    if (ctx->count[0] + len * 8 < ctx->count[0]) {
        ctx->count[1] = ctx->count[1] + 1;
    }
    ctx->count[0] = len * 8 + ctx->count[0];
    ctx->count[1] = (len >> 29) + ctx->count[1];

    while (len != 0) {
        len--;
        ctx->buffer[index] = *data;
        index++;
        data++;
        if (index == 0x40) {
            for (i = 0; i < 16; i++) {
                block[i] = (unsigned int)ctx->buffer[i * 4] |
                           ((unsigned int)ctx->buffer[i * 4 + 1] << 8) |
                           ((unsigned int)ctx->buffer[i * 4 + 2] << 16) |
                           ((unsigned int)ctx->buffer[i * 4 + 3] << 24);
            }
            MD5Transform(ctx->state, block);
            index = 0;
        }
    }
}

// 0x0813e0b4  size=366  FUN_0813e0b4  status=translated  naming=inferred
// Naming: MD5 final. Pads to 56 mod 64 with md5_padding, appends the saved bit count as words 14 and 15, transforms, and writes the digest (little-endian) to ctx+0x58. Word 14/15 follow the 14-word block in the binary's stack frame (saved count pair).
void MD5_Final(struct md5_ctx_s *ctx)
{
    unsigned int saved_lo = ctx->count[0];
    unsigned int saved_hi = ctx->count[1];
    unsigned int index = (ctx->count[0] >> 3) & 0x3f;
    unsigned int pad_len = (index < 0x38) ? (0x38 - index) : (0x78 - index);
    unsigned int block[16];
    unsigned int i;

    MD5_Update(ctx, md5_padding, pad_len);
    for (i = 0; i < 14; i++) {
        block[i] = (unsigned int)ctx->buffer[i * 4] |
                   ((unsigned int)ctx->buffer[i * 4 + 1] << 8) |
                   ((unsigned int)ctx->buffer[i * 4 + 2] << 16) |
                   ((unsigned int)ctx->buffer[i * 4 + 3] << 24);
    }
    block[14] = saved_lo;
    block[15] = saved_hi;
    MD5Transform(ctx->state, block);
    for (i = 0; i < 4; i++) {
        ctx->digest[i * 4] = (byte)(ctx->state[i]);
        ctx->digest[i * 4 + 1] = (byte)(ctx->state[i] >> 8);
        ctx->digest[i * 4 + 2] = (byte)(ctx->state[i] >> 16);
        ctx->digest[i * 4 + 3] = (byte)(ctx->state[i] >> 24);
    }
}

// 0x0813e230  size=90  FUN_0813e230  status=translated  naming=unknown
// Naming: unknown. Crt glue that calls the .init section function (placeholder _init).
void libc_csu_init(void)
{
    _init();
}

// 0x0813e290  size=40  FUN_0813e290  status=translated  naming=inferred
// Naming: crt fini glue (libc __libc_csu_fini shape): walks the .ctors-based list downward from index -1 (the loop body does not execute on the binary's path) and then calls Crt_Fini.
void Crt_RunFini(void)
{
    int i = 0;
    while ((i = i - 1) != -1) {
        g_0817d00c[i]();
    }
    Crt_Fini(); /* 0x0813e310 (.fini) is called after the (here empty) .fini_array walk */
}

// 0x0813e2e0  size=48  FUN_0813e2e0  status=partial  naming=inferred
// Naming: crt ctor runner (__do_global_ctors_aux pattern): starts at the end-of-list slot (g_0817d00c[4]) and calls entries downward until the -1 sentinel. Return value is an uninitialized local in the binary; this translation returns 0. Open question for the merge lane.
uint32 Crt_RunCtors(void)
{
    void (**slot)(void) = &g_0817d00c[4];
    void (*fn)(void) = g_0817d00c[4];

    while ((uintptr_t)fn != 0xffffffffu) {
        slot--;
        fn();
        fn = *slot;
    }
    return 0;
}

// 0x0813e310  size=26  FUN_0813e310  status=translated  naming=inferred
// Naming: ELF .fini section wrapper (_DT_FINI). Calls the placeholder __do_global_dtors_aux (__do_global_dtors_aux pattern).
void Crt_Fini(void)
{
#ifdef COD2_RECON_ORIGINAL_CRT
    __do_global_dtors_aux();
#endif /* the linked build uses the system C runtime's local __do_global_dtors_aux (see chunk_001) */
}

/* Ghidra EXTERNAL-space entries (0x08a70000..0x08a700f4, 4-byte step). These are ELF import
 * placeholders outside the loaded image: no code or bytes exist at these addresses in
 * cod2_lnxded, so there is no body to translate. Names are the imported libc/libstdc++ symbols.
 * The merge lane should map them to the import table, not to chunk code. */
// 0x08a70000  size=1  FUN_08a70000  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import usleep (no body in the image; nothing to translate).
// 0x08a70004  size=1  FUN_08a70004  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import mkdir (no body in the image; nothing to translate).
// 0x08a70008  size=1  FUN_08a70008  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import cos (no body in the image; nothing to translate).
// 0x08a7000c  size=1  FUN_08a7000c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import ntohl (no body in the image; nothing to translate).
// 0x08a70010  size=1  FUN_08a70010  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::operator= (no body in the image; nothing to translate).
// 0x08a70014  size=1  FUN_08a70014  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import rename (no body in the image; nothing to translate).
// 0x08a70018  size=1  FUN_08a70018  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::compare (no body in the image; nothing to translate).
// 0x08a7001c  size=1  FUN_08a7001c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import execl (no body in the image; nothing to translate).
// 0x08a70020  size=1  FUN_08a70020  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import strncat (no body in the image; nothing to translate).
// 0x08a70024  size=1  FUN_08a70024  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import vsprintf (no body in the image; nothing to translate).
// 0x08a70028  size=1  FUN_08a70028  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import strchr (no body in the image; nothing to translate).
// 0x08a7002c  size=1  FUN_08a7002c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::allocator<char>::allocator (no body in the image; nothing to translate).
// 0x08a70030  size=1  FUN_08a70030  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import atan (no body in the image; nothing to translate).
// 0x08a70034  size=1  FUN_08a70034  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import __cxa_pure_virtual (no body in the image; nothing to translate).
// 0x08a70038  size=1  FUN_08a70038  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import pthread_mutexattr_settype (no body in the image; nothing to translate).
// 0x08a7003c  size=1  FUN_08a7003c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import write (no body in the image; nothing to translate).
// 0x08a70040  size=1  FUN_08a70040  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import localtime (no body in the image; nothing to translate).
// 0x08a70044  size=1  FUN_08a70044  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import recvfrom (no body in the image; nothing to translate).
// 0x08a70048  size=1  FUN_08a70048  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import fileno (no body in the image; nothing to translate).
// 0x08a7004c  size=1  FUN_08a7004c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import strcmp (no body in the image; nothing to translate).
// 0x08a70050  size=1  FUN_08a70050  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import isspace (no body in the image; nothing to translate).
// 0x08a70054  size=1  FUN_08a70054  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import close (no body in the image; nothing to translate).
// 0x08a70058  size=1  FUN_08a70058  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import asin (no body in the image; nothing to translate).
// 0x08a7005c  size=1  FUN_08a7005c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import dlsym (no body in the image; nothing to translate).
// 0x08a70060  size=1  FUN_08a70060  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import fprintf (no body in the image; nothing to translate).
// 0x08a70064  size=1  FUN_08a70064  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import fork (no body in the image; nothing to translate).
// 0x08a70068  size=1  FUN_08a70068  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import getenv (no body in the image; nothing to translate).
// 0x08a7006c  size=1  FUN_08a7006c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import signal (no body in the image; nothing to translate).
// 0x08a70070  size=1  FUN_08a70070  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::string (no body in the image; nothing to translate).
// 0x08a70074  size=1  FUN_08a70074  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import fflush (no body in the image; nothing to translate).
// 0x08a70078  size=1  FUN_08a70078  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import seteuid (no body in the image; nothing to translate).
// 0x08a7007c  size=1  FUN_08a7007c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import unlink (no body in the image; nothing to translate).
// 0x08a70080  size=1  FUN_08a70080  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import select (no body in the image; nothing to translate).
// 0x08a70084  size=1  FUN_08a70084  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import vsnprintf (no body in the image; nothing to translate).
// 0x08a70088  size=1  FUN_08a70088  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import strerror (no body in the image; nothing to translate).
// 0x08a7008c  size=1  FUN_08a7008c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import tcsetattr (no body in the image; nothing to translate).
// 0x08a70090  size=1  FUN_08a70090  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import isalnum (no body in the image; nothing to translate).
// 0x08a70094  size=1  FUN_08a70094  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import __cxa_atexit (no body in the image; nothing to translate).
// 0x08a70098  size=1  FUN_08a70098  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import getpwuid (no body in the image; nothing to translate).
// 0x08a7009c  size=1  FUN_08a7009c  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import operator.delete (no body in the image; nothing to translate).
// 0x08a700a0  size=1  FUN_08a700a0  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import __errno_location (no body in the image; nothing to translate).
// 0x08a700a4  size=1  FUN_08a700a4  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import ftell (no body in the image; nothing to translate).
// 0x08a700a8  size=1  FUN_08a700a8  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::c_str (no body in the image; nothing to translate).
// 0x08a700ac  size=1  FUN_08a700ac  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import system (no body in the image; nothing to translate).
// 0x08a700b0  size=1  FUN_08a700b0  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import chmod (no body in the image; nothing to translate).
// 0x08a700b4  size=1  FUN_08a700b4  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import tolower (no body in the image; nothing to translate).
// 0x08a700b8  size=1  FUN_08a700b8  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import access (no body in the image; nothing to translate).
// 0x08a700bc  size=1  FUN_08a700bc  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::operator= (no body in the image; nothing to translate).
// 0x08a700c0  size=1  FUN_08a700c0  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import malloc (no body in the image; nothing to translate).
// 0x08a700c4  size=1  FUN_08a700c4  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import pthread_mutex_init (no body in the image; nothing to translate).
// 0x08a700c8  size=1  FUN_08a700c8  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::~string (no body in the image; nothing to translate).
// 0x08a700cc  size=1  FUN_08a700cc  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import fread (no body in the image; nothing to translate).
// 0x08a700d0  size=1  FUN_08a700d0  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import memmove (no body in the image; nothing to translate).
// 0x08a700d4  size=1  FUN_08a700d4  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::operator[] (no body in the image; nothing to translate).
// 0x08a700d8  size=1  FUN_08a700d8  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import sendto (no body in the image; nothing to translate).
// 0x08a700dc  size=1  FUN_08a700dc  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import remove (no body in the image; nothing to translate).
// 0x08a700e0  size=1  FUN_08a700e0  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import __cxa_rethrow (no body in the image; nothing to translate).
// 0x08a700e4  size=1  FUN_08a700e4  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import rint (no body in the image; nothing to translate).
// 0x08a700e8  size=1  FUN_08a700e8  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import std::string::string (no body in the image; nothing to translate).
// 0x08a700ec  size=1  FUN_08a700ec  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import __xstat (no body in the image; nothing to translate).
// 0x08a700f0  size=1  FUN_08a700f0  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import setsockopt (no body in the image; nothing to translate).
// 0x08a700f4  size=1  FUN_08a700f4  status=partial  naming=observed
// Naming: observed Ghidra EXTERNAL import fseek (no body in the image; nothing to translate).
