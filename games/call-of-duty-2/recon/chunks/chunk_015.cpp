#include <ctype.h>
#include "cod2_types.h"
#include "cod2_canon.h"
#include "cod2_globals.h"
#include "cod2_prototypes.h"
#include "chunk_015_types.h"

/* Chunk 015: script variable table accessors, script value casts and binary operators,
 * class/field tables and the field-definition file loader. All names are inferred
 * (the binary is stripped); error strings and the id Tech 3 / IW script VM structure are
 * the naming evidence. */

#define SCR_TYPE(flags) ((flags) & 0x1f)

/* Type name used in script error messages. */
static inline const char *Scr_TypeName(unsigned int type)
{
    return g_scrTypeNames[type];
}

// 0x0807cf02  size=249  FUN_0807cf02  status=translated  naming=inferred
// Scr_EvalSizeValue: string "size cannot be applied to %s"; replaces a value with its size (array count / strlen).
void Scr_EvalSizeValue(struct VariableValue *value)
{
    if (value->type == SCR_VALUE_OBJECT) {
        unsigned int varId = value->u.pointerValue;
        unsigned int size;
        value->type = SCR_VALUE_INT;
        if (SCR_TYPE(g_scrVar[varId].info) == SCR_VAR_ARRAY)
            size = SCRVAR_AUX(g_scrVar[varId]);
        else
            size = 1;
        value->u.pointerValue = size;
        ScrVar_Release(varId);
    } else if (value->type == SCR_VALUE_STRING) {
        value->type = SCR_VALUE_INT;
        unsigned int stringId = value->u.pointerValue;
        const char *s = ScrStr_TextFromHandle(stringId);
        value->u.pointerValue = strlen(s);
        ScrStr_Release(stringId);
    } else {
        const char *msg = va("size cannot be applied to %s", Scr_TypeName(value->type));
        SCR_REMOVE_REF(value);
        value->type = SCR_VALUE_UNDEFINED;
        Scr_Error(msg);
    }
}

// 0x0807cffc  size=33  FUN_0807cffc  status=translated  naming=inferred
// Scr_GetArraySize: returns entry +6 (array element count); same body as Scr_GetArraySizeDup (see Scr_GetArraySizeDup).
uint16 Scr_GetArraySize(int varId)
{
    return SCRVAR_AUX(g_scrVar[varId]);
}

// 0x0807d01e  size=72  FUN_0807d01e  status=translated  naming=inferred
// Scr_GetNextSibling: iteration step used as for (c = f(id); c; c = f(c)) over an object's children; 0 when end/invalid type.
unsigned int Scr_GetNextSibling(int varId)
{
    unsigned int next = g_scrVar[g_scrVar[varId].prevSibling].hashId;
    if (SCR_TYPE(g_scrVar[next].info) > 0xe)
        next = 0;
    return next;
}

// 0x0807d066  size=92  FUN_0807d066  status=translated  naming=unknown
// Scr_GetSiblingViaPrevLinks: follows link_b twice from the link slot then link_a; 0 when target type > 0xe. Meaning of the links unknown.
unsigned int Scr_GetSiblingViaPrevLinks(int varId)
{
    unsigned int slot = g_scrVar[varId].prevSibling;
    unsigned int a = g_scrVar[slot].hashPrev;
    unsigned int b = g_scrVar[a].hashPrev;
    unsigned int result = g_scrVar[b].hashId;
    if (SCR_TYPE(g_scrVar[result].info) > 0xe)
        result = 0;
    return result;
}

// 0x0807d0c2  size=20  FUN_0807d0c2  status=translated  naming=inferred
// Scr_GetVarNameId: flags >> 8.
unsigned int Scr_GetVarNameId(int varId)
{
    return g_scrVar[varId].info >> 8;
}

// 0x0807d0d6  size=76  FUN_0807d0d6  status=translated  naming=unknown
// Scr_GetOrCreateObjectId: untyped entry becomes type 1 and gets a fresh id from ScrVar_AllocStruct.
int Scr_GetOrCreateObjectId(int varId)
{
    if (SCR_TYPE(g_scrVar[varId].info) == SCR_VAR_FREE_SLOT) {
        g_scrVar[varId].info = g_scrVar[varId].info | 1;
        int created = ScrVar_AllocStruct();
        g_scrVar[varId].u.intValue = created;
    }
    return g_scrVar[varId].u.intValue;
}

// 0x0807d122  size=76  FUN_0807d122  status=translated  naming=unknown
// Scr_GetOrCreateArrayId: as Scr_GetOrCreateObjectId but allocates through ScrVar_AllocArray (array allocation).
int Scr_GetOrCreateArrayId(int varId)
{
    if (SCR_TYPE(g_scrVar[varId].info) == SCR_VAR_FREE_SLOT) {
        g_scrVar[varId].info = g_scrVar[varId].info | 1;
        int created = ScrVar_AllocArray();
        g_scrVar[varId].u.intValue = created;
    }
    return g_scrVar[varId].u.intValue;
}

// 0x0807d16e  size=32  FUN_0807d16e  status=translated  naming=inferred
// Scr_GetVarValue: raw 32-bit value at entry +4.
int Scr_GetVarValue(int varId)
{
    return g_scrVar[varId].u.intValue;
}

// 0x0807d18e  size=54  FUN_0807d18e  status=translated  naming=unknown
// Scr_IsVarTypeBelowArray: entry type < 0x16.
bool Scr_IsVarTypeBelowArray(int varId)
{
    return SCR_TYPE(g_scrVar[varId].info) < 0x16;
}

// 0x0807d1c4  size=54  FUN_0807d1c4  status=translated  naming=unknown
// Scr_IsVarNotDead: entry type != 0x17.
bool Scr_IsVarNotDead(int varId)
{
    return SCR_TYPE(g_scrVar[varId].info) != SCR_VAR_DEAD;
}

// 0x0807d1fa  size=45  FUN_0807d1fa  status=translated  naming=inferred
// Scr_EvalBoolNot: casts to bool then logical not (only if the cast left an int).
void Scr_EvalBoolNot(struct VariableValue *value)
{
    Scr_CastBool(value);
    if (value->type == SCR_VALUE_INT)
        value->u.pointerValue = (value->u.pointerValue == 0);
}

// 0x0807d228  size=95  FUN_0807d228  status=translated  naming=inferred
// Scr_EvalBitNot: string "~ cannot be applied to \"%s\"".
void Scr_EvalBitNot(struct VariableValue *value)
{
    if (value->type == SCR_VALUE_INT) {
        value->u.pointerValue = ~value->u.pointerValue;
    } else {
        unsigned int type = value->type;
        SCR_REMOVE_REF(value);
        value->type = SCR_VALUE_UNDEFINED;
        const char *msg = va("~ cannot be applied to \"%s\"", Scr_TypeName(type));
        Scr_Error(msg);
    }
}

// 0x0807d288  size=149  FUN_0807d288  status=translated  naming=inferred
// Scr_CastBool: string "cannot cast %s to bool". Int -> (v != 0); float -> int (v != 0.0f, NaN is true).
void Scr_CastBool(struct VariableValue *value)
{
    if (value->type == SCR_VALUE_INT) {
        value->u.pointerValue = (value->u.intValue != 0);
    } else if (value->type == SCR_VALUE_FLOAT) {
        value->type = SCR_VALUE_INT;
        value->u.pointerValue = (value->u.floatValue != 0.0f);
    } else {
        int type = value->type;
        SCR_REMOVE_REF(value);
        value->type = SCR_VALUE_UNDEFINED;
        const char *msg = va("cannot cast %s to bool", Scr_TypeName(type));
        Scr_Error(msg);
    }
}

