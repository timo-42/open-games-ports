#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_058_types.h"

/* Chunk 058: config-tree container (0x08132064..0x081333d8) and FX effect list manager
 * (0x081333d8..0x08135d72). All names are descriptive; only FX_GetBoneOrientation is
 * string-observed. */

// 0x08132064  size=62  FUN_08132064  status=translated  naming=inferred
// Frees every node of the value list hanging off a ConfigAttr (+0x10); same shape as ConfigValue list walkers.
void ConfigAttr_FreeValues(ConfigAttr *attr)
{
    while (attr->firstValue != 0) {
        ConfigValue *next = ConfigNode_GetNextAlias(attr->firstValue);
        ConfigLink_Free(attr->firstValue);
        attr->firstValue = next;
    }
}

// 0x081320a2  size=115  FUN_081320a2  status=translated  naming=inferred
// Deep-copies a ConfigAttr (name + all values), duplicating strings into the pool when one is given.
ConfigAttr *ConfigAttr_Copy(ConfigAttr *src, Fx_StringPage **poolRef)
{
    const char *name;
    ConfigAttr *copy;
    ConfigValue *v;

    if (poolRef == 0)
        name = src->name;
    else
        name = Fx_StringPage_Append(*poolRef, src->name, 1, poolRef);
    copy = (ConfigAttr *)ConfigLink_Alloc(0x14);
    ParseNode_ConstructWithText2(copy, name, 0);
    for (v = src->firstValue; v != 0; v = ConfigNode_GetNextAlias(v)) {
        if (poolRef == 0) {
            name = ConfigBlock_GetName(v);
        } else {
            const char *text = ConfigBlock_GetName(v);
            name = Fx_StringPage_Append(*poolRef, text, 1, poolRef);
        }
        ConfigAttr_AddValue(copy, name, 0);
    }
    return copy;
}

// 0x081321c6  size=54  FUN_081321c6  status=translated  naming=inferred
// True when the attribute has more than one value (first value has a successor).
char ConfigAttr_HasMultipleValues(ConfigAttr *attr)
{
    if (attr->firstValue == 0 || ConfigNode_GetNextAlias(attr->firstValue) == 0)
        return 0;
    return 1;
}

// 0x081321fc  size=46  FUN_081321fc  status=translated  naming=inferred
// Returns the text of the first value or 0.
char *ConfigAttr_GetFirstValue(ConfigAttr *attr)
{
    char *result;
    if (attr->firstValue == 0)
        result = 0;
    else
        result = ConfigBlock_GetName(attr->firstValue);
    return result;
}

// 0x0813222a  size=224  FUN_0813222a  status=translated  naming=inferred
// Appends a value to the attribute's singly linked list; the head caches the tail in its +8 slot.
void ConfigAttr_AddValue(ConfigAttr *attr, const char *text, Fx_StringPage **poolRef)
{
    ConfigValue *node;

    if (poolRef != 0)
        text = Fx_StringPage_Append(*poolRef, text, 1, poolRef);
    if (attr->firstValue == 0) {
        node = (ConfigValue *)ConfigLink_Alloc(0x10);
        ParseNode_Init2(node, text);
        attr->firstValue = node;
        ConfigLink_SetSortNext((ConfigLink *)attr->firstValue, (ConfigLink *)attr->firstValue);
    } else {
        ConfigLink *tail;
        ConfigValue *newTail;
        node = (ConfigValue *)ConfigLink_Alloc(0x10);
        ParseNode_Init2(node, text);
        tail = ConfigLink_GetSortNext((ConfigLink *)attr->firstValue);
        ConfigLink_SetNext(tail, (ConfigLink *)node);
        tail = ConfigLink_GetSortNext((ConfigLink *)attr->firstValue);
        newTail = ConfigNode_GetNextAlias((ConfigValue *)tail);
        ConfigLink_SetSortNext((ConfigLink *)attr->firstValue, (ConfigLink *)newTail);
    }
}

// 0x0813230a  size=160  FUN_0813230a  status=translated  naming=inferred
// Reads tokens until "]" (case-insensitive), adding each as a value; returns 0 on end of stream.
char ConfigAttr_ParseList(ConfigAttr *attr, TokenStream *ts, Fx_StringPage **poolRef)
{
    while (true) {
        char *tok = Fx_ParseToken(ts, 1, 1);
        if (*tok == '\0')
            return 0;
        if (strcasecmp(tok, "]") == 0)
            break;
        const char *dup = Fx_StringPage_Append(*poolRef, tok, 1, poolRef);
        ConfigAttr_AddValue(attr, dup, 0);
    }
    return 1;
}

// 0x081323aa  size=660  FUN_081323aa  status=translated  naming=inferred
// Writes one attribute as text: single value on one line, several values as a "[ ... ]" list.
// Quirk: the value writer is invoked with the first value node as 'self' for every item.
char ConfigAttr_Write(ConfigAttr *attr, Fx_StringPage **out, int depth)
{
    int i;
    ConfigValue *v;

    if (attr->firstValue != 0) {
        for (i = 0; i < depth; i++)
            Fx_StringPage_Append(*out, "\t", 0, out);
        Fx_StringPage_AppendQuoted(attr, out, attr->name);
        if (ConfigNode_GetNextAlias(attr->firstValue) == 0) {
            Fx_StringPage_Append(*out, "\t\t", 0, out);
            char *text = ConfigBlock_GetName(attr->firstValue);
            Fx_StringPage_AppendQuoted(attr->firstValue, out, text);
            Fx_StringPage_Append(*out, "\r\n", 0, out);
        } else {
            Fx_StringPage_Append(*out, "\r\n", 0, out);
            for (i = 0; i < depth; i++)
                Fx_StringPage_Append(*out, "\t", 0, out);
            Fx_StringPage_Append(*out, "[\r\n", 0, out);
            for (v = attr->firstValue; v != 0; v = ConfigNode_GetNextAlias(v)) {
                for (i = 0; i < depth + 1; i++)
                    Fx_StringPage_Append(*out, "\t", 0, out);
                char *text = ConfigBlock_GetName(v);
                Fx_StringPage_AppendQuoted(attr->firstValue, out, text);
                Fx_StringPage_Append(*out, "\r\n", 0, out);
            }
            for (i = 0; i < depth; i++)
                Fx_StringPage_Append(*out, "\t", 0, out);
            Fx_StringPage_Append(*out, "]\r\n", 0, out);
        }
    }
    return 1;
}

// 0x0813263e  size=102  FUN_0813263e  status=translated  naming=inferred
// ParseNode constructor (variant 1; identical body to 0x081326a4).
void ConfigBlock_Init2(ParseNode *b, const char *name, int expectUnclosed)
{
    ParseNode_Init((ConfigLink *)b, name);
    b->firstAttr = 0;
    b->sortedAttrs = 0;
    b->lastAttr = 0;
    b->firstChild = 0;
    b->sortedChildren = 0;
    b->lastChild = 0;
    b->expectUnclosed = expectUnclosed;
    b->flag = 0;
}

// 0x081326a4  size=102  FUN_081326a4  status=translated  naming=inferred
// ParseNode constructor (variant 2).
void ConfigBlock_Init(ParseNode *b, const char *name, int expectUnclosed)
{
    ParseNode_Init((ConfigLink *)b, name);
    b->firstAttr = 0;
    b->sortedAttrs = 0;
    b->lastAttr = 0;
    b->firstChild = 0;
    b->sortedChildren = 0;
    b->lastChild = 0;
    b->expectUnclosed = expectUnclosed;
    b->flag = 0;
}

// 0x0813270a  size=19  FUN_0813270a  status=translated  naming=inferred
// ParseNode destructor variant (calls ConfigBlock_Clear).
void ConfigBlock_Destroy1(ParseNode *b)
{
    ConfigBlock_Clear(b);
}

// 0x0813271e  size=19  FUN_0813271e  status=translated  naming=inferred
// ParseNode destructor variant (calls ConfigBlock_Clear).
void ConfigBlock_Destroy2(ParseNode *b)
{
    ConfigBlock_Clear(b);
}

// 0x08132732  size=52  FUN_08132732  status=translated  naming=inferred
// Counts child blocks with a do/while: an empty list dereferences null (original behaviour).
int ConfigBlock_CountChildren(ParseNode *b)
{
    int count = 0;
    ParseNode *c = b->firstChild;
    do {
        count++;
        c = ConfigNode_GetNext(c);
    } while (c != 0);
    return count;
}

// 0x08132766  size=52  FUN_08132766  status=translated  naming=inferred
// Counts attributes with a do/while: an empty list dereferences null (original behaviour).
int ConfigBlock_CountAttrs(ParseNode *b)
{
    int count = 0;
    ConfigAttr *a = b->firstAttr;
    do {
        count++;
        a = ConfigNode_GetNextAlias2(a);
    } while (a != 0);
    return count;
}

// 0x0813279a  size=256  FUN_0813279a  status=translated  naming=inferred
// Frees all attributes and child blocks and resets the block's fields.
void ConfigBlock_Clear(ParseNode *b)
{
    while (b->firstAttr != 0) {
        b->lastAttr = (ConfigAttr *)ConfigNode_GetNextAlias2(b->firstAttr);
        ConfigAttr *a = b->firstAttr;
        if (a != 0) {
            ConfigAttr_FreeValues(a);
            ConfigLink_Free(a);
        }
        b->firstAttr = b->lastAttr;
    }
    while (b->firstChild != 0) {
        b->lastChild = ConfigNode_GetNext(b->firstChild);
        ParseNode *c = b->firstChild;
        if (c != 0) {
            ConfigBlock_Destroy2(c);
            ConfigLink_Free(c);
        }
        b->firstChild = b->lastChild;
    }
    b->lastAttr = 0;
    b->sortedAttrs = 0;
    b->firstAttr = 0;
    b->lastChild = 0;
    b->sortedChildren = 0;
    b->firstChild = 0;
    b->expectUnclosed = 0;
    b->flag = 0;
}

// 0x0813289a  size=263  FUN_0813289a  status=translated  naming=inferred
// Deep-copies a block with its children and attributes. The recursive call in the original passes
// a stale third argument which the callee ignores.
ParseNode *ConfigBlock_Copy(ParseNode *src, Fx_StringPage **poolRef)
{
    const char *name;
    ParseNode *copy;
    ParseNode *child;
    ConfigAttr *attr;

    if (poolRef == 0)
        name = src->name;
    else
        name = Fx_StringPage_Append(*poolRef, src->name, 1, poolRef);
    copy = (ParseNode *)ConfigLink_Alloc(0x30);
    ConfigBlock_Init(copy, name, 0);
    for (child = src->firstChild; child != 0; child = ConfigNode_GetNext(child)) {
        ParseNode *c = ConfigBlock_Copy(child, poolRef);
        ConfigBlock_AddChild(copy, c);
    }
    for (attr = src->firstAttr; attr != 0; attr = ConfigNode_GetNextAlias2(attr)) {
        ConfigAttr *a = ConfigAttr_Copy(attr, poolRef);
        ConfigBlock_AddAttr(copy, a);
    }
    return copy;
}

// 0x081329a2  size=248  FUN_081329a2  status=translated  naming=inferred
// Appends a node to an insertion-ordered list (first/last) and inserts it into the
// case-insensitive sorted list (head), for both attributes and child blocks.
void ConfigBlock_InsertNode(void *unused, ConfigLink *node, ConfigLink **first, ConfigLink **sortedHead,
                            ConfigLink **last)
{
    if (*first == 0) {
        *sortedHead = node;
        *first = node;
    } else {
        ConfigLink *cur;
        ConfigLink *prev;
        ConfigLink_SetNext(*last, node);
        cur = *sortedHead;
        prev = 0;
        for (; cur != 0; cur = ConfigLink_GetSortNext(cur)) {
            const char *curName = ConfigBlock_GetName(cur);
            const char *newName = ConfigBlock_GetName(node);
            if (strcasecmp(newName, curName) < 0)
                break;
            prev = cur;
        }
        if (cur != 0) {
            ConfigLink_SetSortPrev(cur, node);
            ConfigLink_SetSortNext(node, cur);
        }
        if (prev == 0) {
            *sortedHead = node;
        } else {
            ConfigLink_SetSortNext(prev, node);
            ConfigLink_SetSortPrev(node, prev);
        }
    }
    *last = node;
}

// 0x08132a9a  size=146  FUN_08132a9a  status=translated  naming=inferred
// Creates an attribute (optionally with one value), adds it to the block and returns it.
ConfigAttr *ConfigBlock_AddAttrNew(ParseNode *b, const char *name, const char *value, Fx_StringPage **poolRef)
{
    ConfigAttr *a;
    if (poolRef != 0) {
        name = Fx_StringPage_Append(*poolRef, name, 1, poolRef);
        if (value != 0)
            value = Fx_StringPage_Append(*poolRef, value, 1, poolRef);
    }
    a = (ConfigAttr *)ConfigLink_Alloc(0x14);
    ParseNode_ConstructWithText2(a, name, value);
    ConfigBlock_AddAttr(b, a);
    return a;
}

// 0x08132b6e  size=56  FUN_08132b6e  status=translated  naming=inferred
// Adds an attribute node to the block's attribute lists (+0x10/+0x14/+0x18).
void ConfigBlock_AddAttr(ParseNode *b, ConfigAttr *a)
{
    ConfigBlock_InsertNode(b, (ConfigLink *)a, (ConfigLink **)&b->firstAttr, (ConfigLink **)&b->sortedAttrs,
                           (ConfigLink **)&b->lastAttr);
}

// 0x08132ba6  size=118  FUN_08132ba6  status=translated  naming=inferred
// Creates a child block, adds it to the block and returns it.
ParseNode *ConfigBlock_AddChildNew(ParseNode *b, const char *name, Fx_StringPage **poolRef)
{
    ParseNode *c;
    if (poolRef != 0)
        name = Fx_StringPage_Append(*poolRef, name, 1, poolRef);
    c = (ParseNode *)ConfigLink_Alloc(0x30);
    ConfigBlock_Init(c, name, 0);
    ConfigBlock_AddChild(b, c);
    return c;
}

// 0x08132c1c  size=56  FUN_08132c1c  status=translated  naming=inferred
// Adds a child block node to the block's child lists (+0x1c/+0x20/+0x24).
void ConfigBlock_AddChild(ParseNode *b, ParseNode *c)
{
    ConfigBlock_InsertNode(b, (ConfigLink *)c, (ConfigLink **)&b->firstChild, (ConfigLink **)&b->sortedChildren,
                           (ConfigLink **)&b->lastChild);
}

// 0x08132c54  size=89  FUN_08132c54  status=translated  naming=inferred
// Finds a child block by case-insensitive name.
ParseNode *ConfigBlock_FindChild(ParseNode *b, const char *name)
{
    ParseNode *c = b->firstChild;
    while (true) {
        if (c == 0)
            return 0;
        const char *cn = ConfigBlock_GetName(c);
        if (strcasecmp(name, cn) == 0)
            break;
        c = ConfigNode_GetNext(c);
    }
    return c;
}