// 0x0807d31e  size=257  FUN_0807d31e  status=translated  naming=inferred
// Scr_CastString: string "cannot cast %s to string"; returns 1 on success, 0 on failure (message kept in g_scr_compile_error_message).
int Scr_CastString(struct VariableValue *value)
{
    int result;
    if (value->type == SCR_VALUE_STRING) {
        result = 1;
    } else if (value->type == SCR_VALUE_INT) {
        value->type = SCR_VALUE_STRING;
        value->u.intValue = Scr_IntToStringId(value->u.intValue);
        result = 1;
    } else if (value->type == SCR_VALUE_FLOAT) {
        value->type = SCR_VALUE_STRING;
        value->u.intValue = ScrStr_InternFloat(value->u.floatValue);
        result = 1;
    } else if (value->type == SCR_VALUE_VECTOR) {
        value->type = SCR_VALUE_STRING;
        const float *vec = value->u.vectorValue;
        value->u.intValue = Scr_VectorToStringId(vec);
        Scr_VectorRelease((char *)(vec));
        result = 1;
    } else {
        g_scr_compile_error_message = va("cannot cast %s to string", Scr_TypeName(value->type));
        SCR_REMOVE_REF(value);
        value->type = SCR_VALUE_UNDEFINED;
        result = 0;
    }
    return result;
}

// 0x0807d420  size=220  FUN_0807d420  status=translated  naming=inferred
// Scr_ConvertToDisplayString: converts any value to a string value (type name for objects/others, animation name for type 11).
void Scr_ConvertToDisplayString(struct VariableValue *value)
{
    const char *text;
    switch (value->type) {
    default:
        text = Scr_TypeName(value->type);
        break;
    case SCR_VALUE_OBJECT: {
        int entryType = SCR_TYPE(g_scrVar[value->u.intValue].info);
        text = Scr_TypeName(entryType);
        break;
    }
    case SCR_VALUE_STRING:
    case SCR_VALUE_VECTOR:
    case SCR_VALUE_FLOAT:
    case SCR_VALUE_INT:
        Scr_CastString(value);
        return;
    case SCR_VALUE_ISTRING:
        value->type = SCR_VALUE_STRING;
        return;
    case SCR_VALUE_ANIMATION: {
        unsigned int packed = value->u.pointerValue;
        int tree = Anim_GetTreeSlot(packed >> 16);
        text = XAnim_NodeDisplayName((struct XAnimNodeTable *)(size_t)tree, packed & 0xffff);
        break;
    }
    }
    unsigned int stringId = ScrStr_InternCStringEx(text, 0, 0xe);
    SCR_REMOVE_REF(value);
    value->type = SCR_VALUE_STRING;
    value->u.pointerValue = stringId;
}

// 0x0807d4fc  size=38  FUN_0807d4fc  status=translated  naming=unknown
// Scr_GetVarClassChar: class letter of the class table entry indexed by the entry's name field.
int Scr_GetVarClassChar(int varId)
{
    return (int)(char)g_scrClassInfo[g_scrVar[varId].info >> 8].charId;
}

// 0x0807d522  size=18  FUN_0807d522  status=translated  naming=unknown
// Scr_GetArraySizeDup: byte-identical behaviour to Scr_GetArraySize (entry +6).
uint16 Scr_GetArraySizeDup(int varId)
{
    return SCRVAR_AUX(g_scrVar[varId]);
}

// 0x0807d534  size=57  FUN_0807d534  status=translated  naming=unknown
// Scr_ReleaseThreeValues: releases values [2],[1],[0] and clears the type of value [0].
void Scr_ReleaseThreeValues(struct VariableValue *values)
{
    for (int i = 2; i >= 0; i--)
        SCR_REMOVE_REF(&values[i]);
    values[0].type = SCR_VALUE_UNDEFINED;
}

// 0x0807d56e  size=219  FUN_0807d56e  status=translated  naming=inferred
// Scr_CastVector: string "type %s is not a float"; builds a vector from three stack values (values[2] becomes x).
void Scr_CastVector(struct VariableValue *values)
{
    float components[3];
    int i = 2;
    for (;;) {
        if (i < 0) {
            values[0].type = SCR_VALUE_VECTOR;
            values[0].u.vectorValue = (const float *)Scr_CloneVector((const unsigned int *)components);
            return;
        }
        int type = values[i].type;
        if (type == SCR_VALUE_FLOAT) {
            components[2 - i] = values[i].u.floatValue;
        } else {
            if (type != SCR_VALUE_INT) {
                scrVarPub_error_index = i + 1;
                Scr_ReleaseThreeValues(values);
                const char *msg = va("type %s is not a float", Scr_TypeName(type));
                Scr_Error(msg);
                return;
            }
            components[2 - i] = (float)values[i].u.intValue;
        }
        i--;
    }
}

// 0x0807d64a  size=145  FUN_0807d64a  status=translated  naming=inferred
// Scr_GetFieldObject: string "%s is not a field object"; resolves an object value below array type through ScrVar_SetVarValueReleasing.
int Scr_GetFieldObject(int arg, struct VariableValue *value)
{
    unsigned int type = value->type;
    if (type == SCR_VALUE_OBJECT && (type = SCR_TYPE(g_scrVar[value->u.intValue].info), type < 0x16)) {
        struct VariableValue local;
        local.type = SCR_VALUE_OBJECT;
        local.u.intValue = value->u.intValue;
        ScrVar_SetVarValueReleasing(arg, &local);
        return local.u.intValue;
    }
    SCR_REMOVE_REF(value);
    const char *msg = va("%s is not a field object", Scr_TypeName(type));
    Scr_Error(msg);
    return 0;
}

// 0x0807d6dc  size=205  FUN_0807d6dc  status=translated  naming=inferred
// Scr_UnmatchedPairError: string "pair '%s' and '%s' has unmatching types '%s' and '%s'"; releases both values and reports.
void Scr_UnmatchedPairError(struct VariableValue *a, struct VariableValue *b)
{
    const char *msg;
    if (g_scr_compile_error_message == 0) {
        int typeA = a->type;
        int typeB = b->type;
        Scr_ConvertToDisplayString(a);
        Scr_ConvertToDisplayString(b);
        const char *textB = ScrStr_TextFromHandle(b->u.intValue);
        const char *textA = ScrStr_TextFromHandle(a->u.intValue);
        msg = va("pair '%s' and '%s' has unmatching types '%s' and '%s'",
                          textA, textB, Scr_TypeName(typeA), Scr_TypeName(typeB));
    } else {
        msg = 0;
    }
    SCR_REMOVE_REF(a);
    a->type = SCR_VALUE_UNDEFINED;
    SCR_REMOVE_REF(b);
    b->type = SCR_VALUE_UNDEFINED;
    Scr_Error(msg);
}

// 0x0807d7aa  size=122  FUN_0807d7aa  status=translated  naming=inferred
// Scr_CastNumericPair: int/float mix promotes the int side to float; other mismatches report an error.
void Scr_CastNumericPair(struct VariableValue *a, struct VariableValue *b)
{
    int typeA = a->type;
    int typeB = b->type;
    if (typeA != typeB) {
        if (typeA == SCR_VALUE_FLOAT && typeB == SCR_VALUE_INT) {
            b->type = SCR_VALUE_FLOAT;
            b->u.floatValue = (float)b->u.intValue;
        } else if (typeA == SCR_VALUE_INT && typeB == SCR_VALUE_FLOAT) {
            a->type = SCR_VALUE_FLOAT;
            a->u.floatValue = (float)a->u.intValue;
        } else {
            Scr_UnmatchedPairError(a, b);
        }
    }
}