// 0x08132cae  size=467  FUN_08132cae  status=translated  naming=inferred
// Recursive token parser: "name {" opens a child block, "name [" a value list, otherwise
// "name value" is an attribute; "}" ends the block. Fails when the stream ends while
// expectUnclosed is nonzero.
char ConfigBlock_Parse(ParseNode *b, TokenStream *ts, Fx_StringPage **poolRef)
{
    char token[0x400];
    char *tok;

    while (tok = Fx_ParseToken(ts, 1, 0), *tok != '\0') {
        if (strcasecmp(tok, "}") == 0)
            return 1;
        Q_strncpyz(token, tok, 0x400);
        tok = Fx_ParseToken(ts, 1, 1);
        if (strcasecmp(tok, "{") == 0) {
            ParseNode *child = ConfigBlock_AddChildNew(b, token, poolRef);
            ConfigBlock_SetFlag(child, b->flag);
            char ok = ConfigBlock_Parse(child, ts, poolRef);
            if (ok == '\0')
                return 0;
        } else if (strcasecmp(tok, "[") == 0) {
            ConfigAttr *a = ConfigBlock_AddAttrNew(b, token, 0, poolRef);
            char ok = ConfigAttr_ParseList(a, ts, poolRef);
            if (ok == '\0')
                return 0;
        } else {
            ConfigBlock_AddAttrNew(b, token, tok, poolRef);
        }
    }
    if (b->expectUnclosed == 0)
        return 1;
    return 0;
}

// 0x08132e82  size=464  FUN_08132e82  status=translated  naming=inferred
// Writes a block as text ("name\r\n{\r\n ... }\r\n"); depth<0 writes only the contents (root).
int ConfigBlock_Write(ParseNode *b, Fx_StringPage **out, int depth)
{
    ConfigAttr *attr = b->firstAttr;
    ParseNode *child = b->firstChild;
    int i;

    if (-1 < depth) {
        for (i = 0; i < depth; i++)
            Fx_StringPage_Append(*out, "\t", 0, out);
        Fx_StringPage_AppendQuoted(b, out, b->name);
        Fx_StringPage_Append(*out, "\r\n", 0, out);
        for (i = 0; i < depth; i++)
            Fx_StringPage_Append(*out, "\t", 0, out);
        Fx_StringPage_Append(*out, "{\r\n", 0, out);
    }
    for (; attr != 0; attr = ConfigNode_GetNextAlias2(attr))
        ConfigAttr_Write(attr, out, depth + 1);
    for (; child != 0; child = ConfigNode_GetNext(child))
        ConfigBlock_Write(child, out, depth + 1);
    if (-1 < depth) {
        for (i = 0; i < depth; i++)
            Fx_StringPage_Append(*out, "\t", 0, out);
        Fx_StringPage_Append(*out, "}\r\n", 0, out);
    }
    return 1;
}

// 0x08133052  size=91  FUN_08133052  status=translated  naming=inferred
// Finds an attribute by case-insensitive name.
ConfigAttr *ConfigBlock_FindAttr(ParseNode *b, const char *name)
{
    ConfigAttr *a = b->firstAttr;
    while (true) {
        if (a == 0)
            return 0;
        const char *an = ConfigBlock_GetName(a);
        if (strcasecmp(an, name) == 0)
            break;
        a = ConfigNode_GetNextAlias2(a);
    }
    return a;
}

// 0x081330ae  size=60  FUN_081330ae  status=translated  naming=inferred
// Returns the first value of the named attribute, or the default.
const char *ConfigBlock_GetString(ParseNode *b, const char *name, const char *defaultValue)
{
    const char *result;
    ConfigAttr *a = ConfigBlock_FindAttr(b, name);
    if (a == 0)
        result = defaultValue;
    else
        result = ConfigAttr_GetFirstValue(a);
    return result;
}

// 0x081330ea  size=52  FUN_081330ea  status=translated  naming=observed
// ConfigTree constructor (variant 1): root block named "Top Level" (string at 0x08160640).
void ConfigTree_Init1(ConfigTree *t)
{
    ConfigBlock_Init(&t->block, "Top Level", 0);
    t->pool = 0;
    t->flag = 0;
}

// 0x0813311e  size=52  FUN_0813311e  status=translated  naming=observed
// ConfigTree constructor (variant 2).
void ConfigTree_Init2(ConfigTree *t)
{
    ConfigBlock_Init(&t->block, "Top Level", 0);
    t->pool = 0;
    t->flag = 0;
}

// 0x08133152  size=20  FUN_08133152  status=translated  naming=inferred
// ConfigTree destructor variant 1.
void ConfigTree_Destroy1(ConfigTree *t)
{
    ConfigTree_Clear(t);
    ConfigBlock_Destroy2(&t->block);
}

// 0x08133196  size=20  FUN_08133196  status=translated  naming=inferred
// ConfigTree destructor variant 2.
void ConfigTree_Destroy2(ConfigTree *t)
{
    ConfigTree_Clear(t);
    ConfigBlock_Destroy2(&t->block);
}

// 0x081331da  size=99  FUN_081331da  status=translated  naming=inferred
// Parses a token stream into the tree, optionally clearing it first; creates the 0x2800 string pool on demand.
char ConfigTree_Parse(ConfigTree *t, TokenStream *ts, char clearFirst, byte flag)
{
    Fx_StringPage *pool;

    if (clearFirst != '\0')
        ConfigTree_Clear(t);
    if (t->pool == 0) {
        Fx_StringPage *p = (Fx_StringPage *)ConfigPool_Alloc(0x10);
        Fx_StringPage_Init2(p, 0x2800);
        t->pool = p;
    }
    ConfigTree_SetFlag(t, flag);
    ConfigBlock_SetFlag(&t->block, flag);
    pool = t->pool;
    return ConfigBlock_Parse(&t->block, ts, &pool);
}

// 0x081332b6  size=43  FUN_081332b6  status=translated  naming=inferred
// Clears the root block and releases the string pool.
void ConfigTree_Clear(ConfigTree *t)
{
    ConfigBlock_Clear(&t->block);
    Fx_StringPage_FreeChain(t->pool);
    t->pool = 0;
}

// 0x08133308  size=11  FUN_08133308  status=translated  naming=inferred
// Returns link->next.
ConfigLink *ConfigLink_GetNext(ConfigLink *l)
{
    return l->next;
}

// 0x08133314  size=14  FUN_08133314  status=translated  naming=inferred
// Sets link->next (same body as ConfigLink_SetNext at 0x0813334a).
void ConfigLink_SetNextAlt(ConfigLink *l, ConfigLink *n)
{
    l->next = n;
}

// 0x08133322  size=19  FUN_08133322  status=translated  naming=inferred
// Allocation wrapper (checked malloc) used for the string pool object.
void *ConfigPool_Alloc(int size)
{
    return Com_MallocZeroed(size);
}

// 0x08133336  size=19  FUN_08133336  status=translated  naming=inferred
// Free wrapper for pool segments.
void ConfigPool_Free(void *p)
{
    Com_Free_A(p);
}

// 0x0813334a  size=14  FUN_0813334a  status=translated  naming=inferred
// Sets link->next.
void ConfigLink_SetNext(ConfigLink *l, ConfigLink *n)
{
    l->next = n;
}

// 0x08133358  size=11  FUN_08133358  status=translated  naming=inferred
// Returns link->sortNext (+8), which is the tail slot for value-list heads.
ConfigLink *ConfigLink_GetSortNext(ConfigLink *l)
{
    return l->sortNext;
}

// 0x08133364  size=14  FUN_08133364  status=translated  naming=inferred
// Sets link->sortNext (+8).
void ConfigLink_SetSortNext(ConfigLink *l, ConfigLink *n)
{
    l->sortNext = n;
}

// 0x08133372  size=14  FUN_08133372  status=translated  naming=inferred
// Sets link->sortPrev (+0xc).
void ConfigLink_SetSortPrev(ConfigLink *l, ConfigLink *p)
{
    l->sortPrev = p;
}

// 0x08133380  size=19  FUN_08133380  status=translated  naming=inferred
// Allocation wrapper (checked malloc) for config nodes.
void *ConfigLink_Alloc(int size)
{
    return Com_MallocZeroed(size);
}

// 0x08133394  size=19  FUN_08133394  status=translated  naming=inferred
// Free wrapper for config nodes.
void ConfigLink_Free(void *p)
{
    Com_Free_A(p);
}

// 0x081333a8  size=24  FUN_081333a8  status=translated  naming=inferred
// Sets ParseNode::flag (+0x2c).
void ConfigBlock_SetFlag(ParseNode *b, byte v)
{
    b->flag = v;
}

// 0x081333c0  size=24  FUN_081333c0  status=translated  naming=inferred
// Sets ConfigTree::flag (+0x34).
void ConfigTree_SetFlag(ConfigTree *t, byte v)
{
    t->flag = v;
}

// 0x081333d8  size=178  FUN_081333d8  status=translated  naming=inferred
// Finds an origin-ref entry within 131072.0 of the given point (bumping its refcount) or appends a new
// entry with refcount 1; returns the table index.
int FX_AddOriginRef(const float *origin)
{
    int i = 0;
    while (true) {
        if (g_fxOriginRefCount <= i) {
            int index;
            Vec3_Copy_08137210(origin, (float *)((byte *)g_fxOriginRefs + g_fxOriginRefCount * 0x10));
            g_fxOriginRefs[g_fxOriginRefCount].refCount = 1;
            index = g_fxOriginRefCount;
            g_fxOriginRefCount = g_fxOriginRefCount + 1;
            return index;
        }
        float d = Vec3_DistanceSquared(origin, g_fxOriginRefs[i].origin);
        if (d < 131072.0f)
            break;
        i = i + 1;
    }
    g_fxOriginRefs[i].refCount = g_fxOriginRefs[i].refCount + 1;
    return i;
}

// 0x0813348a  size=311  FUN_0813348a  status=translated  naming=inferred
// Drops one reference of an origin-ref entry; when it reaches zero the last entry is moved into the
// slot and all effects that referenced the last index are re-pointed.
void FX_ReleaseOriginRef(int index)
{
    FxOriginRef *table;
    int i;

    g_fxOriginRefs[index].refCount = g_fxOriginRefs[index].refCount - 1;
    table = g_fxOriginRefs;
    if (g_fxOriginRefs[index].refCount < 1) {
        g_fxOriginRefCount = g_fxOriginRefCount - 1;
        if (index != g_fxOriginRefCount) {
            table[index].origin[0] = table[g_fxOriginRefCount].origin[0];
            table[index].origin[1] = table[g_fxOriginRefCount].origin[1];
            table[index].origin[2] = table[g_fxOriginRefCount].origin[2];
            table[index].refCount = table[g_fxOriginRefCount].refCount;
            for (i = 0; i < g_08a5c9a0; i++) {
                if (g_fxEffectListA[i]->originRefIndex == g_fxOriginRefCount)
                    g_fxEffectListA[i]->originRefIndex = index;
            }
            for (i = 0; i < g_08a5c9a4; i++) {
                if (g_fxEffectListB[i]->originRefIndex == g_fxOriginRefCount)
                    g_fxEffectListB[i]->originRefIndex = index;
            }
        }
    }
}

// 0x081335c2  size=5  FUN_081335c2  status=translated  naming=unknown
// Empty function (no callers inside the chunk).
void FX_Noop(void)
{
}

// 0x081335c8  size=250  FUN_081335c8  status=translated  naming=inferred
// Destroys every effect in both lists (virtual deleting dtor) and zeroes the counters.
void FX_ClearEffects(void)
{
    int i;

    for (i = 0; i < g_08a5c9a0; i++) {
        if (g_fxEffectListA[i] != 0)
            g_fxEffectListA[i]->vtbl->Destroy(g_fxEffectListA[i]);
    }
    for (i = 0; i < g_08a5c9a4; i++) {
        if (g_fxEffectListB[i] != 0)
            g_fxEffectListB[i]->vtbl->Destroy(g_fxEffectListB[i]);
    }
    g_08a5c9a0 = 0;
    g_08a5c9a4 = 0;
    g_08a5c9a8 = 0;
    g_08a5c9c4 = 0;
    g_fxOriginRefCount = 0;
}

// 0x081336c2  size=106  FUN_081336c2  status=translated  naming=inferred
// Clears all effects; with final!=0 the scheduler is also torn down and unregistered from its context slot.
void FX_ShutdownSystem(char final)
{
    FX_ClearEffects();
    if (g_fxScheduler != 0) {
        FxScheduler_Clear(g_fxScheduler, final, 0);
        if (final != '\0') {
            Fx_FreeSmall(g_fxScheduler);
            g_fxScheduler = 0;
            int ctx = Net_CurrentClientNum();
            g_08a533f0[ctx] = 0;
        }
    }
}

// 0x0813372c  size=42  FUN_0813372c  status=translated  naming=inferred
// Clears all effects and resets the scheduler (no null check on the scheduler).
void FX_Reset(void)
{
    FX_ClearEffects();
    FxScheduler_Clear(g_fxScheduler, 0, 0);
}

// 0x08133756  size=25  FUN_08133756  status=translated  naming=observed
// Stub: prints "FX_GetBoneOrientation called!" and returns 0.
int FX_GetBoneOrientation(int *bolt, struct FxPlacement *placement)
{
    (void)bolt;
    (void)placement;
    printf("FX_GetBoneOrientation called!\n");
    return 0;
}

// 0x08133770  size=15  FUN_08133770  status=translated  naming=inferred
// Resets the blocker count.
void FX_ResetBlockers(void)
{
    g_fxBlockerCount = 0;
}

// 0x08133780  size=154  FUN_08133780  status=translated  naming=inferred
// Initialises the FX system for the current context: first-time counters, shuts down any old state,
// stores the flag, creates the scheduler (0xc bytes) and registers it.
int FX_Init(byte flag)
{
    int ctx = Net_CurrentClientNum();
    if (g_08a5cadc[ctx] == 0) {
        ctx = Net_CurrentClientNum();
        g_08a5cadc[ctx] = 1;
        g_08a5c9a0 = 0;
        g_08a5c9a4 = 0;
        g_08a5c9a8 = 0;
        g_08a5c9c4 = 0;
        g_fxOriginRefCount = 0;
    }
    FX_ShutdownSystem(1);
    g_fxRequireMaterials = flag;
    FxScheduler *sched = (FxScheduler *)Fx_AllocSmall(0xc);
    FxScheduler_ResetList_B(sched);
    g_fxScheduler = sched;
    FX_ResetTemplateCount();
    ctx = Net_CurrentClientNum();
    g_08a533f0[ctx] = (int)g_fxScheduler;
    FX_ClockReset(g_fxSystem);
    return 1;
}

// 0x08133872  size=62  FUN_08133872  status=translated  naming=inferred
// Releases an effect through its vtable (+8) when requested and drops the flagged-effect counter.
void FxEffect_Retire(Effect *e, char release)
{
    if (release != '\0')
        e->vtbl->Release(e);
    if ((e->flags & 0x1000U) != 0)
        g_08a5c9c4 = g_08a5c9c4 - 1;
}

// 0x081338b0  size=80  FUN_081338b0  status=translated  naming=inferred
// Removes effect 'index' from list A: shrinks the live end, moves it out via FX_ListA_SwapEntries, retires it.
void FX_RemoveFromListA(int index, char release)
{
    Effect *e = g_fxEffectListA[index];
    g_fxListAUpdateBound = g_fxListAUpdateBound - 1;
    FX_ListA_SwapEntries(index, g_fxListAUpdateBound);
    FxEffect_Retire(e, release);
}

// 0x08133900  size=80  FUN_08133900  status=translated  naming=inferred
// Same as FX_RemoveFromListA for list B.
void FX_RemoveFromListB(int index, char release)
{
    Effect *e = g_fxEffectListB[index];
    g_fxListBUpdateBound = g_fxListBUpdateBound - 1;
    FX_ListB_SwapEntries(index, g_fxListBUpdateBound);
    FxEffect_Retire(e, release);
}

// 0x08133950  size=5  FUN_08133950  status=translated  naming=unknown
// Empty function called when the effect list is full.
void FX_ListFull(void)
{
}