// 0x0807d824  size=482  FUN_0807d824  status=translated  naming=inferred
// Scr_CastStringPair: like Scr_CastNumericPair but a string on either side converts float/vector/int on the other side to string.
void Scr_CastStringPair(struct VariableValue *a, struct VariableValue *b)
{
    int typeA = a->type;
    int typeB = b->type;
    if (typeA == typeB)
        return;
    if (typeA < typeB) {
        if (typeA == SCR_VALUE_STRING) {
            if (typeB == SCR_VALUE_FLOAT) {
                b->type = SCR_VALUE_STRING;
                b->u.intValue = ScrStr_InternFloat(b->u.floatValue);
                return;
            }
            if (typeB < 6) {
                if (typeB == SCR_VALUE_VECTOR) {
                    b->type = SCR_VALUE_STRING;
                    const float *vec = b->u.vectorValue;
                    b->u.intValue = Scr_VectorToStringId(vec);
                    Scr_VectorRelease((char *)(vec));
                    return;
                }
            } else if (typeB == SCR_VALUE_INT) {
                b->type = SCR_VALUE_STRING;
                b->u.intValue = Scr_IntToStringId(b->u.intValue);
                return;
            }
        } else if (typeA != SCR_VALUE_FLOAT) {
            goto mismatch;
        }
        if (typeB == SCR_VALUE_INT) {
            b->type = SCR_VALUE_FLOAT;
            b->u.floatValue = (float)b->u.intValue;
            return;
        }
    } else {
        if (typeB == SCR_VALUE_STRING) {
            if (typeA == SCR_VALUE_FLOAT) {
                a->type = SCR_VALUE_STRING;
                a->u.intValue = ScrStr_InternFloat(a->u.floatValue);
                return;
            }
            if (typeA < 6) {
                if (typeA == SCR_VALUE_VECTOR) {
                    a->type = SCR_VALUE_STRING;
                    const float *vec = a->u.vectorValue;
                    a->u.intValue = Scr_VectorToStringId(vec);
                    Scr_VectorRelease((char *)(vec));
                    return;
                }
            } else if (typeA == SCR_VALUE_INT) {
                a->type = SCR_VALUE_STRING;
                a->u.intValue = Scr_IntToStringId(a->u.intValue);
                return;
            }
        } else if (typeB != SCR_VALUE_FLOAT) {
            goto mismatch;
        }
        if (typeA == SCR_VALUE_INT) {
            a->type = SCR_VALUE_FLOAT;
            a->u.floatValue = (float)a->u.intValue;
            return;
        }
    }
mismatch:
    Scr_UnmatchedPairError(a, b);
}

// 0x0807da06  size=61  FUN_0807da06  status=translated  naming=inferred
// Scr_EvalBitOr: opcode 0x66; ints only.
void Scr_EvalBitOr(struct VariableValue *a, struct VariableValue *b)
{
    if (a->type == SCR_VALUE_INT && b->type == SCR_VALUE_INT)
        a->u.pointerValue = b->u.pointerValue | a->u.pointerValue;
    else
        Scr_UnmatchedPairError(a, b);
}

// 0x0807da44  size=61  FUN_0807da44  status=translated  naming=inferred
// Scr_EvalBitXor: opcode 0x67; ints only.
void Scr_EvalBitXor(struct VariableValue *a, struct VariableValue *b)
{
    if (a->type == SCR_VALUE_INT && b->type == SCR_VALUE_INT)
        a->u.pointerValue = b->u.pointerValue ^ a->u.pointerValue;
    else
        Scr_UnmatchedPairError(a, b);
}

// 0x0807da82  size=61  FUN_0807da82  status=translated  naming=inferred
// Scr_EvalBitAnd: opcode 0x68; ints only.
void Scr_EvalBitAnd(struct VariableValue *a, struct VariableValue *b)
{
    if (a->type == SCR_VALUE_INT && b->type == SCR_VALUE_INT)
        a->u.pointerValue = b->u.pointerValue & a->u.pointerValue;
    else
        Scr_UnmatchedPairError(a, b);
}

// 0x0807dac0  size=613  FUN_0807dac0  status=translated  naming=inferred
// Scr_EvalEquals: opcode 0x69; result left in a as int. Float compares |a-b| < 1e-06f.
void Scr_EvalEquals(struct VariableValue *a, struct VariableValue *b)
{
    Scr_CastNumericPair(a, b);
    switch (a->type) {
    case SCR_VALUE_UNDEFINED:
        a->type = SCR_VALUE_INT;
        a->u.intValue = 1;
        break;
    case SCR_VALUE_OBJECT:
        if ((SCR_TYPE(g_scrVar[a->u.intValue].info) != SCR_VAR_ARRAY &&
             SCR_TYPE(g_scrVar[b->u.intValue].info) != SCR_VAR_ARRAY) || scr_compile_error_deferred != 0) {
            a->type = SCR_VALUE_INT;
            int idA = a->u.intValue;
            int idB = b->u.intValue;
            ScrVar_Release(a->u.intValue);
            ScrVar_Release(b->u.intValue);
            a->u.pointerValue = (idA == idB);
            return;
        }
        /* fall through: array objects are not comparable */
    default:
        Scr_UnmatchedPairError(a, b);
        break;
    case SCR_VALUE_STRING:
    case SCR_VALUE_ISTRING: {
        a->type = SCR_VALUE_INT;
        int idA = a->u.intValue;
        int idB = b->u.intValue;
        ScrStr_Release(a->u.intValue);
        ScrStr_Release(b->u.intValue);
        a->u.pointerValue = (idA == idB);
        break;
    }
    case SCR_VALUE_VECTOR: {
        a->type = SCR_VALUE_INT;
        int equal = 0;
        if (a->u.vectorValue[0] == b->u.vectorValue[0] &&
            a->u.vectorValue[1] == b->u.vectorValue[1] &&
            a->u.vectorValue[2] == b->u.vectorValue[2])
            equal = 1;
        Scr_VectorRelease((char *)(a->u.vectorValue));
        Scr_VectorRelease((char *)(b->u.vectorValue));
        a->u.intValue = equal;
        break;
    }
    case SCR_VALUE_FLOAT:
        a->type = SCR_VALUE_INT;
        a->u.pointerValue = (Q_fabs(a->u.floatValue - b->u.floatValue) < 1e-06f);
        break;
    case SCR_VALUE_INT:
        a->u.pointerValue = (a->u.intValue == b->u.intValue);
        break;
    case SCR_VALUE_FUNCTION:
        a->type = SCR_VALUE_INT;
        a->u.pointerValue = (a->u.intValue == b->u.intValue);
        break;
    case SCR_VALUE_ANIMATION:
        a->type = SCR_VALUE_INT;
        a->u.pointerValue = (a->u.intValue == b->u.intValue);
        break;
    }
}

// 0x0807dd26  size=43  FUN_0807dd26  status=translated  naming=inferred
// Scr_EvalNotEquals: opcode 0x6a; !Scr_EvalEquals.
void Scr_EvalNotEquals(struct VariableValue *a, struct VariableValue *b)
{
    Scr_EvalEquals(a, b);
    a->u.pointerValue = (a->u.pointerValue == 0);
}

// 0x0807dd52  size=130  FUN_0807dd52  status=translated  naming=inferred
// Scr_EvalLess: opcode 0x6b; a < b for float/int.
void Scr_EvalLess(struct VariableValue *a, struct VariableValue *b)
{
    Scr_CastNumericPair(a, b);
    if (a->type == SCR_VALUE_FLOAT) {
        a->type = SCR_VALUE_INT;
        a->u.pointerValue = (a->u.floatValue < b->u.floatValue);
    } else if (a->type == SCR_VALUE_INT) {
        a->u.pointerValue = (a->u.intValue < b->u.intValue);
    } else {
        Scr_UnmatchedPairError(a, b);
    }
}

// 0x0807ddd4  size=43  FUN_0807ddd4  status=translated  naming=inferred
// Scr_EvalGreaterEqual: opcode 0x6e; !(a < b).
void Scr_EvalGreaterEqual(struct VariableValue *a, struct VariableValue *b)
{
    Scr_EvalLess(a, b);
    a->u.pointerValue = (a->u.pointerValue == 0);
}

// 0x0807de00  size=130  FUN_0807de00  status=translated  naming=inferred
// Scr_EvalGreater: opcode 0x6c; b < a for float/int.
void Scr_EvalGreater(struct VariableValue *a, struct VariableValue *b)
{
    Scr_CastNumericPair(a, b);
    if (a->type == SCR_VALUE_FLOAT) {
        a->type = SCR_VALUE_INT;
        a->u.pointerValue = (b->u.floatValue < a->u.floatValue);
    } else if (a->type == SCR_VALUE_INT) {
        a->u.pointerValue = (b->u.intValue < a->u.intValue);
    } else {
        Scr_UnmatchedPairError(a, b);
    }
}