// 0x08133956  size=75  FUN_08133956  status=translated  naming=inferred
// Reserves a slot at the end of list A (max 0x708 effects total); returns its address or 0.
Effect **FX_ReserveListA(void)
{
    Effect **slot;
    g_08a5c9a8 = g_08a5c9a8 + 1;
    int total = g_08a5c9a8;
    if (total < 0x709) {
        g_08a5c9a0 = g_08a5c9a0 + 1;
        slot = g_fxEffectListA + g_08a5c9a0 - 1;
    } else {
        g_08a5c9a8 = g_08a5c9a8 - 1;
        slot = 0;
    }
    return slot;
}

// 0x081339a2  size=75  FUN_081339a2  status=translated  naming=inferred
// Same as FX_ReserveListA for list B.
Effect **FX_ReserveListB(void)
{
    Effect **slot;
    g_08a5c9a8 = g_08a5c9a8 + 1;
    int total = g_08a5c9a8;
    if (total < 0x709) {
        g_08a5c9a4 = g_08a5c9a4 + 1;
        slot = g_fxEffectListB + g_08a5c9a4 - 1;
    } else {
        g_08a5c9a8 = g_08a5c9a8 - 1;
        slot = 0;
    }
    return slot;
}

// 0x08133b50  size=200  FUN_08133b50  status=translated  naming=inferred
// Resets the blocker count then runs UpdateFlagged (vtbl +0x1c) on flagged (0x1000) effects of lists B then A.
void FX_UpdateFlagged(void)
{
    int i;

    g_fxBlockerCount = 0;
    if (g_08a5c9c4 != 0) {
        for (i = 0; i < g_08a5c9a4; i++) {
            Effect *e = g_fxEffectListB[i];
            if ((e->flags & 0x1000U) != 0)
                e->vtbl->UpdateFlagged(e);
        }
        for (i = 0; i < g_08a5c9a0; i++) {
            Effect *e = g_fxEffectListA[i];
            if ((e->flags & 0x1000U) != 0)
                e->vtbl->UpdateFlagged(e);
        }
    }
}

// 0x08133c18  size=342  FUN_08133c18  status=translated  naming=inferred
// Returns the product of blocker attenuations along the segment from 'from' to 'to'
// (1.0 when there are no blockers or the segment is shorter than the cvar threshold).
float FX_GetAttenuation(const float *from, const float *to)
{
    float result;
    float dir[4];
    float dist;

    if (g_fxBlockerCount == 0) {
        result = 1.0f;
    } else {
        Vec3_Sub_08137284(to, from, dir);
        dist = Vec3_Normalize(dir);
        if (dist < g_08a53ca0->current.value) {
            result = 1.0f;
        } else {
            float half = dist * 0.5f;
            float factor = 1.0f;
            int i;
            for (i = 0; i < g_fxBlockerCount; i++) {
                FxBlocker *blk = (FxBlocker *)g_fxBlockers + i;
                float rel[4];
                float point[4];
                Vec3_Sub_08137284(blk->origin, from, rel);
                float t = Vec3_Dot_08137354(rel, dir);
                float off = Float_Abs_081371a4(t - half);
                if (off <= half) {
                    Vec3_MA_08137300(from, t, dir, point);
                    float d = Vec3_DistanceSquared(blk->origin, point);
                    if (d < blk->radius)
                        factor = factor * blk->attenuation;
                }
            }
            result = factor;
        }
    }
    return result;
}

// 0x08133d6e  size=157  FUN_08133d6e  status=translated  naming=inferred
// Frees the list A effects between the live end and the old count (those removed during the update).
void FX_FreeRemovedListA(void)
{
    int i;
    for (i = g_fxListAUpdateBound; i < g_08a5c9b4; i++) {
        Effect *e = g_fxEffectListA[i];
        FX_ReleaseOriginRef(e->originRefIndex);
        if (e != 0)
            e->vtbl->Destroy(e);
        g_08a5c9a0 = g_08a5c9a0 - 1;
        g_fxEffectListA[i] = g_fxEffectListA[g_08a5c9a0];
        g_08a5c9a8 = g_08a5c9a8 - 1;
    }
}

// 0x08133e0c  size=157  FUN_08133e0c  status=translated  naming=inferred
// Same as FX_FreeRemovedListA for list B.
void FX_FreeRemovedListB(void)
{
    int i;
    for (i = g_fxListBUpdateBound; i < g_08a5c9b8; i++) {
        Effect *e = g_fxEffectListB[i];
        FX_ReleaseOriginRef(e->originRefIndex);
        if (e != 0)
            e->vtbl->Destroy(e);
        g_08a5c9a4 = g_08a5c9a4 - 1;
        g_fxEffectListB[i] = g_fxEffectListB[g_08a5c9a4];
        g_08a5c9a8 = g_08a5c9a8 - 1;
    }
}

// 0x08133fa0  size=93  FUN_08133fa0  status=translated  naming=inferred
// Serialises an effect into a message: type byte, end time, then the effect's own data.
void FxEffect_Write(Effect *e, FxMsg *msg)
{
    byte type = (byte)e->vtbl->GetType(e);
    MsgStream_WriteDecodedByte(msg, type);
    FxStream_WriteInt(msg, e->endTime);
    e->vtbl->Serialize(e, msg);
}

// 0x081340ee  size=50  FUN_081340ee  status=translated  naming=inferred
// Bounds-checked table lookup (count at +4, entries at +8).
void *FxPtrTable_Get(FxEffectTemplate *table, int index)
{
    void *result;
    if (index < table->count)
        result = table->entries[index];
    else
        result = 0;
    return result;
}

// 0x08134120  size=164  FUN_08134120  status=translated  naming=inferred
// Reads serialised effects from a message and adds them to the lists (type 1,2,3,4,5,7,9,11,12 create
// different effect classes; other ids are skipped). A zero type byte ends the stream.
void FX_ReadEffects(void *source)
{
    FxMsg msg;      /* 32-byte message object (ctor MsgStream_ResetAlt) */
    Effect *e;
    int count;

    MsgStream_ResetAlt(&msg);
    FX_ShutdownSystem(0);
    MsgStream_InitRead(&msg, source);
    FX_SerializeSlot(g_fxSystem, &msg);
    FX_SerializePlaybackList(g_fxScheduler, &msg);
    count = 0;
    do {
        unsigned int type = MsgStream_ReadDecodedByte(&msg);
        type = type & 0xff;
        if (type == 0) {
            Handle_GetTargetField8(&msg);
            return;
        }
        FxStream_ReadInt(&msg);   /* serialised end time; value is not used */
        switch (type) {
        case 1:
            e = (Effect *)FxPool_AllocParticleObj(0x24c);
            Particle_ConstructBase(e);
            break;
        case 2:
            e = (Effect *)FxPool_AllocLineObj(600);
            Line_ConstructBase(e);
            break;
        case 3:
            e = (Effect *)FxPool_AllocTailObj(0x278);
            Tail_ConstructBase(e);
            break;
        case 4:
            e = (Effect *)FxPool_AllocCylinderObj(0x278);
            Cylinder_ConstructBase(e);
            break;
        case 5:
            e = (Effect *)FxPool_AllocEmitterObj(0x29c);
            Emitter_ConstructBase(e);
            break;
        case 7:
            e = (Effect *)FxPool_AllocOrientedParticleObj(600);
            OrientedParticle_ConstructBase(e);
            break;
        case 9:
            e = (Effect *)FxPool_AllocLightFlashObj(0xfc);
            Light_ConstructBase(e);
            break;
        case 0xb:
            e = (Effect *)FxPool_AllocLightFlashObj(0xfc);
            Flash_Construct(e);
            break;
        case 0xc:
            e = (Effect *)FxPool_AllocCloudObj(0x27c);
            Cloud_Construct(e);
            break;
        default:
            goto next;
        }
        e->vtbl->Serialize(e, &msg);
        {
            int idx = FxObj_GetField38(e);
            FxEffectTemplate *table = (FxEffectTemplate *)FxObj_GetField34(e);
            void *entry = FxPtrTable_Get(table, idx);
            if (entry == 0) {
                if (e != 0)
                    e->vtbl->Destroy(e);
            } else {
                e->vtbl->Attach(e, entry);
                if ((e->flags & 0x1000U) != 0)
                    g_08a5c9c4 = g_08a5c9c4 + 1;
                if (FxRef_IsValid(&e->handle) == 0) {
                    g_fxEffectListB[g_08a5c9a4] = e;
                    g_08a5c9a4 = g_08a5c9a4 + 1;
                    g_08a5c9a8 = g_08a5c9a8 + 1;
                } else {
                    g_fxEffectListA[g_08a5c9a0] = e;
                    g_08a5c9a0 = g_08a5c9a0 + 1;
                    g_08a5c9a8 = g_08a5c9a8 + 1;
                }
            }
        }
    next:
        count = count + 1;
    } while (true);
}

// 0x0813461e  size=226  FUN_0813461e  status=translated  naming=inferred
// qsort comparator for draw entries: sortKey, then origin-ref rank, then sortKey2, then farther first.
int FX_CompareDrawEntries(const void *pa, const void *pb)
{
    const FxDrawEntry *a = (const FxDrawEntry *)pa;
    const FxDrawEntry *b = (const FxDrawEntry *)pb;
    Effect *ea = a->effect;
    Effect *eb = b->effect;
    int diff;
    int tie;

    diff = ea->sortKey - eb->sortKey;
    if (diff == 0) {
        diff = g_08a53cac[ea->originRefIndex] - g_08a53cac[eb->originRefIndex];
        if (diff == 0) {
            diff = ea->sortKey2 - eb->sortKey2;
            if (diff == 0) {
                if (a->dist < b->dist)
                    tie = 1;
                else
                    tie = -1;
                diff = tie;
            }
        }
    }
    return diff;
}

// 0x08134700  size=60  FUN_08134700  status=translated  naming=inferred
// qsort comparator on (index, distance) pairs: larger distance first.
int FX_CompareByDistance(const void *pa, const void *pb)
{
    const FxDrawEntry *a = (const FxDrawEntry *)pa;
    const FxDrawEntry *b = (const FxDrawEntry *)pb;
    int result;
    if (a->dist < b->dist)
        result = 1;
    else
        result = -1;
    return result;
}

// 0x0813473c  size=242  FUN_0813473c  status=translated  naming=inferred
// Sorts the draw list: first ranks the origin-ref entries by distance to the view origin
// (farther = lower rank), publishes the ranks in g_08a53cac, then qsorts the draw entries.
void FX_SortDrawList(FxDrawEntry *list, size_t count)
{
    int rank[1800];
    struct { int index; float dist; } pairs[1800];
    int i;

    for (i = 0; i < g_fxOriginRefCount; i++) {
        pairs[i].index = i;
        pairs[i].dist = Vec3_DistanceSquared(g_fxOriginRefs[i].origin, g_fxSystem->viewOrigin);
    }
    qsort(pairs, g_fxOriginRefCount, 8, FX_CompareByDistance);
    for (i = 0; i < g_fxOriginRefCount; i++)
        rank[pairs[i].index] = i;
    g_08a53cac = rank;
    qsort(list, count, 8, FX_CompareDrawEntries);
    g_08a53cac = 0;
}

// 0x0813482e  size=192  FUN_0813482e  status=translated  naming=inferred
// Per-frame update of list A: effects past their end time (flagged 0x400) or whose Update returns 0
// are removed; finally frees the removed ones.
void FX_UpdateListA(void)
{
    int i;

    g_fxListAUpdateBound = g_08a5c9a0;
    g_08a5c9b4 = g_08a5c9a0;
    i = 0;
    while (i < g_fxListAUpdateBound) {
        Effect *e = g_fxEffectListA[i];
        if (e->endTime < g_fxSystem->time) {
            FxObj_ClearFlagsA8(e, 0x400);
            FX_RemoveFromListA(i, 1);
        } else {
            char alive = e->vtbl->Update(e);
            if (alive == '\0')
                FX_RemoveFromListA(i, 1);
            else
                i = i + 1;
        }
    }
    FX_FreeRemovedListA();
}

// 0x081348ee  size=192  FUN_081348ee  status=translated  naming=inferred
// Same as FX_UpdateListA for list B.
void FX_UpdateListB(void)
{
    int i;

    g_fxListBUpdateBound = g_08a5c9a4;
    g_08a5c9b8 = g_08a5c9a4;
    i = 0;
    while (i < g_fxListBUpdateBound) {
        Effect *e = g_fxEffectListB[i];
        if (e->endTime < g_fxSystem->time) {
            FxObj_ClearFlagsA8(e, 0x400);
            FX_RemoveFromListB(i, 1);
        } else {
            char alive = e->vtbl->Update(e);
            if (alive == '\0')
                FX_RemoveFromListB(i, 1);
            else
                i = i + 1;
        }
    }
    FX_FreeRemovedListB();
}

// 0x081349ae  size=181  FUN_081349ae  status=translated  naming=inferred
// Updates list A from index 'start' only (no free pass; old count taken from the live end).
void FX_UpdateListAFrom(int start)
{
    int i;

    g_08a5c9b4 = g_fxListAUpdateBound;
    i = start;
    while (i < g_fxListAUpdateBound) {
        Effect *e = g_fxEffectListA[i];
        if (e->endTime < g_fxSystem->time) {
            FxObj_ClearFlagsA8(e, 0x400);
            FX_RemoveFromListA(i, 1);
        } else {
            char alive = e->vtbl->Update(e);
            if (alive == '\0')
                FX_RemoveFromListA(i, 1);
            else
                i = i + 1;
        }
    }
}

// 0x08134a64  size=171  FUN_08134a64  status=translated  naming=inferred
// Appends list A effects that are not hidden to draw list A with their view distance.
void FX_CollectDrawListA(void)
{
    int i;

    for (i = g_08a5c9bc; i < g_fxListAUpdateBound; i++) {
        Effect *e = g_fxEffectListA[i];
        if (g_08a53c88->enabled == '\0' || e->vtbl->IsHidden(e) == '\0') {
            g_08a6ad00[g_08a5cae4].effect = e;
            int slot = g_08a5cae4;
            g_08a6ad00[slot].dist = Vec3_DistanceSquared(e->origin, g_fxSystem->viewOrigin);
            g_08a5cae4 = g_08a5cae4 + 1;
        }
    }
    g_08a5c9bc = g_fxListAUpdateBound;
}

// 0x08134b10  size=181  FUN_08134b10  status=translated  naming=inferred
// Updates list B from index 'start' only (no free pass).
void FX_UpdateListBFrom(int start)
{
    int i;

    g_08a5c9b8 = g_fxListBUpdateBound;
    i = start;
    while (i < g_fxListBUpdateBound) {
        Effect *e = g_fxEffectListB[i];
        if (e->endTime < g_fxSystem->time) {
            FxObj_ClearFlagsA8(e, 0x400);
            FX_RemoveFromListB(i, 1);
        } else {
            char alive = e->vtbl->Update(e);
            if (alive == '\0')
                FX_RemoveFromListB(i, 1);
            else
                i = i + 1;
        }
    }
}

// 0x08134bc6  size=185  FUN_08134bc6  status=translated  naming=inferred
// Appends list B effects that are not hidden to draw list B (only when g_08a53818 is set).
void FX_CollectDrawListB(void)
{
    int i;

    if (g_08a53818 != 0) {
        for (i = g_08a5c9c0; i < g_fxListBUpdateBound; i++) {
            Effect *e = g_fxEffectListB[i];
            if (g_08a53c88->enabled == '\0' || e->vtbl->IsHidden(e) == '\0') {
                g_08a674c0[g_08a5cae0].effect = e;
                int slot = g_08a5cae0;
                g_08a674c0[slot].dist = Vec3_DistanceSquared(e->origin, g_fxSystem->viewOrigin);
                g_08a5cae0 = g_08a5cae0 + 1;
            }
        }
        g_08a5c9c0 = g_fxListBUpdateBound;
    }
}