// 0x0807de82  size=43  FUN_0807de82  status=translated  naming=inferred
// Scr_EvalLessEqual: opcode 0x6d; !(a > b).
void Scr_EvalLessEqual(struct VariableValue *a, struct VariableValue *b)
{
    Scr_EvalGreater(a, b);
    a->u.pointerValue = (a->u.pointerValue == 0);
}

// 0x0807deae  size=69  FUN_0807deae  status=translated  naming=inferred
// Scr_EvalShiftLeft: opcode 0x6f; ints only, shift count masked to 5 bits.
void Scr_EvalShiftLeft(struct VariableValue *a, struct VariableValue *b)
{
    if (a->type == SCR_VALUE_INT && b->type == SCR_VALUE_INT)
        a->u.intValue = a->u.intValue << (b->u.intValue & 0x1f);
    else
        Scr_UnmatchedPairError(a, b);
}

// 0x0807def4  size=69  FUN_0807def4  status=translated  naming=inferred
// Scr_EvalShiftRight: opcode 0x70; ints only, arithmetic shift, count masked to 5 bits.
void Scr_EvalShiftRight(struct VariableValue *a, struct VariableValue *b)
{
    if (a->type == SCR_VALUE_INT && b->type == SCR_VALUE_INT)
        a->u.intValue = a->u.intValue >> (b->u.intValue & 0x1f);
    else
        Scr_UnmatchedPairError(a, b);
}

// 0x0807df3a  size=599  FUN_0807df3a  status=translated  naming=inferred
// Scr_EvalPlus: opcode 0x71; string "cannot concat \"%s\" and \"%s\" - max string length exceeded".
void Scr_EvalPlus(struct VariableValue *a, struct VariableValue *b)
{
    char buffer[8200];
    Scr_CastStringPair(a, b);
    int type = a->type;
    if (type == SCR_VALUE_VECTOR) {
        float *sum = (float *)Scr_AllocVectorBlock();
        sum[0] = a->u.vectorValue[0] + b->u.vectorValue[0];
        sum[1] = a->u.vectorValue[1] + b->u.vectorValue[1];
        sum[2] = a->u.vectorValue[2] + b->u.vectorValue[2];
        Scr_VectorRelease((char *)(a->u.vectorValue));
        Scr_VectorRelease((char *)(b->u.vectorValue));
        a->u.vectorValue = sum;
        return;
    }
    if (type < 5) {
        if (type == SCR_VALUE_STRING) {
            const char *textA = ScrStr_TextFromHandle(a->u.intValue);
            const char *textB = ScrStr_TextFromHandle(b->u.intValue);
            int lenA = ScrStr_NulIndexOfHandle(a->u.intValue);
            int lenB = ScrStr_NulIndexOfHandle(b->u.intValue);
            if (lenA < 0x2001) {
                strcpy(buffer, textA);
                strcpy(buffer + lenA, textB);
                int joined = ScrStr_Intern(buffer, 0, lenB + lenA + 1, 0xe);
                ScrStr_Release(a->u.intValue);
                ScrStr_Release(b->u.intValue);
                a->u.intValue = joined;
                return;
            }
            ScrStr_Release(a->u.intValue);
            ScrStr_Release(b->u.intValue);
            a->type = SCR_VALUE_UNDEFINED;
            b->type = SCR_VALUE_UNDEFINED;
            const char *msg = va("cannot concat \"%s\" and \"%s\" - max string length exceeded", textA, textB);
            Scr_Error(msg);
            return;
        }
    } else {
        if (type == SCR_VALUE_FLOAT) {
            a->u.floatValue = a->u.floatValue + b->u.floatValue;
            return;
        }
        if (type == SCR_VALUE_INT) {
            a->u.intValue = b->u.intValue + a->u.intValue;
            return;
        }
    }
    Scr_UnmatchedPairError(a, b);
}

// 0x0807e192  size=253  FUN_0807e192  status=translated  naming=inferred
// Scr_EvalMinus: opcode 0x72; float, vector, int.
void Scr_EvalMinus(struct VariableValue *a, struct VariableValue *b)
{
    Scr_CastNumericPair(a, b);
    int type = a->type;
    if (type == SCR_VALUE_FLOAT) {
        a->u.floatValue = a->u.floatValue - b->u.floatValue;
    } else {
        if (type < 6) {
            if (type == SCR_VALUE_VECTOR) {
                float *diff = (float *)Scr_AllocVectorBlock();
                diff[0] = a->u.vectorValue[0] - b->u.vectorValue[0];
                diff[1] = a->u.vectorValue[1] - b->u.vectorValue[1];
                diff[2] = a->u.vectorValue[2] - b->u.vectorValue[2];
                Scr_VectorRelease((char *)(a->u.vectorValue));
                Scr_VectorRelease((char *)(b->u.vectorValue));
                a->u.vectorValue = diff;
                return;
            }
        } else if (type == SCR_VALUE_INT) {
            a->u.intValue = a->u.intValue - b->u.intValue;
            return;
        }
        Scr_UnmatchedPairError(a, b);
    }
}

// 0x0807e290  size=102  FUN_0807e290  status=translated  naming=inferred
// Scr_EvalMultiply: opcode 0x73; float or int.
void Scr_EvalMultiply(struct VariableValue *a, struct VariableValue *b)
{
    Scr_CastNumericPair(a, b);
    if (a->type == SCR_VALUE_FLOAT)
        a->u.floatValue = a->u.floatValue * b->u.floatValue;
    else if (a->type == SCR_VALUE_INT)
        a->u.intValue = a->u.intValue * b->u.intValue;
    else
        Scr_UnmatchedPairError(a, b);
}

// 0x0807e2f6  size=189  FUN_0807e2f6  status=translated  naming=inferred
// Scr_EvalDivide: opcode 0x74; string "divide by 0". Int/int yields a float.
void Scr_EvalDivide(struct VariableValue *a, struct VariableValue *b)
{
    Scr_CastNumericPair(a, b);
    if (a->type == SCR_VALUE_FLOAT) {
        if (b->u.floatValue != 0.0f) {
            a->u.floatValue = a->u.floatValue / b->u.floatValue;
        } else {
            a->u.floatValue = 0.0f;
            Scr_Error("divide by 0");
        }
    } else if (a->type == SCR_VALUE_INT) {
        a->type = SCR_VALUE_FLOAT;
        if (b->u.intValue == 0) {
            a->u.intValue = 0;
            Scr_Error("divide by 0");
        } else {
            a->u.floatValue = (float)a->u.intValue / (float)b->u.intValue;
        }
    } else {
        Scr_UnmatchedPairError(a, b);
    }
}

// 0x0807e3b4  size=110  FUN_0807e3b4  status=translated  naming=inferred
// Scr_EvalModulo: opcode 0x75; ints only; string "divide by 0".
void Scr_EvalModulo(struct VariableValue *a, struct VariableValue *b)
{
    if (a->type == SCR_VALUE_INT && b->type == SCR_VALUE_INT) {
        if (b->u.intValue == 0) {
            a->u.intValue = 0;
            Scr_Error("divide by 0");
        } else {
            a->u.intValue = a->u.intValue % b->u.intValue;
        }
    } else {
        Scr_UnmatchedPairError(a, b);
    }
}

// 0x0807e422  size=384  FUN_0807e422  status=translated  naming=inferred
// Scr_EvalBinaryOperator: dispatches opcodes 0x66..0x75 (or, xor, and, ==, !=, <, >, <=, >=, <<, >>, +, -, *, /, %).
void Scr_EvalBinaryOperator(int opcode, struct VariableValue *a, struct VariableValue *b)
{
    switch (opcode) {
    case 0x66: Scr_EvalBitOr(a, b); break;
    case 0x67: Scr_EvalBitXor(a, b); break;
    case 0x68: Scr_EvalBitAnd(a, b); break;
    case 0x69: Scr_EvalEquals(a, b); break;
    case 0x6a: Scr_EvalNotEquals(a, b); break;
    case 0x6b: Scr_EvalLess(a, b); break;
    case 0x6c: Scr_EvalGreater(a, b); break;
    case 0x6d: Scr_EvalLessEqual(a, b); break;
    case 0x6e: Scr_EvalGreaterEqual(a, b); break;
    case 0x6f: Scr_EvalShiftLeft(a, b); break;
    case 0x70: Scr_EvalShiftRight(a, b); break;
    case 0x71: Scr_EvalPlus(a, b); break;
    case 0x72: Scr_EvalMinus(a, b); break;
    case 0x73: Scr_EvalMultiply(a, b); break;
    case 0x74: Scr_EvalDivide(a, b); break;
    case 0x75: Scr_EvalModulo(a, b); break;
    }
}

// 0x0807e5a2  size=42  FUN_0807e5a2  status=translated  naming=unknown
// Scr_IsVarUnreferenced: (flags & 0x60) == 0.
bool Scr_IsVarUnreferenced(int varId)
{
    return (g_scrVar[varId].info & 0x60) == 0;
}

// 0x0807e5cc  size=20  FUN_0807e5cc  status=translated  naming=inferred
// Scr_GetVarType: flags & 0x1f.
unsigned int Scr_GetVarType(int varId)
{
    return g_scrVar[varId].info & 0x1f;
}

// 0x0807e5e0  size=186  FUN_0807e5e0  status=translated  naming=unknown
// Scr_QueueEntityRelease: if class tracking is on, looks the name up in the class entity array, retags the target as type 0x14, links it on the scrVarPub_freeEntList chain and removes the lookup entry.
void Scr_QueueEntityRelease(unsigned int nameId, int classnum)
{
    if (scrVarPub_inited != 0) {
        uint16 entArrayId = g_scrClassInfo[classnum].entArrayId;
        int found = ScrVar_FindVarValueId24(entArrayId, nameId);
        if (found != 0) {
            int target = Scr_GetVarValue(found);
            g_scrVar[target].info = g_scrVar[target].info & 0xffffffe0u;
            g_scrVar[target].info = g_scrVar[target].info | 0x14;
            ScrVar_AddRef(target);
            SCRVAR_AUX(g_scrVar[target]) = (uint16)scrVarPub_freeEntList;
            scrVarPub_freeEntList = target;
            ScrVar_RemoveVar24(entArrayId, nameId);
        }
    }
}

// 0x0807e69a  size=124  FUN_0807e69a  status=translated  naming=unknown
// Scr_FlushEntityReleaseChain: pops the scrVarPub_freeEntList chain (links in entry +6), notifies each id and releases it.
void Scr_FlushEntityReleaseChain(void)
{
    unsigned int id;
    while (id = (unsigned int)scrVarPub_freeEntList, scrVarPub_freeEntList != 0) {
        int offset = scrVarPub_freeEntList * 0x10;
        (void)offset;
        scrVarPub_freeEntList = SCRVAR_AUX(g_scrVar[id]);
        SCRVAR_AUX(g_scrVar[id]) = 0;
        Scr_ClearObjectChains(id);
        if (g_scrVar[g_scrVar[id].prevSibling].hashId != id)
            ScrVar_FreeChildren(id);
        ScrVar_Release(id);
    }
}

// 0x0807e716  size=118  FUN_0807e716  status=translated  naming=unknown
// Scr_SweepEntityVars: for every id 1..0xfffd whose status bits are set and type is 0x13 or 0x14: Scr_ClearObjectChains then ScrVar_DestroyObject.
void Scr_SweepEntityVars(void)
{
    for (unsigned int id = 1; id < 0xfffe; id++) {
        unsigned int flags = g_scrVar[id].info;
        if ((flags & 0x60) != 0 && (SCR_TYPE(flags) == SCR_VAR_ENTITY_A || SCR_TYPE(flags) == SCR_VAR_ENTITY_B)) {
            Scr_ClearObjectChains(id);
            ScrVar_DestroyObject(id);
        }
    }
}

// 0x0807e78c  size=69  FUN_0807e78c  status=translated  naming=inferred
// Scr_InitClass: allocates the per-class entity array (entArrayId) first, then the field map (fieldMapId).
void Scr_InitClass(int classnum)
{
    g_scrClassInfo[classnum].entArrayId = (uint16)ScrVar_AllocArray();
    g_scrClassInfo[classnum].id = (uint16)ScrVar_AllocArray();
}

// 0x0807e7d2  size=115  FUN_0807e7d2  status=translated  naming=inferred
// Scr_ShutdownClass: if class tracking is on, releases and clears both class arrays.
void Scr_ShutdownClass(int classnum)
{
    if (scrVarPub_inited != 0) {
        ScrVar_Release(g_scrClassInfo[classnum].entArrayId);
        g_scrClassInfo[classnum].entArrayId = 0;
        ScrVar_Release(g_scrClassInfo[classnum].id);
        g_scrClassInfo[classnum].id = 0;
    }
}

// 0x0807e846  size=243  FUN_0807e846  status=translated  naming=inferred
// Scr_AddClassField: registers a field name (by id of the name and of its lowered string) -> int offset in the class field map.
void Scr_AddClassField(int classnum, const char *name, uint16 offset)
{
    uint16 mapId = g_scrClassInfo[classnum].id;
    unsigned int nameId = Scr_GetOrAssignSlot((int)(size_t)name);
    int entry = ScrVar_NewChildAVarId24(mapId, nameId);
    g_scrVar[entry].info = g_scrVar[entry].info & 0xffffffe0u;
    g_scrVar[entry].info = g_scrVar[entry].info | 6;
    g_scrVar[entry].u.pointerValue = (unsigned int)offset;
    int stringId = ScrStr_InternCStringEx(name, 0, 0xf);
    entry = ScrVar_NewChildAVarId(mapId, stringId);
    ScrStr_Release(stringId);
    g_scrVar[entry].info = g_scrVar[entry].info & 0xffffffe0u;
    g_scrVar[entry].info = g_scrVar[entry].info | 6;
    g_scrVar[entry].u.pointerValue = (unsigned int)offset;
}

// 0x0807e93a  size=93  FUN_0807e93a  status=translated  naming=inferred
// Scr_FindClassFieldOffset: looks a name up in the class field map; -1 when absent.
int Scr_FindClassFieldOffset(int classnum, const char *name)
{
    uint16 mapId = g_scrClassInfo[classnum].id;
    int nameId = ScrStr_HandleOfText(name);
    int entry = ScrVar_FindVarValueId(mapId, nameId);
    if (entry == 0)
        return -1;
    return g_scrVar[entry].u.intValue;
}

// 0x0807e998  size=96  FUN_0807e998  status=translated  naming=unknown
// Scr_FindClassEntityVar: value of the named entry in the class entity array, 0 if missing.
int Scr_FindClassEntityVar(unsigned int nameId, int classnum)
{
    int entry = ScrVar_FindVarValueId24(g_scrClassInfo[classnum].entArrayId, nameId);
    if (entry == 0)
        return 0;
    return g_scrVar[entry].u.intValue;
}

// 0x0807e9f8  size=148  FUN_0807e9f8  status=translated  naming=unknown
// Scr_GetOrCreateClassEntityVar: indexed entry in the class entity array; creates it through ScrVar_AllocEntity on first use.
int Scr_GetOrCreateClassEntityVar(unsigned int index, int classnum)
{
    int entry = ScrVar_GetOrNewVarValueId24(g_scrClassInfo[classnum].entArrayId, index);
    int result;
    if (SCR_TYPE(g_scrVar[entry].info) == SCR_VAR_FREE_SLOT) {
        result = ScrVar_AllocEntity(classnum, index & 0xffff);
        g_scrVar[entry].info = g_scrVar[entry].info | 1;
        g_scrVar[entry].u.intValue = result;
    } else {
        result = g_scrVar[entry].u.intValue;
    }
    return result;
}