// 0x08134c80  size=272  FUN_08134c80  status=translated  naming=inferred
// Builds the final draw list (B entries then A entries), optionally sorts it, draws every entry
// (vtbl +0x14) and prints the "Active/Drawn/Scheduled FX" counters when the debug cvar is set.
void FX_DrawEffects(void)
{
    int i;

    FX_CollectDrawListB();
    FX_CollectDrawListA();
    for (i = 0; i < g_08a5cae4; i++) {
        float d = g_08a6ad00[i].dist;
        g_08a674c0[g_08a5cae0].effect = g_08a6ad00[i].effect;
        int slot = g_08a5cae0;
        g_08a674c0[slot].dist = d;
        g_08a5cae0 = g_08a5cae0 + 1;
    }
    FX_UpdateFlagged();
    if (g_08a53c8c->enabled != '\0')
        FX_SortDrawList(g_08a674c0, g_08a5cae0);
    if (g_08a53c84->enabled != '\0') {
        for (i = 0; i < g_08a5cae0; i++)
            g_08a674c0[i].effect->vtbl->Draw(g_08a674c0[i].effect);
    }
    if (g_08a53c90->enabled != '\0') {
        FxPrintf("Active    FX: %i\n", g_08a5c9a8);
        FxPrintf("Drawn     FX: %i\n", g_08a5cae0);
        FxPrintf("Scheduled FX: %i\n", g_fxScheduler->pendingCount);
    }
}

// 0x08134d90  size=1286  FUN_08134d90  status=translated  naming=inferred
// Computes the spawn origin of an effect element: local offset (randomised ranges, rotated by the axis
// unless flags2 bit 0x40), plus spherical (flag 1) or cone/disc (flag 4) random spread, optionally rebuilding
// the axis from the spread direction (flag 2); the result is copied or transformed to outOrigin.
// axis points at three consecutive vec3 (forward/right/up) that may be rewritten.
void FxSpawner_ComputeOrigin(FxSpawner *spawner, float *outOrigin, const float *base, float *axis)
{
    float up[3];
    float pos[4];
    fx_primitive_s *def;
    float rx, ry, rz;

    up[0] = 0.0f;
    up[1] = 0.0f;
    up[2] = 1.0f;
    def = spawner->def;
    if ((def->flags2 & 0x40) == 0) {
        rz = FxRange_Random(&def->offZ);
        ry = FxRange_Random(&def->offY);
        rx = FxRange_Random(&def->offX);
        Mat3_TransformComponents(axis, rx, ry, rz, pos);
    } else {
        rz = FxRange_Random(&def->offZ);
        ry = FxRange_Random(&def->offY);
        rx = FxRange_Random(&def->offX);
        Vec3_Set_081371ec(pos, rx, ry, rz);
    }
    Vec3_Add_08137240(pos, base, pos);
    if ((def->flags2 & 1) == 0) {
        if ((def->flags2 & 4) != 0) {
            float spreadVec[4];
            float rotated[8];
            float rnd = Rand_Float(-0.5f, 0.5f);
            float spreadAmt = FxRange_Random(&def->spread);
            float side = (float)(spreadAmt * rnd);
            float radius = FxRange_Random(&def->radius);
            Vec3_Scale_081372c8(axis + 3, radius, spreadVec);
            Vec3_MA_08137300(spreadVec, side, axis, spreadVec);
            float angle = Rand_Float(0.0f, 360.0f);
            Vec3_RotateAroundAxis(rotated, axis, spreadVec, angle);
            Vec3_Add_08137240(pos, rotated, pos);
            if ((def->flags2 & 2) != 0) {
                float len = Vec3_Length_08137388(rotated);
                if (len != 0.0f || (len != len)) {
                    Vec3_Scale_081372c8(rotated, 1.0f / len, axis);
                    float z = Float_Abs_081371a4(axis[2]);
                    if (0.999f <= z)
                        Vec3_Set_081371ec(up, 0.0f, 1.0f, 0.0f);
                    Vec3_Cross(up, axis, axis + 3);
                    Vec3_Normalize(axis + 3);
                    Vec3_Cross(axis, axis + 3, axis + 6);
                }
            }
        }
    } else {
        float sinYaw, cosYaw, sinPitch, cosPitch;
        float yaw, pitch, radius, height;
        float dir[8];
        yaw = (float)(0.017453292519943295 * Rand_Float(0.0f, 360.0f));
        Math_SinCos(yaw, &sinYaw, &cosYaw);
        pitch = (float)(0.017453292519943295 * Rand_Float(0.0f, 180.0f));
        Math_SinCos(pitch, &sinPitch, &cosPitch);
        radius = FxRange_Random(&def->radius);
        height = FxRange_Random(&def->spread);
        Vec3_Set_081371ec(dir, sinYaw * radius * sinPitch, cosYaw * radius * sinPitch, cosPitch * height);
        Vec3_Add_08137240(pos, dir, pos);
        if ((def->flags2 & 2) != 0) {
            float len = Vec3_Length_08137388(dir);
            if (len != 0.0f || (len != len)) {
                Vec3_Scale_081372c8(dir, 1.0f / len, axis);
                Vec3_PerpendicularBasis(axis, axis + 3, axis + 6);
            }
        }
    }
    if (FxRef_IsValid(&spawner->handle) == 0) {
        Vec3_Copy_08137210(pos, outOrigin);
    } else {
        FxBinding *binding = FxRef_Get(&spawner->handle);
        const float *h = FxBinding_ResolveFrame(binding);
        Orient_PointWorldToLocal(h, pos, outOrigin);
    }
}

// 0x08135296  size=721  FUN_08135296  status=translated  naming=inferred
// Origin for a traced element: without flags2 bit 8 it is a random offset (rotated by axis unless 0x80)
// added to base; with bit 8 a 16384-unit ray along axis from 'start' is traced, the hit position is
// stored in 'out' and, with bit 0x10, the scheduler is notified.
void FxSpawner_ComputeTraceOrigin(fx_primitive_s *def, const float *start, float *out, const float *base, float *axis)
{
    float end[4];
    float offset[4];
    int trace;
    byte schedOut[44];
    float rx, ry, rz;

    if ((def->flags2 & 8) == 0) {
        if ((def->flags2 & 0x80) == 0) {
            rz = FxRange_Random(&def->traceZ);
            ry = FxRange_Random(&def->traceY);
            rx = FxRange_Random(&def->traceX);
            Mat3_TransformComponents(axis, rx, ry, rz, out);
        } else {
            rz = FxRange_Random(&def->traceZ);
            ry = FxRange_Random(&def->traceY);
            rx = FxRange_Random(&def->traceX);
            Vec3_Set_081371ec(out, rx, ry, rz);
        }
        Vec3_Add_08137240(out, base, out);
    } else {
        Vec3_MA_08137300(start, 16384.0f, axis, end);
        if ((def->flags2 & 0x20) != 0) {
            if ((def->flags2 & 0x80) == 0) {
                rz = FxRange_Random(&def->traceZ);
                ry = FxRange_Random(&def->traceY);
                rx = FxRange_Random(&def->traceX);
                Mat3_TransformComponents(axis, rx, ry, rz, offset);
                Vec3_Add_08137240(end, offset, end);
            } else {
                rz = FxRange_Random(&def->traceZ);
                ry = FxRange_Random(&def->traceY);
                rx = FxRange_Random(&def->traceX);
                Vec3_Set_081371ec(out, rx, ry, rz);
                Vec3_Add_08137240(out, end, end);
            }
        }
        FX_InitSampleRecord(g_fxSystem, (float *)&trace, start, vec3_origin, vec3_origin, end, -1, 1);
        VectorLerp(start, end, trace, out);
        if ((def->flags2 & 0x10) != 0) {
            int h = FxPickList_Pick(&def->handle70);
            FxScheduler_PlayEffect_Forward(g_fxScheduler, (FxEffectTemplate *)h, out, (float *)schedOut);
        }
    }
}