// 0x0807ea8c  size=200  FUN_0807ea8c  status=translated  naming=inferred
// Scr_GetArrayVarForIndex: strings "array index %d out of range", "%s is not an array index"; creates the slot (int index or string key).
int Scr_GetArrayVarForIndex(int arrayId, struct VariableValue *index)
{
    int result;
    if (index->type == SCR_VALUE_INT) {
        char valid = Scr_IsSmallNameIndex(index->u.intValue);
        if (valid == 0) {
            const char *msg = va("array index %d out of range", index->u.intValue);
            Scr_Error(msg);
            result = 0;
        } else {
            result = ScrVar_GetOrNewVarValueId24(arrayId, index->u.intValue);
        }
    } else if (index->type == SCR_VALUE_STRING) {
        result = ScrVar_GetNewVarValueId(arrayId, index->u.intValue);
        ScrStr_Release(index->u.intValue);
    } else {
        const char *msg = va("%s is not an array index", Scr_TypeName(index->type));
        Scr_Error(msg);
        result = 0;
    }
    return result;
}

// 0x0807eb54  size=222  FUN_0807eb54  status=translated  naming=inferred
// Scr_FindArrayVarForIndex: lookup-only variant of Scr_GetArrayVarForIndex; releases arrayId on error.
int Scr_FindArrayVarForIndex(int arrayId, struct VariableValue *index)
{
    int result;
    if (index->type == SCR_VALUE_INT) {
        char valid = Scr_IsSmallNameIndex(index->u.intValue);
        if (valid == 0) {
            const char *msg = va("array index %d out of range", index->u.intValue);
            Scr_Error(msg);
            ScrVar_AddRef(arrayId);
            result = 0;
        } else {
            result = ScrVar_FindVarValueId24(arrayId, index->u.intValue);
        }
    } else if (index->type == SCR_VALUE_STRING) {
        result = ScrVar_FindVarValueId(arrayId, index->u.intValue);
        ScrStr_Release(index->u.intValue);
    } else {
        const char *msg = va("%s is not an array index", Scr_TypeName(index->type));
        Scr_Error(msg);
        ScrVar_AddRef(arrayId);
        result = 0;
    }
    return result;
}

// 0x0807ec32  size=629  FUN_0807ec32  status=translated  naming=inferred
// Scr_EvalArrayIndex: container a[index]; index value is overwritten with the result. Handles strings, arrays and vectors.
void Scr_EvalArrayIndex(struct VariableValue *container, struct VariableValue *index)
{
    int ctype = container->type;
    if (ctype == SCR_VALUE_STRING) {
        if (index->type == SCR_VALUE_INT) {
            if (-1 < index->u.intValue) {
                const char *text = ScrStr_TextFromHandle(container->u.intValue);
                size_t length = strlen(text);
                if (index->u.pointerValue < length) {
                    char one[2];
                    index->type = SCR_VALUE_STRING;
                    one[0] = text[index->u.pointerValue];
                    one[1] = 0;
                    index->u.pointerValue = ScrStr_Intern(one, 0, 2, 0xe);
                    ScrStr_Release(container->u.intValue);
                    return;
                }
            }
            const char *msg = va("string index %d out of range", index->u.intValue);
            Scr_Error(msg);
        } else {
            const char *msg = va("%s is not a string index", Scr_TypeName(index->type));
            Scr_Error(msg);
        }
    } else {
        if (ctype < 3) {
            if (ctype == SCR_VALUE_OBJECT) {
                int varId = container->u.intValue;
                if (SCR_TYPE(g_scrVar[varId].info) == SCR_VAR_ARRAY) {
                    int element = Scr_FindArrayVarForIndex(container->u.intValue, index);
                    struct VariableValue result;
                    ScrVar_ReadVarValue(&result, element);
                    index->u.intValue = result.u.intValue;
                    index->type = result.type;
                    ScrVar_Release(container->u.intValue);
                    return;
                }
                scrVarPub_error_index = 1;
                const char *msg = va("%s is not an array", Scr_TypeName(SCR_TYPE(g_scrVar[varId].info)));
                Scr_Error(msg);
                return;
            }
        } else if (ctype == SCR_VALUE_VECTOR) {
            if (index->type != SCR_VALUE_INT) {
                const char *msg = va("%s is not a vector index", Scr_TypeName(index->type));
                Scr_Error(msg);
                return;
            }
            if (index->u.pointerValue < 3) {
                index->type = SCR_VALUE_FLOAT;
                index->u.floatValue = container->u.vectorValue[index->u.pointerValue];
                Scr_VectorRelease((char *)(container->u.vectorValue));
                return;
            }
            const char *msg = va("vector index %d out of range", index->u.intValue);
            Scr_Error(msg);
            return;
        }
        scrVarPub_error_index = 1;
        const char *msg = va("%s is not an array, string, or vector", Scr_TypeName(container->type));
        Scr_Error(msg);
    }
}

// 0x0807eea8  size=681  FUN_0807eea8  status=translated  naming=inferred
// Scr_GetArrayForWrite: resolves the array variable to modify (varId 0xfffe means the current field scrVarPub_pendingFieldObjectId/scrVarPub_pendingFieldNameId); strings "read-only array cannot be changed", "string characters cannot be individually changed", "vector components cannot be individually changed", "%s is not an array". Shared arrays (refCount != 0) are replaced by a fresh copy.
// Quirk kept from the binary: on the 0xfffe path with a non-pointer-resolved array the entry pointer stays NULL and a shared array would store through NULL.
int Scr_GetArrayForWrite(int varId)
{
    struct ScrVarEntry *entry;
    int kind;
    int target;
    struct VariableValue resolved;

    if (varId == 0xfffe) {
        struct ScrVarEntry *owner = &g_scrVar[scrVarPub_pendingFieldObjectId];
        int found = ScrVar_FindVarValueId24(g_scrClassInfo[owner->info >> 8].id, scrVarPub_pendingFieldNameId);
        if (found != 0) {
            Scr_RunSetup(&resolved, owner->info >> 8, SCRVAR_AUX(*owner), g_scrVar[found].u.intValue);
            target = resolved.u.intValue;
            kind = resolved.type;
            if (kind != 0) {
                if (kind == SCR_VALUE_OBJECT && SCRVAR_RC(g_scrVar[target]) == 0) {
                    SCR_REMOVE_REF(&resolved);
                    scrVarPub_error_index = 1;
                    Scr_Error("read-only array cannot be changed");
                    return 0;
                }
                SCR_REMOVE_REF(&resolved);
                entry = 0;
                goto have_target;
            }
        }
        entry = &g_scrVar[ScrVar_NewChildAVarId(scrVarPub_pendingFieldObjectId, scrVarPub_pendingFieldNameId)];
    } else {
        entry = &g_scrVar[varId];
        kind = SCR_TYPE(entry->info);
        if (kind != 0) {
            target = entry->u.intValue;
            goto have_target;
        }
    }
    /* create a fresh array in the empty slot */
    entry->info = entry->info | 1;
    entry->u.intValue = ScrVar_AllocArray();
    return entry->u.intValue;

have_target:
    if (kind == SCR_VALUE_OBJECT) {
        struct ScrVarEntry *array = &g_scrVar[target];
        if (SCR_TYPE(array->info) != SCR_VAR_ARRAY) {
            scrVarPub_error_index = 1;
            const char *msg = va("%s is not an array", Scr_TypeName(SCR_TYPE(array->info)));
            Scr_Error(msg);
            return 0;
        }
        if (SCRVAR_RC(*array) != 0) {
            int oldArray = target;
            ScrVar_Release(oldArray);
            target = ScrVar_AllocArray();
            ScrVar_CopyChildren(oldArray, target);
            entry->u.intValue = target;
        }
        return target;
    }
    scrVarPub_error_index = 1;
    if (kind == SCR_VALUE_STRING) {
        Scr_Error("string characters cannot be individually changed");
        return 0;
    }
    if (kind == SCR_VALUE_VECTOR) {
        Scr_Error("vector components cannot be individually changed");
        return 0;
    }
    {
        const char *msg = va("%s is not an array", Scr_TypeName(kind));
        Scr_Error(msg);
    }
    return 0;
}

// 0x0807f152  size=666  FUN_0807f152  status=translated  naming=inferred
// Scr_SetArrayElement: writes into the array resolved like Scr_GetArrayForWrite, element chosen by index (int or string key); strings "array index %d out of range", "%s is not an array index", "%s is not an array".
// Same NULL-entry quirk as Scr_GetArrayForWrite on the 0xfffe path.
void Scr_SetArrayElement(int varId, struct VariableValue *index)
{
    struct ScrVarEntry *entry;
    unsigned int kind;
    int target;
    struct VariableValue resolved;

    if (varId == 0xfffe) {
        struct ScrVarEntry *owner = &g_scrVar[scrVarPub_pendingFieldObjectId];
        int found = ScrVar_FindVarValueId24(g_scrClassInfo[owner->info >> 8].id, scrVarPub_pendingFieldNameId);
        if (found != 0) {
            Scr_RunSetup(&resolved, owner->info >> 8, SCRVAR_AUX(*owner), g_scrVar[found].u.intValue);
            target = resolved.u.intValue;
            kind = resolved.type;
            if (kind != 0) {
                if (kind == SCR_VALUE_OBJECT && SCRVAR_RC(g_scrVar[target]) == 0) {
                    SCR_REMOVE_REF(&resolved);
                    scrVarPub_error_index = 1;
                    Scr_Error("read-only array cannot be changed");
                    return;
                }
                SCR_REMOVE_REF(&resolved);
                entry = 0;
                goto have_target;
            }
        }
        kind = 0;
    } else {
        entry = &g_scrVar[varId];
        kind = SCR_TYPE(entry->info);
        target = entry->u.intValue;
        goto have_target;
    }
    goto not_array;

have_target:
    if (kind == SCR_VALUE_OBJECT) {
        struct ScrVarEntry *array = &g_scrVar[target];
        if (SCR_TYPE(array->info) != SCR_VAR_ARRAY) {
            scrVarPub_error_index = 1;
            const char *msg = va("%s is not an array", Scr_TypeName(SCR_TYPE(array->info)));
            Scr_Error(msg);
            return;
        }
        if (SCRVAR_RC(*array) != 0) {
            int oldArray = target;
            ScrVar_Release(oldArray);
            target = ScrVar_AllocArray();
            ScrVar_CopyChildren(oldArray, target);
            entry->u.intValue = target;
        }
        if (index->type == SCR_VALUE_INT) {
            char valid = Scr_IsSmallNameIndex(index->u.intValue);
            if (valid == 0) {
                const char *msg = va("array index %d out of range", index->u.intValue);
                Scr_Error(msg);
                return;
            }
            ScrVar_RemoveVarIfPresent24(target, index->u.intValue);
            return;
        }
        if (index->type == SCR_VALUE_STRING) {
            ScrStr_Release(index->u.intValue);
            ScrVar_RemoveVarIfPresent(target, index->u.intValue);
            return;
        }
        const char *msg = va("%s is not an array index", Scr_TypeName(index->type));
        Scr_Error(msg);
        return;
    }
not_array:
    scrVarPub_error_index = 1;
    {
        const char *msg = va("%s is not an array", Scr_TypeName(kind));
        Scr_Error(msg);
    }
}

// 0x0807f3ec  size=41  FUN_0807f3ec  status=translated  naming=unknown
// Scr_AddNewArrayValue: allocates an array and passes it as an object value to ScrVar_SetVarValueReleasing.
void Scr_AddNewArrayValue(int arg)
{
    struct VariableValue value;
    value.type = SCR_VALUE_OBJECT;
    value.u.intValue = ScrVar_AllocArray();
    ScrVar_SetVarValueReleasing(arg, &value);
}

// 0x0807f416  size=56  FUN_0807f416  status=translated  naming=unknown
// Scr_GetVarInfoPair: returns {entry +6, flags>>8 as u16} through a hidden result pointer.
struct ScrVarInfoPair Scr_GetVarInfoPair(int varId)
{
    struct ScrVarInfoPair result;
    result.field_6 = SCRVAR_AUX(g_scrVar[varId]);
    result.nameId = (uint16)(g_scrVar[varId].info >> 8);
    return result;
}

// 0x0807f44e  size=210  FUN_0807f44e  status=translated  naming=unknown
// Scr_CopyChildren: copies every child (except name id 0x1fffe) of srcId into dstId, merging type bits and taking a reference on the value.
void Scr_CopyChildren(int srcId, int dstId)
{
    for (int child = Scr_GetNextSibling(srcId); child != 0; child = Scr_GetNextSibling(child)) {
        if (g_scrVar[child].info >> 8 != 0x1fffe) {
            int dest = ScrVar_GetNewVarValueId(dstId, g_scrVar[child].info >> 8);
            unsigned int srcFlags = g_scrVar[child].info;
            g_scrVar[dest].info = (srcFlags & 0x1f) | g_scrVar[dest].info;
            g_scrVar[dest].u.intValue = g_scrVar[child].u.intValue;
            ScrVar_AddRefValue(srcFlags & 0x1f, g_scrVar[dest].u.intValue);
        }
    }
}

// 0x0807f520  size=85  FUN_0807f520  status=translated  naming=unknown
// Scr_CopyClassEntityChildren: if the source class entity has children, copies them into the destination class entity.
void Scr_CopyClassEntityChildren(unsigned int srcIndex, unsigned int dstIndex, int classnum)
{
    int src = Scr_FindClassEntityVar(srcIndex, classnum);
    if (src != 0 && Scr_GetNextSibling(src) != 0) {
        int dst = Scr_GetOrCreateClassEntityVar(dstIndex, classnum);
        Scr_CopyChildren(src, dst);
    }
}

// 0x0807f576  size=95  FUN_0807f576  status=translated  naming=unknown
// Scr_GetValueWeight: weight of a value: for a type-1 value pointing at an array, weight(array) / (1 + refCount); otherwise 0.
float Scr_GetValueWeight(int type, int varId)
{
    float weight;
    if (type == 1 && SCR_TYPE(g_scrVar[varId].info) == SCR_VAR_ARRAY) {
        float total = Scr_GetVarWeight(varId);
        weight = total / (1.0f + (float)SCRVAR_RC(g_scrVar[varId]));
    } else {
        weight = 0.0f;
    }
    return weight;
}

// 0x0807f5d6  size=41  FUN_0807f5d6  status=translated  naming=unknown
// Scr_GetEntryWeight: 1 + weight of the entry's typed value.
float Scr_GetEntryWeight(struct ScrVarEntry *entry)
{
    float weight = Scr_GetValueWeight(entry->info & 0x1f, entry->u.intValue);
    return 1.0f + weight;
}

// 0x0807f600  size=87  FUN_0807f600  status=translated  naming=unknown
// Scr_GetNamedRootWeight: weight of the value found in scrVarPub_notifyArrayId under the given name, 0 when absent.
float Scr_GetNamedRootWeight(int nameId)
{
    float result;
    int found = ScrVar_FindVarValueIdBias(scrVarPub_notifyArrayId, nameId);
    if (found == 0) {
        result = 0.0f;
    } else {
        int varId = Scr_GetVarValue(found);
        result = Scr_GetVarWeight(varId);
    }
    return result;
}

// 0x0807f658  size=108  FUN_0807f658  status=translated  naming=unknown
// Scr_GetVarWeight: 1.0 plus the entry weight of every child of varId.
float Scr_GetVarWeight(int varId)
{
    float total = 1.0f;
    for (int child = Scr_GetNextSibling(varId); child != 0; child = Scr_GetNextSibling(child))
        total = total + Scr_GetEntryWeight(&g_scrVar[child]);
    return total;
}