// 0x08135568  size=85  FUN_08135568  status=translated  naming=inferred
// Advances a position by the effect's velocity for 'msec' milliseconds (only when msec > 0).
void FxSpawner_ApplyElapsed(fx_primitive_s *def, Effect *e, float *pos, int msec)
{
    float vel[6];
    if (0 < msec) {
        ((Particle *)e)->EvalDisplacement(msec, vel);
        Vec3_MA_08137300(pos, (float)msec * 0.001f, vel, pos);
    }
}

// 0x081355be  size=465  FUN_081355be  status=translated  naming=inferred
// Registers a freshly created effect: reserves a list slot (A when the spawner handle is set, else B),
// adds an origin ref, and fills the effect from the definition (time range, ids, flags, handles).
// Returns 0 (after leaking the origin ref) when the lists are full.
char FxSpawner_Setup(FxSpawner *s, Effect *e, const float *origin)
{
    fx_primitive_s *def = s->def;
    Effect **slot;
    int originIndex;
    char result;

    if (FxRef_IsValid(&s->handle) == 0)
        slot = FX_ReserveListB();
    else
        slot = FX_ReserveListA();
    originIndex = FX_AddOriginRef(origin);
    if (slot == 0) {
        FX_ListFull();
        result = 0;
    } else {
        if ((def->flags & 0x1000) != 0)
            g_08a5c9c4 = g_08a5c9c4 + 1;
        *slot = e;
        FxObj_SetFlagsA8(e, def->flags);
        FxSystem *sys = g_fxSystem;
        float life = FxRange_Random(&def->lifetime);
        e->SetTimes(g_fxSystem->time, (int)life + sys->time);
        FxObj_SetField34(e, s->id);
        FxObj_SetField38(e, def->id44);
        FxObj_SetField10(e, def->value98);
        FxObj_SetVec14OrClear(e, def->valueA0);
        FxObj_SetVec20OrClear(e, def->valueAC);
        int h = FxPickList_Pick(&def->handle78);
        FxObj_SetField30(e, h);
        h = FxPickList_Pick(&def->handle70);
        FxObj_SetField2c(e, h);
        e->vtbl->Init(e, def);
        e->SetBinding((struct FxRef *)&s->handle);
        e->originRefIndex = originIndex;
        result = 1;
    }
    return result;
}

// 0x08135790  size=70  FUN_08135790  status=translated  naming=inferred
// Resets sortKey to 0, or -1 when the effect's id40 is flagged by the FX system.
void FxEffect_ResetSortKey(Effect *e)
{
    e->sortKey = 0;
    if (e->sortKey2 != 0) {
        char flagged = (char)FX_IsEffectFlagged_Stub(g_fxSystem, e->sortKey2);
        if (flagged != '\0')
            e->sortKey = -1;
    }
}

// 0x081357d6  size=383  FUN_081357d6  status=translated  naming=inferred
// Sets the animation sequence of an effect: frame chosen by def->frameMode (0 fixed, 1 random,
// 2 caller-supplied) and rate by def->rateMode (0 ms value, 1 frames over duration).
void FxSpawner_SetupSequence(fx_primitive_s *def, Effect *e, int count, int index)
{
    int seq;
    int frame;
    float rate;
    int frames;

    FxPickList_PickOut(&seq, &def->seqHandle);
    if (seq == 0) {
        frame = 0;
        rate = 0.0f;
    } else {
        frames = FX_GetSpriteFrameCount_Stub(g_fxSystem, seq);
        if (frames == 1) {
            frame = 0;
            rate = 0.0f;
        } else {
            if (def->frameMode == 0)
                frame = def->frameCount - 1;
            else if (def->frameMode == 1)
                frame = Rand_Int(0, frames);
            else if (def->frameMode == 2)
                frame = index;
            else
                frame = 0;
            if (def->rateMode == 0)
                rate = def->rateMs / 1000.0f;
            else if (def->rateMode == 1)
                rate = (float)frames / (float)count;
            else
                rate = 0.0f;
        }
    }
    FxObj_SetField108(e, frame);
    FxObj_SetField10c(e, rate);
    FxObj_SetField110(e, def->value298);
    FxObj_SetField114(e, def->value29c);
    FxObj_SetField40_B(e, seq);
    FxEffect_ResetSortKey(e);
}

// 0x08135956  size=740  FUN_08135956  status=translated  naming=inferred
// Randomises an effect from its definition: per-flag random values in [0,1), ranges, spawn origin
// (FxSpawner_ComputeOrigin) and axis. The sixth argument is passed by the caller but unused.
void FxSpawner_Randomize(FxSpawner *s, Effect *e, float *a, const float *b, float *axis)
{
    fx_primitive_s *def = s->def;
    float r0, r1, r2;

    if ((def->flags & 0x2000) != 0)
        FxObj_SetField118(e, Rand_Float(0.0f, 1.0f));
    if ((def->flags & 0x4000) != 0)
        FxObj_SetField11c(e, Rand_Float(0.0f, 1.0f));
    if ((unsigned short)def->flags & 0x8000)
        FxObj_SetField120(e, Rand_Float(0.0f, 1.0f));
    if ((def->flags & 0x10000) != 0)
        FxObj_SetField124(e, Rand_Float(0.0f, 1.0f));
    if ((def->flags & 0x40000) != 0)
        FxObj_SetField128(e, Rand_Float(0.0f, 1.0f));
    if ((def->flags & 0x80000) != 0) {
        r0 = Rand_Float(0.0f, 1.0f);
        r1 = Rand_Float(0.0f, 1.0f);
        r2 = Rand_Float(0.0f, 1.0f);
        ((Particle *)e)->SetVelocityRand(r2, r1, r0);
    }
    if ((def->flags & 0x100000) != 0) {
        r0 = Rand_Float(0.0f, 1.0f);
        r1 = Rand_Float(0.0f, 1.0f);
        r2 = Rand_Float(0.0f, 1.0f);
        ((Particle *)e)->SetAccelRand(r2, r1, r0);
    }
    FxObj_SetFieldF4(e, FxRange_Random(&def->range258));
    FxObj_SetFieldF8(e, FxRange_Random(&def->rangeF8));
    FxSpawner_ComputeOrigin(s, a, b, axis);
    ((Particle *)e)->SetBasis(axis);
    FxObj_SetField104(e, def->value9c);
    FxObj_SetField100(e, FxRange_Random(&def->range280));
    FxObj_SetField44(e, FxRange_Random(&def->range220));
}

// 0x08135c3a  size=44  FUN_08135c3a  status=translated  naming=inferred
// Spawns one effect (class with vtable 0x24c bytes): setup, randomise, sequence, elapsed-time advance.
// The object is deleted through its vtable if setup fails.
void FxSpawner_Spawn(FxSpawner *s, float *axis, const float *origin, int msec, int index)
{
    Effect *e;
    float pos[7];

    e = (Effect *)FxPool_AllocParticleObj(0x24c);
    Particle_ConstructBase(e);
    if (e != 0) {
        char ok = FxSpawner_Setup(s, e, origin);
        if (ok == '\0') {
            if (e != 0)
                e->vtbl->Destroy(e);
        } else {
            FxSpawner_Randomize(s, e, pos, origin, axis);
            int duration = FxObj_GetSpan(e);
            FxSpawner_SetupSequence(s->def, e, duration, index);
            FxSpawner_ApplyElapsed(s->def, e, pos, msec);
            FxObj_SetOrigin(e, pos);
        }
    }
}