// 0x0807f6c4  size=246  FUN_0807f6c4  status=translated  naming=unknown
// Scr_GetFrameWeight: accumulates variable weights across a frame's records walked from the last to the first; kind 7 records advance the variable id through ScrVar_GetInfoHighB. The second weight sum is stored through outRootWeight.
float Scr_GetFrameWeight(struct ScrStackBuffer_s *frame, float *outRootWeight)
{
    unsigned int remaining = frame->size;
    /* Records (5 bytes each, packed) follow the 0xb-byte header. */
    struct ScrStackRecord_s *record = (struct ScrStackRecord_s *)((byte *)frame + 0xb) + remaining;
    float total = Scr_GetVarWeight(frame->localId);
    *outRootWeight = Scr_GetNamedRootWeight(frame->localId);
    unsigned int varId = frame->localId;
    while (remaining != 0) {
        record--;
        remaining--;
        if (record->type == 7) {
            varId = ScrVar_GetInfoHighB(varId);
            total = total + Scr_GetVarWeight(varId);
            *outRootWeight = *outRootWeight + Scr_GetNamedRootWeight(varId);
        } else {
            total = total + Scr_GetValueWeight(record->type, record->value);
        }
    }
    return total;
}

// 0x0807f7ba  size=134  FUN_0807f7ba  status=translated  naming=inferred
// Scr_FindFieldDef: case-insensitive search of the field table (records: name, NUL, u16 id, s8 type); 0 when absent.
uint16 Scr_FindFieldDef(const char *name, int *outType)
{
    char *record = g_scr_field_table;
    size_t length;
    for (;;) {
        if (*record == '\0')
            return 0;
        length = strlen(record);
        if (strcasecmp(name, record) == 0)
            break;
        record = record + length + 4;
    }
    uint16 id = *(uint16 *)(record + length + 1);
    *outType = (int)(record + length + 1)[2];
    return id;
}

// 0x0807f840  size=652  FUN_0807f840  status=translated  naming=inferred
// Scr_AddFieldsFromFile: parses a field definition file ("type name" pairs, types float/int/string/vector) and appends field records to the table; parse session label "Scr_AddFields"; errors via Com_Error(1, ...).
void Scr_AddFieldsFromFile(const char *fileName)
{
    int handle;
    int length = FSH_FOpenFile(fileName, &handle, 0);
    if (length < 0) {
        const char *msg = va("\025cannot find '%s'", fileName);
        Com_Error(1, msg);
    }
    char *text = (char *)Hunk_AllocateTempMemoryHigh(length + 1);
    FS_Read(text, length, handle);
    text[length] = 0;
    FS_FCloseFile(handle);
    const char *cursor = text;
    Com_BeginParseSession("Scr_AddFields");
    for (;;) {
        const char *token = Com_Parse(&cursor);
        if (cursor == 0) {
            Com_EndParseSession();
            Hunk_ClearTempMemoryHigh();
            return;
        }
        int fieldType;
        if (strcmp(token, "float") == 0) {
            fieldType = SCR_VALUE_FLOAT;
        } else if (strcmp(token, "int") == 0) {
            fieldType = SCR_VALUE_INT;
        } else if (strcmp(token, "string") == 0) {
            fieldType = SCR_VALUE_STRING;
        } else {
            if (strcmp(token, "vector") != 0) {
                const char *msg = va("\025unknown type '%s' in '%s'", token, fileName);
                Com_Error(1, msg);
                return;
            }
            fieldType = SCR_VALUE_VECTOR;
        }
        char *fieldName = Com_Parse(&cursor);
        if (cursor == 0) {
            const char *msg = va("\025missing field name in '%s'", fileName);
            Com_Error(1, msg);
        }
        size_t nameLength = strlen(fieldName);
        length = nameLength + 1;
        for (; -1 < (int)nameLength; nameLength = nameLength - 1) {
            int lowered = tolower((int)fieldName[nameLength]);
            fieldName[nameLength] = (char)lowered;
        }
        unsigned int nameId = Scr_GetOrAssignSlot((int)(size_t)fieldName);
        int existingType;
        int existing = Scr_FindFieldDef(fieldName, &existingType);
        if (existing != 0)
            Com_Error(1, "\025duplicate key '%s' in '%s'", fieldName, fileName);
        int recordSize = length + 3;
        char *record = (char *)(size_t)TempMemory_Reserve(recordSize);
        strcpy(record, fieldName);
        record = record + length;
        *(int16 *)record = (int16)nameId;
        record[2] = (char)fieldType;
        record = record + 3;
        *record = '\0';
    }
}

// 0x0807facc  size=217  FUN_0807facc  status=translated  naming=inferred
// Scr_LoadFieldFiles: lists the files of a directory, rebuilds the field table (empty NUL-terminated record list) and loads every file with Scr_AddFieldsFromFile; format "%s/%s".
void Scr_LoadFieldFiles(const char *directory, const char *extension)
{
    int count[3];
    char path[68];
    char **fileList = FS_ListFiles(directory, extension, 0, count, 10);
    TempMemory_Reset();
    g_scr_field_table = (char *)Hunk_AllocLow(0);
    *g_scr_field_table = 0;
    for (int i = 0; i < count[0]; i++) {
        sprintf(path, "%s/%s", directory, fileList[i]);
        Scr_AddFieldsFromFile(path);
    }
    if (fileList != 0)
        FS_FreeFileList(fileList, 10);
    char *terminator = (char *)(size_t)TempMemory_Reserve(1);
    *terminator = 0;
    Hunk_CommitLowTemp();
}

// 0x0807fba6  size=19  FUN_0807fba6  status=translated  naming=unknown
// Scr_ReleaseVarThunk: tail wrapper around ScrVar_Release.
void Scr_ReleaseVarThunk(int varId)
{
    ScrVar_Release(varId);
}

// 0x0807fbba  size=227  FUN_0807fbba  status=translated  naming=unknown
// Scr_PruneArrayChildren: returns 0 if varId is not an array; otherwise removes children that are types 7..11 or pointers whose own prune returns 0, restarting the scan after each removal; returns 1.
int Scr_PruneArrayChildren(int varId)
{
    if (SCR_TYPE(g_scrVar[varId].info) != SCR_VAR_ARRAY)
        return 0;
restart:
    int child = Scr_GetNextSibling(varId);
    for (;;) {
        if (child == 0)
            return 1;
        unsigned int flags = g_scrVar[child].info;
        switch (SCR_TYPE(flags)) {
        case 1: {
            int ok = Scr_PruneArrayChildren(g_scrVar[child].u.intValue);
            if (ok == 0) {
                ScrVar_RemoveVar(varId, flags >> 8);
                goto restart;
            }
        }
            /* fall through */
        default:
            child = Scr_GetNextSibling(child);
            break;
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
            ScrVar_RemoveVar(varId, flags >> 8);
            goto restart;
        }
    }
}

// 0x0807fc9e  size=42  FUN_0807fc9e  status=translated  naming=unknown
// Scr_InitPersistentArray: first call allocates scrVarPub_gameId and registers it.
void Scr_InitPersistentArray(void)
{
    if (scrVarPub_gameId == 0) {
        scrVarPub_gameId = ScrVar_AllocValue();
        Scr_AddNewArrayValue(scrVarPub_gameId);
    }
}

// 0x0807fcc8  size=73  FUN_0807fcc8  status=translated  naming=unknown
// Scr_ShutdownPersistentArray: flag 0 prunes the array children, otherwise releases the variable and clears scrVarPub_gameId.
void Scr_ShutdownPersistentArray(int release)
{
    if (release == 0) {
        Scr_PruneArrayChildren(g_scrVar[scrVarPub_gameId].u.intValue);
    } else {
        ScrVar_ReleaseAndFree(scrVarPub_gameId);
        scrVarPub_gameId = 0;
    }
}

// 0x0807fd12  size=49  FUN_0807fd12  status=translated  naming=unknown
// Scr_GetStatSnapshot: fills {g_scr_code_hash, g_scr_code_length, scrGlobalWindowEnd - g_scr_code_base}.
void Scr_GetStatSnapshot(struct ScrStatSnapshot *out)
{
    out->value0 = g_scr_code_hash;
    out->value1 = g_scr_code_length;
    out->delta = scrGlobalWindowEnd - g_scr_code_base;
}
