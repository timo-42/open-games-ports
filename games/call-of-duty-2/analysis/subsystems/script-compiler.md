# Subsystem: GSC script compiler (cod2_lnxded 1.3)

Target: `cod2_lnxded` 1.3, SHA-256 `05e774a3ee9fb487d4ec8a3d7d8d8e667f8ae459653e3c9e60a1c55604d5a3a7`.
Scope: GSC lexer (flex), parser (Bison LALR(1) `yyparse`), AST builders, the two-pass bytecode compiler,
script file loading (`maps/.../*.gsc` through the filesystem, i.e. iwd/pak search), include and
cross-file function linking, `#using_animtree` handling, compile-time debug/source-position tables, compile error reporting.
Out of scope but referenced: the VM (`VM_ExecuteInternal`, chunk 16), the variable/string system (chunks 13-15), builtin tables (chunk 50).

Evidence tags used on every claim: **[O]** observed (directly in code, bytes, strings or tables), **[I]** inferred (consistent
with several observations, not proven), **[U]** unknown. Addresses are virtual addresses in the 32-bit ELF.
Method: chunk sources `recon/chunks/chunk_010..013, 018, 019` (plus 014, 015, 016, 017, 040, 049, 050 for callers/callees), symbols
`analysis/symbols/symbols.json`, and the binary itself. Two facts below were derived from the binary rather than from the
chunk text: (a) the flex DFA was re-run in a small Python emulator over the tables at `0x08147cc0..0x08148be0` to recover the
token vocabulary; (b) bytes at `0x08144450..0x08144460` were read to identify the synthetic lexer prefix. The emulator is a
scratch artifact and is not committed; its results are tabulated in section 4.2.

Function coverage: 323 symbols tagged `script/compiler*`, `script/lexer`, `script/parser`, `script/source`, `script/symbols`,
`script/init`, `script/debug-tables`, `script/values` (AST builders), `script/errors`, `server/script_files`, `server/script_buffer`.
Tag noise found: see section 12 (several untagged or mis-tagged helpers are included below).

## 1. Purpose and architecture (as implemented in this binary)

The dedicated server compiles every GSC source file it needs **at map load** (inside `G_InitGame`, only when it is not a fast
restart) into a flat in-memory bytecode image. No bytecode is read from disk and no bytecode is written to disk: **[O]** the only
file inputs are `*.gsc` text (via `ScrDbg_LoadFrameFile` -> `FSH_FOpenFile`/`FS_Read`) and `animtrees/*.atr`.
The pipeline for one file is:

1. `Scr_LoadScriptFile(name)` (0x08076296) normalises the name, checks the loaded-scripts table, otherwise reads `<name>.gsc`.
2. `Lex_RunParser` (0x0808ba2b) installs a flex buffer fed by `Scr_ReadSourceLine` (0x08076200) and runs `yyparse` (0x08085932).
   Reductions build a heap AST out of variable-length word boxes (`ScrBox_Make1..9`) and lists (`ScrList_*`). Result: a 2-word box
   `{includeList, topLevelList}` stored in the parse-result global `0x083deec0`.
3. `Scr_CompileScriptUnit` (0x08075a26) registers `#include`s, compiles the top-level list, links forward references and loads dependencies
   recursively.
4. Per function (`kind 0x44` node), the compiler does a **scan pass** (`Scr_ScanStmt` family: control-flow state and local variable
   discovery, mutates the AST by storing scope-block pointers into reserved operand slots) and a **code-generation pass**
   (`Scr_CompileStmt` family, `ScrCompile_*`, `Emit*`), emitting bytes into one growing code buffer.
5. After the whole load the game calls `Scr_ReleasePools` and `Scr_ShutdownCompileState`; the bytecode image stays resident
   (low hunk) and the VM runs it by absolute pointers.

Lineage: the structure (EmitOpcode peephole, `/# ... #/` developer blocks, `#using_animtree`, canonical strings, `VAR_DEVELOPER_CODEPOS`,
`CodeCallback_*` entry points) matches the Infinity Ward script system later seen in CoD4. Names below follow that lineage only where this
binary's behaviour agrees. **[I]**

## 2. Entry points and call flow

| Step | Function (address) | Notes |
| --- | --- | --- |
| level start | `G_InitGame` 0x08109096 (chunk 49) | calls `GScr_LoadScripts` only when `isRestart == 0` **[O]** (chunk_049.cpp:156). `G_ShutdownGame` 0x081094e8 -> `Scr_ResetAll` 0x080765b6 when `freeLevel`. |
| load driver | `GScr_LoadScripts` 0x0811041e | `Scr_InitCompileState` (0x080760fc) -> `GScr_GetFunctionOffset("codescripts/delete","main")`, `("codescripts/struct","initstructs")`, `("codescripts/struct","createstruct")` -> `GScr_LoadGametypeScript` (`maps/mp/gametypes/<g_gametype>` `main` + 5 `CodeCallback_*` in `maps/mp/gametypes/_callbacksetup`) -> `GScr_LoadLevelScript` (`maps/mp/<mapname>` `main`, optional) -> `Scr_ReleasePools` -> `GScr_InitClasses` -> `Scr_ShutdownCompileState` **[O]** (chunk_050.cpp:786..800). |
| name resolve | `GScr_GetFunctionOffset` 0x08110208 | `Scr_LoadScriptFile(script)` then `Scr_ResolveFunctionOffset(script,label)` 0x08075eb6; fatal `Com_Error(1,"\x15Could not find script '%s'")` / `"...label '%s' in script '%s'"` when required. |
| file load | `Scr_LoadScriptFile` 0x08076296 | idempotent per normalised name (table `0x08205c88`). |
| source read | `ScrDbg_LoadFrameFileOrSaved` 0x08077fde -> `ScrDbg_LoadFrameFile` 0x08077efc | `FSH_FOpenFile(name,&h,0)`, temp-hunk buffer `len+1`, registers a debug "frame" (`ScrDbg_AddFrame` 0x08077d90). |
| lex+parse | `Lex_RunParser` 0x0808ba2b -> `yyparse` 0x08085932 -> `yylex` 0x0808948b | input via `yy_get_next_buffer` 0x0808a925 -> `Scr_ReadSourceLine`. |
| compile unit | `Scr_CompileScriptUnit` 0x08075a26 | `Scr_RegisterIncludeList` 0x080759ee, `Scr_CompileTopLevelList` 0x08075902, dependency loop, `ScrCompile_ResolveAllFunctionRefs` 0x080710f0. |
| top level | `Scr_CompileTopLevelItem` 0x080757fc | kinds 0x44 function, 0x45 `/#`, 0x46 `#/`, 0x47 `#using_animtree`. |
| function | `Scr_CreateFunctionContext` 0x08075520 (scan pass), `Scr_CompileFunctionNode` 0x08075662 / `Scr_CompileFunctionNodeDev` 0x080756e4, `Scr_CompileFunctionBody` 0x08075436 | |
| statements | `Scr_CompileStmt` 0x0807491a (switch on node kind), `Scr_ScanStmt` 0x08074e8a | |
| expressions | `ScrCompile_Expr` 0x08072020, `ScrCompile_PrimaryExpr` 0x080719b8, `ScrCompile_BinaryExpr` 0x08071de4 | constant folding via `Scr_EvalBinaryOperator` 0x0807e422 (chunk 15). |
| calls | `ScrCompile_FunctionCall` 0x08070b42, `ScrCompile_MethodCall` 0x08070d66, `EmitFunctionRef` 0x08070124 | |
| emit sink | `EmitOpcode` 0x0806e120 | single sink for every opcode byte. |
| anim trees | `Scr_UsingAnimTree` 0x0806dba2, `Scr_RegisterAnimTree` 0x0806da84, `Scr_LoadAnimTree` 0x0806dcb0, `Scr_LoadAnimTreeFile` 0x0806dbee, `Scr_AddAnimTreeEntry` 0x0806dfca, `Scr_GetAnimTreeValue` 0x0806df16 | MP player anims: `Player_LoadMultiplayerAnims` (chunk 40) calls `Scr_ForEachTableSlot` 0x08076510 and `Scr_ShutdownTableSlot` 0x08076550. |
| errors | `ScrCompile_ErrorInFile` 0x0807863e, `ScrCompile_ErrorAt` 0x0807873e | both end in `Com_Error(5, "\x15script compile error\n(see console for details)")`. |

Fast restart (`map_restart`) does **not** recompile: **[O]** compile is skipped when `isRestart != 0` (chunk_049.cpp:155).

## 3. Source loading and the synthetic "+" prefix

* `Scr_LoadScriptFile` builds the path with `Com_sprintf(path, 0x40, "%s.gsc", name)`: the buffer is 64 bytes, so a normalised name is limited to 57 characters before `.gsc` **[O]**; the overflow behaviour of `Com_sprintf` was not checked **[U]**.
  `Scr_NormalizeFilename` 0x0807a1d4 lowercases, converts `\` to `/`, collapses leading and repeated separators, and calls
  `Com_Error(1,"\x15Filename '%s' exceeds maximum length of %d")` at 0x400 bytes **[O]**. `Scr_NormalizeFilenameToStringId` 0x0807a27a interns the result with
  `ScrStr_InternCStringEx(buf,0,7)`.
* The loaded-scripts table `g_08205c88` is a script-variable array object keyed by the normalised name id **[O]**; the entry is created
  **before** parsing (so a recursive `#include` cycle finds the entry and does not loop; the returned symbol-table object may still be
  incomplete in that case) **[I]**. After `Scr_CompileScriptUnit` finishes the entry is set to an integer (type 6) value **[O]**
  (`finish_args[1] = 6`).
* Per-file function tables live in a second array `g_08205c8c`, keyed by the same name id: `ScrVar_GetNewVarValueId(g_08205c8c,sym)`
  then `Scr_GetOrCreateObjectId` gives the file's **symbol-table object** (`symtab`).
* Lexer input: `g_08205c9c` (read cursor) is initialised to `0x0814445f`, `g_08205ca0` (saved cursor) to the loaded text **[O]**
  (0x080763d3, 0x080763e0). The bytes at `0x0814445f` are `"+\0"` (0x2b 0x00) **[O]** (read from the file). `Scr_ReadSourceLine`
  (0x08076200) copies up to `max` bytes up to and including `\n`; at a NUL it switches `g_08205c9c` to `g_08205ca0` once
  (the `"+"` is consumed first, then the real file), afterwards it parks on the terminating NUL so the next read returns 0 (EOF).
  So **every script is lexed as `"+" + <file contents>`** **[O]**. `yy_get_next_buffer` caps reads at 0x2000 bytes and requests line-sized reads.
* `Lex_RunParser` sets `g_lex_column = -1` (0x083deec4) **[O]**. Since `"+"` has length 1, the first real source byte gets column 0, so
  token positions (`yylval.pos`, `g_lex_token_pos`) are byte offsets into the file text **[I]** (consistent with
  `ScrDbg_PrintSourceContext(text, nbytes)` taking `nbytes` as an offset). Every AST "srcPos" is such an offset.
* The parser's start state 0 accepts only external token 0x119 (`+`) and 0x11b (`*`) **[O]** (computed from `yypact[0]=9`,
  `yycheck`, `yytable`). `+` selects the program rule (rule 2: `includes toplevel`). `*` appears to select alternative entry rules 3..5
  (node kinds 0x41 and 0x52, not reachable from `Scr_LoadScriptFile`) **[I]**; no caller of that entry was found **[U]** (probably a
  debugger/expression evaluator; `scrCompileDebuggerMode == 0` paths in `EmitDebuggerVariable` suggest it).
* `Lex_RunParser(int *out, char param)`: `param` is stored in `g_lex_param` (0x083deecc) and is the *flags* value passed into every
  string intern (`ScrStr_InternCStringEx(buffer, g_lex_param, 0xd)`, `ScrStr_ReinternLower(h, g_lex_param, 0xd)`); `Scr_LoadScriptFile` passes 0 **[O]**.
  `g_lex_reserved` (0x083deed0) is reset to 0 and is used by the grammar as a constant 0 placeholder operand (see 4.4) **[O]**.
* Debug/source text: with `scr_debug_enabled` (0x0839750a) set, `ScrDbg_AddFrame` keeps a copy of each file (name + text with line breaks
  replaced by NUL) in `scr_frame_table` (0x08287334, 0x18-byte records). Without it, `ScrDbg_AddFrame` early-outs but loading still
  works **[O]** (`scr_frame_table == 0` -> `scr_last_source_text = 0; return`). `ScrDbg_LoadFrameFileOrSaved` additionally supports a *saved source stack*
  (`scr_saved_src`, 0x08287324) used instead of the file when present **[O]**; producer unknown **[U]** (savegame restore path, " (savegame)" label).
* `scr_debug_enabled` = `(com_developer != 0 || com_logfile != 0)` and `developer_script` dvar feed `Scr_Settings` (0x0807ffd8) via
  `Com_ApplyDeveloperOptions` (0x08061cdc) **[O]** (chunk_007.cpp:12..19). Third argument `abort_on_error` receives `com_developer->integer` **[O]**.
  Consequence: **debug tables and source-context error output exist only when `developer` or `logfile` is non-zero**; a default dedicated server
  prints compile errors without file/line/caret **[O]** (`ScrCompile_ErrorInFile`: `scr_debug_enabled == 0` prints only `"%s\n"`).

## 4. Front end

### 4.1 Lexer (flex 2.5.x, 0x080892d8..0x0808baed)

* Standard flex scaffolding (`yy_buffer_state` 0x30 bytes, buffer stack, `yy_scan_*`, `yywrap` always 1) **[O]**. Tables:
  `yy_accept` 0x08147cc0 (short), `yy_ec` 0x08147ec0 (256 x int32), `yy_meta` 0x081482c0 (int32), `yy_base` 0x081483c0, `yy_def` 0x081485e0,
  `yy_nxt` 0x08148800, `yy_chk` 0x08148be0 (shorts); jam base 0x1af; 0x5e is `YY_END_OF_BUFFER`; EOF actions 0x5f/0x60/0x61 (one per start condition) return token 0.
* Start conditions: `g_yy_start` 3 = normal code (set by `Lex_RunParser`), 5 = inside `/* ... */` (action 6 enters it, action 2 `*/` leaves it); in state 5 every
  other char is action 3 (skipped) **[O]** (emulator). Block comments do not nest; `//` comments run to end of line (action 5); `\r` skipped (action 4).
  Strings cannot span a newline (`"a<LF>b"` yields action 0x5c "bad token") **[O]**.
* `LEX_ADVANCE` (every action): `g_lex_token_pos = g_lex_error_pos = g_lex_column; g_lex_column += yyleng` **[O]**.
* Literals: `Lex_ParseInt` 0x08089445 = `sscanf(text,"%d",&yylval)`, `Lex_ParseFloat` 0x08089468 = `sscanf(text,"%f",&yylval)`. Hex literals are not
  recognised (`0x1F` lexes as `0` then identifier `x1F`) **[O]** (emulator). Integer overflow behaviour is that of glibc `sscanf` **[U]**.
* `Lex_StoreEscapedString` 0x08089308: decodes `\n \r \t`; any other `\c` yields `c`; strings of 0x2000 bytes or more raise
  `ScrCompile_ErrorInFile(g_lex_error_pos,"max string length exceeded: \"%s\"")` and return token 0x102. `&"..."` is an *istring* (localized
  string reference), token 0x105 **[O]**. `Lex_StoreIdentifier` 0x080892d8 interns with `ScrStr_Intern(text,0,len+1,0xd)`.
* Bad characters: action 0x5c -> `ScrCompile_ErrorInFile(tok_start,"bad token '%s'")`, token 0x102.

### 4.2 Token vocabulary (recovered by running the flex DFA, 100% from binary tables) **[O]**

```
0x102 error/bad token   0x103 identifier        0x104 "string"          0x105 &"istring"
0x106 {   0x107 }   0x108 (   0x109 )   0x10a [   0x10b ]
0x10c ||  0x10d &&  0x10e |   0x10f ^   0x110 &   0x111 ==  0x112 !=
0x113 <   0x114 >   0x115 <=  0x116 >=  0x117 <<  0x118 >>
0x119 +   0x11a -   0x11b *   0x11c /   0x11d %   0x11e !   0x11f ~
0x120 integer   0x121 float   0x122 .   0x123 ,   0x124 :   0x125 ;   0x126 =   0x127 ?
0x128 return  0x129 wait  0x12a thread  0x12b undefined  0x12c self  0x12d level  0x12e game  0x12f anim
0x130 if  0x131 else  0x132 while  0x133 for
0x134 ++  0x135 --  0x136 |=  0x137 ^=  0x138 &=  0x139 <<=  0x13a >>=
0x13b +=  0x13c -=  0x13d *=  0x13e /=  0x13f %=
0x140 .size  0x141 #using_animtree  0x142 #animtree  0x143 #include  0x144 ::
0x145 identifier containing '\' (file path)
0x146 waittill  0x147 waittillmatch  0x148 waittillframeend  0x149 notify
0x14a switch  0x14b case  0x14c default  0x14d break  0x14e continue  0x14f endon
0x150 false  0x151 true  0x152 breakpoint  0x153 prof_begin  0x154 prof_end
0x155 /#  0x156 #/  0x157 $
```

Notes: `[[` and `]]` are lexed as two `[`/`]` tokens (the parser forms the function-pointer syntax). `childthread`, `size`, `call` are plain
identifiers (CoD2 has no `childthread` keyword) **[O]**. `%name` is lexed as `%` (0x11d) followed by an identifier; `breakpoint` (0x152) has no compiler action: kind 0x4b compiles to nothing (`Scr_CompileNop`) **[I]**. Maximum keyword probing was done over a candidate list, so an additional rarely used lexeme cannot be excluded **[U]**.
`yytranslate` (0x08145e60) maps external codes 0..0x158 to internal tokens; codes above 0x158 map to 2 (undefined) **[O]**.
Identifiers are case-folded to lower case later by the parser (`fn_080858dc` 0x080858dc = `ScrStr_ReinternLower(handle, g_lex_param, 0xd)`), string literals keep case **[O]**.

### 4.3 Parser

* Bison 1.x LALR(1) `yyparse` 0x08085932 (14758 bytes), 131 rules (cases 2..0x83), `YYFINAL = 0x36`, `YYLAST = 0x522`, `YYPACT_NINF = -105`.
  Tables in `.rodata`: `yytranslate` 0x08145e60, `yyr1` 0x08145fc0, `yyr2` 0x08146060, `yydefact` 0x08146100, `yydefgoto` 0x0814614c,
  `yypact` 0x08146240, `yypgoto` 0x0814638c, `yytable` 0x08146480, `yycheck` 0x08146ee0, `yystos` 0x08147940 **[O]**.
  Stacks start at 200 entries (`short` state stack + 8-byte `yy_stype_t {value,pos}` value stack), grow by doubling with `alloca` up to 10000, then
  `"parser stack overflow"` (return 2) **[O]**. `yydestruct` 0x08085920 is a no-op.
* Semantic value: `{value, pos}`; `pos` is the token's start offset; node builders receive `pos` of selected tokens through `ScrVal_StoreWordAlt` (identity store).
* Error path: `Parse_ReportSyntaxError` 0x0808b9dd ignores its argument; with `g_parse_last_token == 0` it reports `"unexpected end of file found"`, with any token other than 0x102
  it reports `"bad syntax"` (both through `ScrCompile_ErrorInFile(g_lex_error_pos, ...)`, which does not return unless errors are deferred) **[O]**.
  Note: `g_parse_last_token` is `yychar` (0x08a6f4d4) **[I]** (same address, different names in chunks 18/19).
* Identifier-bearing rules (29-34, 58, 62, 64, 65, 114, 115, 117, 123) lower-case the identifier on reduce (`fn_080858dc`) **[O]**.
* Two global counters are touched by reductions: `g_08205c84` is incremented by rules 31/33 (kind 0x15 `file::function` reference) and rule 129 (`#include`) **[O]**;
  it is therefore "number of dependency records to reserve" (see 6.4), not merely "include record count".

### 4.4 AST representation (reconciles chunk 10/11/12 definitions)

* A node is a **variable-length array of 32-bit words** allocated from the temp-high hunk by `ScrBox_MakeN` (0x08078a04..0x08078d26, N = 1..9, 4*N bytes) **[O]**.
  Word 0 is the node kind, words 1.. are operands. The three chunk-local struct declarations (`ScrNode` 5 words, `ScriptNode` 6 words, `cod2_scr_node` 9 words)
  are fixed-size views over these variable boxes. Real sizes per kind are in the table below (words including the kind word).
* `g_083deec0` is **not** an `int[5]` context block (chunk 18) **[O]**: `0x083deec0` = parse result pointer (`g_parse_result`, written through `ScrBox_Make*(g_083deec0,...)` for the start rules),
  `0x083deec4` = `g_lex_column`, `0x083deec8` = `g_lex_error_pos`, `0x083deecc` = `g_lex_param`, `0x083deed0` = `g_lex_reserved`. The expression `g_083deec0[4]` in rules 99-107, 123 is
  `g_lex_reserved` (always 0 at that point), i.e. **a zero-initialised placeholder operand slot**. The scan pass later stores the scope-block pointer into that slot
  (`&p[3]` for `if`, `&p[5],&p[6]` for if/else, `&p[4]` for `while`, `&p[6],&p[7]` for `for`, `&p[2]` for dev blocks, `&p[5]` for functions) **[O]**.
* Lists come in two shapes **[I]** (consistent across all list users):
  * *dummy-headed* `{head,tail}` list (8 bytes; `ScrList_Create` 0x08078da0, `ScrList_Append` 0x08078e3c, `ScrList_Splice` 0x08078e94): created as
    `ScrBox_Make1(0)` + `ScrList_Create` so `head->value` is a pointer to a zeroed 1-word box and **real items start at `head->next`**. Used by statement lists, parameter lists,
    waittill variable lists, top-level and include lists. The "seq" struct in chunk 12 is this dummy head node.
  * *headless* 1-word list `{head}` (`ScrBox_Make1(0)` then `ScrList_PushFront` 0x08078df6, right-recursive): used by call-argument / expression lists; items are 8-byte pairs
    `{exprNode, srcPos}` (`ScrBox_Make2Alt`) and iteration starts at `head`.
* Node kinds and shapes (from `yyparse` reductions; operand count = words-1):

| Kind | Words | Operands (op0..) | Source form (inferred from rule + compiler use) |
| --- | --- | --- | --- |
| 0x02 | 5 | lvalue, rhs, pos, pos | `lvalue = expr;` (rule 67) |
| 0x04 | 3 | ident, pos | local variable (lvalue/rvalue) (64) |
| 0x06 | 3 | primary, pos | wrapper around a primary expression (6) |
| 0x07/08/09/0a | 3 | value, pos | int / float / -int / -float literal (42-45) |
| 0x0b/0x0c | 3 | stringId, pos | `"str"` / `&"istr"` (46, 47) |
| 0x0d | 5 | arrayExpr, indexExpr, pos, pos | `a[i]` (63) |
| 0x0f | 4 | objExpr, fieldIdent, pos | `obj.field` (62) |
| 0x11 | 3 | lvalue, pos | lvalue used as primary (49) |
| 0x12 | 3 | name node (0x14/0x15), pos | function name wrapper: call target (35) or `::func` value (56) |
| 0x13 | 2 | call node | call used as expression (48) |
| 0x14 | 3 | ident, pos | plain function name (32, 34) |
| 0x15 | 4 | pathIdent, funcIdent, pos | `path::func` (31, 33) |
| 0x16 | 3 | expr, pos | `[[ expr ]]` function pointer target (36) |
| 0x17 | 4 | target(0x1a/0x1e), argList, pos | `f(args)` (39) |
| 0x18 | 6 | objExpr, target, argList, pos, pos | `obj f(args)` method call (40) |
| 0x19 | 2 | call node | call as statement (93) |
| 0x1a | 3 | name node, pos | plain call target (37) |
| 0x1e | 4 | name node, pos, pos | **`thread` call target** (38) **[I]**, see 12.2/12.3 |
| 0x1b / 0x1c | 3 / 2 | expr,pos / pos | `return expr;` / `return;` (68, 69) |
| 0x1d | 4 | expr, pos, pos | `wait expr;` (70) |
| 0x1f | 2 | pos | `undefined` (50) |
| 0x20 / 0x22 / 0x23 / 0x24 | 2 | pos | `self` / `level` / `game` / `anim` (51-54) |
| 0x25 | 5 | cond, body, pos, **scopeSlot** | `if` (99) |
| 0x26 | 8 | cond, ifBody, elseBody, posIf, posElse, **ifScope**, **elseScope** | `if/else` (100) |
| 0x27 | 6 | cond, body, posCond, posLoop, **bodyScope** | `while` (101) |
| 0x28 | 9 | init, cond, iter, body, posCond, posLoop, **bodyScope**, **iterScope** | `for` (102) |
| 0x29 / 0x2a | 3 | lvalue, pos | `x++;` / `x--;` (71, 72) |
| 0x2b | 5 | lvalue, rhs, opcode(0x66..0x75), pos | compound assign `op=` (73-82) |
| 0x2c | 4 | stmtList, pos, pos | `{ ... }` block (98) |
| 0x2d | 4 | stmtList, pos, **scopeSlot** | `/# ... #/` inside a function (104) |
| 0x2e | 3 | exprList, pos | `( e1, e2, e3 )` grouping / vector literal (41) |
| 0x2f / 0x30 | 6 | lhs, rhs, posOp, posEnd, ... | `\|\|` / `&&` (7, 8) |
| 0x31 | 5 | lhs, rhs, opcode, pos | binary operator (9-24; opcode 0x66..0x75 in order `\| ^ & == != < > <= >= << >> + - * / %`) |
| 0x32 / 0x33 | 3 | expr, pos | `!` / `~` (25, 26) |
| 0x34 | 3 | expr, pos | `.size` (55) |
| 0x35 | 3 | expr, pos | debugger-only "self field" form; compile error otherwise (66) **[U]** exact syntax |
| 0x37 / 0x38 | 5 | obj, varList, pos, pos | `obj waittill(...)` / `obj waittillmatch(...)` (83, 84) |
| 0x39 | 2 | pos | `waittillframeend;` (85) |
| 0x3a / 0x3b | 5 | obj, args, pos, pos | `obj notify(...)` / `obj endon(...)` (86, 87) |
| 0x3c | 4 | expr, caseList, pos | `switch` (103) |
| 0x3d / 0x3e | 4 / 3 | caseExpr,pos,**scopeSlot** / pos,**scopeSlot** | `case X:` / `default:` (106, 107) |
| 0x3f / 0x40 | 2 | pos | `break;` / `continue;` (88, 89) |
| 0x41 / 0x52 | 2 | child | alternative start entries (3-5) **[U]** |
| 0x42 | 2 | pos | `[]` empty array (57) |
| 0x43 | 3 | ident, pos | `%anim` animation reference (58) |
| 0x44 | 7 | name, params, body, posName, posEnd, **scopeSlot** | function definition (123) |
| 0x45 / 0x46 | 2 | pos | `/#` / `#/` at top level (125, 126) |
| 0x47 | 4 | treeIdent, pos, pos | `#using_animtree(name);` (124) |
| 0x4a | 2 | pos | `#animtree` (61) |
| 0x48 / 0x49 | 2 | pos | `false` / `true` literals (59, 60) |
| 0x4b | 2 | pos | `breakpoint;` (90), compiled to nothing **[I]** |
| 0x4c / 0x4d | 3 | arg, pos | `prof_begin(..)` / `prof_end(..)` (91, 92) |
| 0x4f | 3 | ident, pos | `$name` debugger variable (65) |
| 0x55 | 3 | path, pos | `#include path;` (129) |

Kind values 0x00, 0x01, 0x03, 0x05, 0x0e, 0x10, 0x21, 0x36, 0x4e, 0x50, 0x51, 0x53, 0x54 are never built by `yyparse` **[O]** (checked against all `ScrBox_Make*` first arguments); kind 0x41/0x52 are built only by start rules 3-5.

## 5. Code generation

### 5.1 Code buffer and positions

* One growing buffer. `ScrBuf_Advance(delta)` 0x0807a2e4 keeps a running offset `g_08297484`, calls `Hunk_ReallocateTempMemory(newOffset)` and returns `base + oldOffset`
  as an **absolute pointer**; `ScrBuf_Rewind(mark)` 0x0807a340 truncates; `ScrBuf_Reset` zeroes the offset **[O]**. Base `g_08397548` = `Hunk_AllocLow(0)` at `Scr_InitCompileState`.
  `ScrBuf_AdvanceA/B` are byte-identical wrappers (0x0807a318, 0x0807a32c). Code length `g_08205cb0` is recomputed (`pos - base`) after each unit's top-level compile (0x08075a26); because nested dependency units finish later, the last assignment equals the total size **[I]**.
* **Bytecode contains absolute addresses** (`EmitCodePos` writes absolute function addresses; jumps through `ScrBuf_Advance(0)`) **[O]**; it is not relocatable and must be generated at the final address.
* Source positions: `ScrDbg_PushLine(srcPos, flags)` 0x080773d2 appends a position record keyed by the address of the **last emitted opcode** (`scrLastOpcodePtr` 0x08205cac). Tables: `scr_file_table` (0x08287300, cap 0x10000, 0x14 bytes) is really an
  *opcode-address -> record range* table, `scr_line_table` (0x0828730c, 8 bytes `{value,flags}`) holds the positions; both double when full **[O]**. Records with value -1/-2 are placeholders resolved later
  (`ScrDbg_SetPendingEntry` 0x08077656). `ScrDbg_PopLine` removes the last record when the peephole fuses opcodes. Recording is disabled when `scr_debug_enabled == 0` or `scrCompileMode == 2`.
  Flag bit 0 is cleared when `scrOpcodeAtStackBase == 0` (0x08205ca5; set when stack depth was 0 or the opcode is a call kind 2/3). Lookup: `ScrDbg_FindFileEntryFloor` 0x080779f6, `ScrDbg_LineValue` 0x08077b6a,
  `ScrDbg_GetFileAndLine` 0x080785ac, `ScrDbg_PrintLocation` 0x080783fa.
* Hash: `EmitOpcode` maintains `scrCompileCodeHash` (0x0839753c) `= hash*0x1f + opcode` **[O]**; `EmitFunctionEpilogue`, `ScrCompile_IfElseStatement` and `Scr_CompileGuardedBlock` save/restore or adjust it (`saved+1`) so the hash is
  independent of the alternative tail-return shapes **[O]**. Consumer: `Scr_GetStatSnapshot` 0x0807fd12 only (found) **[U]** whether the value is networked/compared.

### 5.2 EmitOpcode (0x0806e120): the single sink

Order of work **[O]**: flush all pending deferred constants (`FlushDeferredValue`), set `scrOpcodeAtStackBase`, add `stackDelta` to `scrCompileStackDepth` (tracking
`scrCompileMaxStackDepth`, and `scrCompileMaxCallStackDepth` when `callKind != 0`), update the hash, then try to **fuse** with `scrLastOpcodePtr`, else emit the byte.
Function limit: `Scr_CompileFunctionBody` raises `"function exceeds operand stack size"` when `maxCallStackDepth*0x20 + maxStackDepth > 0x7ff` **[O]**.

Fusion table (new opcode if last opcode == X -> rewrite last to Y; `PopLine` removes the position record in the starred cases). Opcode names are from chunk 16's `OP_*` enum (matches the VM's dispatch) **[O]** (values), names **[I]**:

| New opcode | Last | Rewritten to |
| --- | --- | --- |
| 0x20 EvalArray | 0x1e EvalLocalVariableCached* | 0x1f EvalLocalArrayCached (also last 0x18..0x1d -> 0x1f followed by the index byte `last-0x18`)* |
| 0x23 EvalArrayRef | 0x37* / 0x36* | 0x22 / 0x21 EvalLocalArrayRefCached(0) |
| 0x2a EvalFieldVariable | 0x26 / 0x0d / 0x0e | 0x29 / 0x27 / 0x28 (self/level/anim field) |
| 0x2e EvalFieldVariableRef | 0x26 / 0x0d / 0x0e | 0x2d / 0x2b / 0x2c |
| 0x31 SafeSetVariableFieldCached0 | 0x16 CreateLocalVariable | 0x30 SafeCreateVariableFieldCached |
| 0x39 SetVariableField | 0x37* / 0x36* / 0x2d* / 0x2b* / 0x2c* | 0x3d / 0x3c / 0x3b / 0x38 / 0x3a |
| 0x50 ScriptFunctionCall | 0x4e PreScriptCall | 0x4f PreScriptCallThenCall |
| 0x52 ScriptMethodCall | 0x0f GetSelf* | 0x50 ScriptFunctionCall; if the opcode before was 0x4e: the last two collapse to 0x4f |
| 0x56 ScriptMethodThreadCall | 0x0f GetSelf* | 0x54 ScriptThreadCall |
| 0x59 CastFieldObject | 0x1e / 0x18..0x1d | 0x5a EvalLocalVariableObjectCached (plus index byte) |
| 0x5e JumpOnFalse | 0x5c BoolNot* | 0x5f JumpOnTrue |

### 5.3 Opcode -> emitter map (compiler side)

(Chunk-local names in parentheses are the names currently in `recon/chunks`; proposals in section 12.)

| Opcode(s) | Emitter | Encoding |
| --- | --- | --- |
| 0x00 End | `EmitOp_00` 0x0806e50e | none |
| 0x01 Return | `EmitOp_01` 0x0806e532 | stack -1 |
| 0x02 GetUndefined | `EmitGetUndefined` 0x0806e68a | |
| 0x03..0x08 | `EmitGetInteger` 0x0806e6e4 | 0 / u8 / -u8 / u16 / -u16 / i32; narrowest chosen |
| 0x09 GetFloat | `EmitGetFloat` 0x0806e8ba | 4 bytes |
| 0x0a/0x0b GetString/IString | `EmitGetString` 0x0806f30a / `EmitGetIString` 0x0806f382 | u16 string **handle** (not canonical slot) |
| 0x0c GetVector | `EmitGetVector` 0x0806f3d8 | 3 floats; components are stored in **reverse** order of the written expression (`MakeVectorValue` writes `xyz[2-i]`) **[O]** |
| 0x0d/0x0e/0x0f/0x10/0x11/0x12/0x13/0x14/0x15 | `EmitGetLevelObject`, `EmitGetAnimObject`, `EmitGetSelf`, `EmitGetLevel`, `EmitGetGame`, `EmitGetAnim`, `EmitGetAnimation` (0x0806fa6a), `EmitOp_14` (GetGameRef), `EmitGetFunction` | GetAnimation emits a placeholder int -1 and registers a fixup with `Anim_AddNodeToCurrentTree` |
| 0x16 CreateLocalVariable / 0x17 RemoveLocalVariables | `Compile_LookupLocalVar` 0x0806e9b2 (lazy), `ScrLocalBlock_EmitPendingVars` 0x0806eb12 / `EmitRemoveLocalVars` 0x0806eb82 | 0x16: u16 canonical slot; 0x17: u8 count |
| 0x18..0x1d, 0x1e EvalLocalVariableCached* | `EmitEvalLocalVariableCached` 0x0806f7f4 | offsets 0..5 are dedicated opcodes, else 0x1e + u8 offset |
| 0x20/0x23/0x24/0x25/0x26 | `EmitOp_20/23/24/25/26` | EvalArray / EvalArrayRef / ClearArray / EmptyArray / GetSelfObject |
| 0x2a/0x2e/0x2f | `EmitOp_2a_Expr` 0x0806fad4, `EmitOp_2e_Expr` 0x0806fb30, `EmitOp_2f_Expr` 0x0806fb78 | u16 canonical field slot |
| 0x31/0x32 (named EvalLocalVariableRefCached) | `EmitEvalLocalVariableRefCached` 0x0806f1dc | SafeSetVariableFieldCached0/ + u8 offset **[O]** values; name wrong, see 12.3 |
| 0x33 | `EmitClearLocalVariableFieldCached` 0x0806f280 | SafeSetWaittillVariableFieldCached + u8 |
| 0x35 checkclearparams | `Scr_CompileParamList` 0x080752b2 | after one 0x31/0x32 per parameter |
| 0x36/0x37 | `EmitSetLocalVariableFieldCached` 0x0806f878 | EvalLocalVariableRefCached0/ + u8; always followed by 0x39 |
| 0x39 SetVariableField | `EmitOp_39` 0x0806e936 | stack -1 |
| 0x3e..0x43 / 0x44 | `ScrCompile_EmitCallOpcode` 0x08070a78 | builtin call: `0x3e+argc` for argc < 6 else 0x44 + u8 argc; then u16 index into `scrFuncTable`; stack `1-argc` |
| 0x45..0x4a / 0x4b | `ScrCompile_EmitMethodCallOpcode` 0x08070ade | builtin method: `0x45+argc` else 0x4b + u8; stack `-argc` |
| 0x4c wait / 0x4d waittillframeend | `ScrCompile_Op4C` 0x080727e8 / `ScrCompile_Op4D` 0x08072858 | |
| 0x4e PreScriptCall | `ScrCompile_EmitCallPrefix` 0x0807085e | emitted only for kind 0x1a call targets |
| 0x50/0x52, 0x51/0x53 | `EmitCallFunction` 0x0807053e, `EmitCallFunctionPtr` 0x080705b4 | then 4-byte code address via `EmitFunctionRef` (0x50 `-argc`, 0x52 `-1-argc`; call kind 3) |
| 0x54/0x56, 0x55/0x57 | `EmitCallFunctionMethod` 0x08070640, `EmitCallFunctionPtrMethod` 0x080706c4 | thread variants; followed by the argument count as a 4-byte dword; call kind 2 |
| 0x58 DecTop | `EmitOp_58` 0x0806fd46 | after call statements |
| 0x59 CastFieldObject | `EmitOp_59` 0x0806fd6a | object-position call result |
| 0x5b/0x5c/0x5d | `EmitOp_5b/5c/5d` | CastBool / BoolNot / BoolComplement |
| 0x5e/0x5f/0x60/0x61 | if / while cond; `ScrCompile_ShortCircuit60/61` | u16 forward displacement relative to just after the operand; patched as `(short)(end) - (short)(start)` **[O]** |
| 0x62 jump / 0x63 jumpback | `ScrCompile_IfElseStatement`, break/continue, `ScrCompile_WhileLoop`, `ForLoop` | 0x62 has an i32 displacement; 0x63 a u16 (`end - loopStart`) |
| 0x64 inc / 0x65 dec | `Scr_CompileExprStmt_Op100/Op101` | lvalue, then op, then 0x39 |
| 0x76 size | `EmitOp_76` 0x0806f622 | |
| 0x77 / 0x78 | `Scr_CompileLoop_Multi` 0x08073ba8 / `Scr_CompileLoop_Single` 0x08073aa6 | waittillmatch (with trailing u8 count) / waittill; variables are bound by 0x33 per name, then 0x34 clearparams |
| 0x79 notify, 0x7a endon, 0x7b voidCodepos | `Scr_CompileListStmt_3a` 0x08073cf4, `Scr_CompileSimple_3b` 0x08073dea | notify emits 0x7b first |
| 0x7c switch / 0x7d endswitch | `Scr_CompileSwitchStmt` 0x08074404 | see 5.6 |
| 0x7e vector | `ScrCompile_VectorExpr` 0x08071608 | non-constant 3-element list |
| 0x81 / 0x82 | `EmitDebuggerVariable` 0x0806fbd4 | `$eNN`-style object / `$tNN` thread variable (debugger only) |
| 0x85 / 0x86 prof_begin/end | `Scr_CompileMarker` 0x0807487e | emitted (as op + byte 0) **only when `developer_script` is on** |

Opcodes 0x7f NOP and 0x80 abort are never emitted by this compiler **[O]** (no emitter found).

### 5.4 Local variables and the scan pass

* Scope object `ScrLocalBlock` (0x118 bytes, section 13): word0 is a **control-flow state** (0 live, 1 ended by `continue`, 2 by `break`, 3 by `return`) **[I]** (written by
  `Scr_CompileBreakStmt` 0x080746c6 `*state = 2`, `Scr_CompileContinueStmt` 0x080747a0 `*state = 1`, return paths `scope->state = 3`). Chunk 10 calls it `inactive`.
  Up to 0x40 variables per function (`Compile_CheckLocalVarCount`: `"LOCAL_VAR_STACK_SIZE exceeded"` at count >= 0x40, also checked when merging) **[O]**.
* Locals are **stack positions**, not names, at run time: `Compile_LookupLocalVar` returns `(emittedCount - index) - 1`, the distance from the top of the local stack **[O]**. `CreateLocalVariable` (0x16) opcodes are emitted
  lazily the first time a variable is referenced or when `ScrLocalBlock_EmitPendingVars` is called (loop heads) **[O]**.
* Pass 1, `Scr_ScanStmt` (0x08074e8a) mirrors `Scr_CompileStmt`; it assigns variables (`ScrLocalBlock_AddVar*`), computes the control-flow state, merges branch scopes
  (`ScrLocalBlock_IntersectInit`, `MergeVars`, `ApplyMerged`, `RebaseInto`, `Copy`) and **stores scope pointers into the AST** (the placeholder slots above).
  A variable is "initialised" only if initialised on all joining paths (`initBits`, bit per variable) **[O]**. Using an uninitialised variable raises `"uninitialised variable '%s'"`;
  code after a `return` in the same block raises `"unreachable code"` (reported from `Compile_LookupLocalVar` when the block is dead) **[O]**.
  `scrCompileUninitIsError` (0x08206d10) forces the stricter check while compiling `x++`/`x--` left sides **[O]**.
* Parameters are scanned with `Scr_ScanParamList` (adds vars) and compiled by `Scr_CompileParamList`: one 0x31/0x32 (SafeSetVariableFieldCached) per parameter followed by 0x35 **[O]**.
  `waittill(...)` variable lists compile to 0x33 per name followed by 0x34 clearparams **[O]**.
* Break/continue use patch lists `g_08206cf0` / `g_08206cf8` (12-byte records `{slotPtr, pos, next}`): patched at loop end with `(currentPos - recordPos)` as **i32** **[O]**; `break` is legal only when `g_08206cec` (breakable) is set and the scope is live: `"illegal break statement"`, `"illegal continue statement"`.
* Developer blocks `/# ... #/`: `Scr_PreScanDevComment` (0x0807531a) checks pairing (`"cannot recurse /#"`, `"#/ has no matching /#"`, `"/# has no matching #/"`) and pre-defines functions declared inside dev regions with kind 0xc (`VAR_DEVELOPER_CODEPOS`) when
  `developer_script` is on, else dev functions are compiled in "discard" mode.

### 5.5 `scrCompileMode` (0x08205ca8)

0 = normal; 1 = inside a developer region that is kept (`developer_script` on); 2 = inside a developer region that is compiled and then discarded (`developer_script` off): `Scr_CompileGuardedBlock` (0x080751c4), `Scr_CompileFunctionNodeDev` (0x080756e4),
`ScrCompile_DevBlockEnter/Leave` (0x080709a6 / 0x080709fe) save `ScrBuf_Advance(0)` and `ScrBuf_Rewind` it **[O]**. In mode 2: position records are suppressed, string-reference accounting is rolled back (`EmitStringRef`, `EmitCanonicalString`),
case entries are not recorded (`Scr_AddCaseEntry`), and `EmitFunctionRef` undoes the dependency-record count for `file::func` nodes (`g_08205c84--`) **[O]**. References from non-dev code into dev functions are errors:
`"normal script cannot reference a function in a /# ... #/ comment"`.

### 5.6 Expressions, folding, calls, switch

* Constant folding is real and observable **[O]**: literal nodes and binary operators over literals fold via `Scr_EvalBinaryOperator` (using the VM's evaluators, errors reported through `scrVarPub_error_message` at `0x08397510`, printed with `"%s"`);
  folded values are kept on the 0x20-entry deferred stack (`scrValueStack` 0x08206d1c, 12 bytes each: `{value,type,srcPos}`; overflow `"VALUE_STACK_SIZE exceeded"`) and emitted on demand by `FlushDeferredValue` so that the narrowest literal opcode is used.
  Vector literals `(a,b,c)` of constants fold to a constant vector via `Scr_CloneVector`. `x = undefined` to a field or array element compiles to Clear* (0x2f/0x24) instead of push+set. `!`/`~` and `&&`/`||` are not folded.
  `while(<const false>)` raises `"conditional expression cannot be always false"`; `while(<const true>)` omits the condition jump. Constant conditions in `if` are not folded.
* Short-circuit: `||` (kind 0x2f) emits JumpOnTrueExpr 0x61, `&&` (kind 0x30) emits JumpOnFalseExpr 0x60, each with u16 displacement, then CastBool of the rhs **[O]**.
* Calls: `ScrCompile_FunctionCall` first tests whether the name exists in the **current file's function table** (`g_08206cc8`, `ScrVar_FindVarValueId`); only an *unresolved* bare name is looked up as a builtin, through a cache
  object `g_08205c90` (`Scr_GetFunction_08117cb2` 0x08117cb2, table `0x08188040`, 0x91 entries) or `g_08205c94` for methods (`Scr_GetMethod` 0x08117dea). **A function defined in the same file shadows a builtin of the same name; a function that only comes from `#include` or `file::` does not** (the builtin wins, because imported entries are added after code generation) **[O]** (order of `Scr_CompileTopLevelList` then the import loop; test in `ScrCompile_GetUnresolvedCalleeId`). Developer-only builtins (flag in table, tag 0xc)
  are wrapped in `DevBlockEnter/Leave`; using their value outside `/# #/` raises `"return value of developer command can not be accessed if not in a /# ... #/ comment"`. Arguments > 255: `"parameter count exceeds 256"` (check is `> 0xff`). Builtin handler pointers are interned in `scrFuncTable` (0x08205cb8, 0x400 entries, linear search,
  `"SCR_FUNC_TABLE_SIZE exceeded"`) and referenced by u16 index **[O]**.
* `ScrCompile_MethodCall`: note the builtin lookup order in `Scr_GetMethod` evaluates *all four* lookups every time (player, game-entity, entity table, hudelem), first non-null wins **[O]** (chunk_050.cpp).
* User function calls to not-yet-compiled functions are emitted with a 4-byte placeholder; see 6.3.
* `switch` (0x0807491a case 0x3c -> `Scr_CompileSwitchStmt` 0x08074404): emits `0x7c` + i32 displacement to the case table, body, `0x7d` + u16 count, then `count` entries of `{i32 value, i32 codeAddress}` sorted **descending by unsigned value** (`Scr_CompareCaseKeyDesc`, `qsort`).
  Case values: integers (must satisfy `Scr_IsSmallNameIndex`, `"case index %d out of range"`, stored via `Scr_BiasName24`) or strings (string handle); anything else `"case expression must be an int or string"`; duplicates `"duplicate case expression"`;
  statements before the first label `"missing case statement"`; at most 0x3ff cases (`"MAX_SWITCH_CASES exceeded"`). A `default` is stored with value 0.
* `case`/`default` outside a switch: `"illegal case statement"` / `"illegal default statement"`.

### 5.7 Statements compiled specially

`if` uses `0x5e` with a **u16** displacement (programs whose if-body exceeds 64 KiB silently wrap) **[O]**; if/else uses `0x62` with i32; tail-position returns (`flag` set when the statement is last in the function or followed only by dev blocks while `developer_script` is off,
`Scr_ListTailAllType2D` 0x08073e94) replace the jump by `OP_End` plus a 4-byte pad **[O]** (`EmitOp_00(); EmitInt32(0)`).
`while`/`for` emit `0x63 jumpback` with a u16 backward displacement. `for` and `while` re-run `ScrLocalBlock_EmitPendingVars` at the loop head.

## 6. Symbols, includes and linking

### 6.1 Symbol tables (all are script-variable objects keyed by canonical-string id)

| Global | Meaning (as used) | Evidence |
| --- | --- | --- |
| `0x08205c88` | loaded scripts: name id -> (integer when finished) | `Scr_LoadScriptFile`, `Scr_CompileScriptUnit` |
| `0x08205c8c` | per-file function tables: name id -> object (child key = function name id) | `Compile_AddFuncRecord`, `Scr_LoadScriptFile` |
| `0x08205c90` | builtin function cache: name id -> `{fn pointer, tag}` | `ScrCompile_FunctionCall` |
| `0x08205c94` | builtin method cache | `ScrCompile_MethodCall` |
| `0x08206cc8` | **current file's** symtab (function table) | `Scr_CompileScriptUnit` first statement |

Per function symbol: `child[1]` = `{codeAddress, tag}` with tag 7 (`VAR_CODEPOS`) normal, 0xc (`VAR_DEVELOPER_CODEPOS`) developer-only, 0xd (`VAR_INCLUDE_CODEPOS`) imported by `#include`; tag 0 means referenced but undefined;
`child[0]` is a fixup list: `{count}` with children `index+2` holding absolute pointers to 4-byte placeholders **[O]** (`EmitFunctionRef`, `ScrCompile_ResolveFunctionRefs` 0x08070fd4).
Duplicate definitions: `"function '%s' already defined"` / `"function '%s' already defined in '%s'"` (second form names the file via `ScrDbg_FindFrameIndex` and `scr_frame_table[idx].name`) (`ScrCompile_DefineFunction` 0x080711ce).

### 6.2 Canonical string slots

Variable names and field names in bytecode are **16-bit slot numbers**, not string handles: `Scr_AssignSlot` (0x08076034) keeps a 0x20000-byte `u16[0x10000]` table `g_08205c98` indexed by string handle,
numbering slots from 1 via counter `g_08397508`; `ScrStr_SetFlagOrDecRef(id, 2)` marks the string. The VM consumes these as variable ids (`OP_CreateLocalVariable`: `ScrVar_NewChildAVarId(localId, u16)`) **[O]**.
String literals (`OP_GetString`) carry the raw pool handle **[O]**. Pools are created by `Scr_InitSymbolPools` (0x08075fe8: `Hunk_GetHighPermanent`, `Hunk_Alloc(0x20000)`) and released by `Scr_FreeSymbolPools` (`Hunk_SetHighMark`).
`Scr_GetOrAssignSlot` 0x080760a2 is used by chunk 15 to translate field names.

### 6.3 Forward references and link order

`EmitFunctionRef` (0x08070124): for a plain name it looks up/creates the entry in `g_08206cc8`; for `file::func` it normalises the path, creates a dependency record, and resolves in the other file's symtab. If the target address is known it is emitted directly; otherwise
a 4-byte word (initially 1 for file-qualified, 0 for plain, i.e. `flag`) is emitted and its absolute address recorded; `ScrCompile_ResolveFunctionRefs` later writes the final address into every recorded slot, or fails with
`"unknown function"` (plain name never defined; file-qualified reference to a target file with no such function). `ScrCompile_ResolveAllFunctionRefs` runs after all dependencies of a unit are loaded.

### 6.4 Dependency records and `#include`

`Scr_CompileScriptUnit` allocates `g_08205c84 * 12` bytes (`Com_MallocZeroed`) **before** compiling, pointed to by `scrFuncRecordPtr` (0x08206d14). `Compile_AddFuncRecord` appends `{u16 scriptNameId, u8 flag, pad, i32 srcPos}` (flag 1 = `#include`, 0 = `path::func` reference).
`g_08205c84` is bumped by the parser per `#include` and per `path::func` node and decremented again by `EmitFunctionRef` in discard mode, so the record count equals the number of records actually appended **[O]**. Record 0's trailing dword is used as the
chain link to the previous block (`g_08206d18`); `Scr_FreeIncludeBlocks` frees the chain **[O]**.

Dependency loop (0x08075a26): for each record, `Scr_LoadScriptFile(name)` (**recursive full compile**); failure -> `"Could not find script '%s'"`. For `#include` records the loaded file's functions with tag != 0xd (not themselves imported) are copied into the current symtab with tag 0xd,
and the current file's pending references to them are resolved; a later include of the same script id in the include prefix of the array is `"Duplicate #include"` (only forward comparison within the contiguous `pending` run) **[O]**. Include is therefore **not transitive** (imported entries carry tag 0xd and are skipped when another file includes this one) **[O]**. Importing goes through `ScrCompile_DefineFunction`, so an include that provides a name already defined in the including file (or by an earlier include) is a compile error (`"function '%s' already defined"` / `"... already defined in '%s'"`) **[I]**.
Important ordering consequence: imports happen **after** the including file's own functions were compiled, so while compiling calls, names that come only from other files are not yet in the current symtab **[O]**.
After the includes, `Hunk_ClearTempMemoryHigh` has freed the AST of the current unit (before dependency recursion) **[O]**: dependencies are compiled with the AST of the including file already gone.

## 7. Anim trees (`#using_animtree`, `animtrees/*.atr`)

* `#using_animtree("name")` (kind 0x47) outside dev regions calls `Scr_UsingAnimTree` 0x0806dba2: identifier check (`Scr_IsIdentifier` 0x08075e70, alnum or `_`), else `"bad anim tree name"`; registers via `Scr_RegisterAnimTree` (group 1) and sets `scrUsingAnimTreeIndex` (0x08205a54). Inside `/# #/`: `"cannot put #using_animtree inside /# ... #/ comment"`.
* `#animtree` expressions (`EmitGetAnimTree` 0x0806e8fc) emit the tree index as an integer or `"#using_animtree was not specified"`.
* `%name` animation references (OP_GetAnimation) emit an `int -1` placeholder patched by the anim system (`Anim_AddNodeToCurrentTree`).
* Group state: `scrAnimTreeRoot` (0x08205640) variable object, `scrAnimTreeCount[2]` (0x08205a4c), `scrAnimTreeNames[2][0x80]` (u16 var ids, 0x08205a68), `scrAnimTreeObjects[2][0x80]` (int, 0x0820564c). `Scr_LoadAnimTree` 0x0806dcb0 loads `animtrees/%s.atr` (`Scr_LoadAnimTreeFile` 0x0806dbee, parser `Scr_AnimTreeParse` 0x0806d4bc in chunk 9) and fatally errors with `"unknown anim tree '%s'"`.
  Chunk 12's `Scr_InitTableSlot`/`Scr_ShutdownTableSlot`/`Scr_ForEachTableSlot` are the anim-tree group init/shutdown/load-all functions (see 12.2).

## 8. Error reporting

* `ScrCompile_ErrorInFile(srcPos, fmt, ...)` 0x0807863e: formats up to 0x400 bytes; unless `scrVarPub_evaluate` (0x0839750c) is set it prints `"\n"`, `"******* script compile error *******\n"`, then either `"%s\n"` (no debug) or `"%s: "` + source context
  (`"(file '%s'%s, line %d)\n"`, the source line, a caret line `"*"`), `"************************************\n"`, then `Com_Error(5, "\x15script compile error\n(see console for details)")` **[O]**. With `scrVarPub_evaluate` the first message is stored in `0x08397510` and compilation continues (debugger evaluation) **[I]**. `Com_Error` code 5 = script-drop class **[I]** (CoD4 lineage `ERR_SCRIPT_DROP`).
* `ScrCompile_ErrorAt(pc, ...)` 0x0807873e: same banner, location via `ScrDbg_PrintLocation(pc)` (used for unresolved function references).
* All compile errors are fatal for the level load (no recovery, no error counting): a native port should abort load on the first error.

## 9. Global state (addresses)

| Address | Name used here | Type / notes | Tag |
| --- | --- | --- | --- |
| 0x08205640 | scrAnimTreeRoot | var object id | O |
| 0x08205644 | scrAnimTreeScratchObject | | O |
| 0x08205648 | scrCurrentAnimTree (`g_08205648` in chunk 12) | cleared per file in `Scr_LoadScriptFile` | O |
| 0x0820564c / 0x08205a4c / 0x08205a68 | anim tree objects / counts / names | `[2][0x80]` int, `[2]`, `[2][0x80]` u16 | O |
| 0x08205a54 | scrUsingAnimTreeIndex | reset in `Scr_CompileScriptUnit` | O |
| 0x08205a58 | animtree-groups-active flag | set by `Scr_InitTableSlot`, cleared by shutdown | I |
| 0x08205c80 | scrValueStackCount | deferred constants | O |
| 0x08205c84 | pending dependency-record count | see 4.3/6.4 | O |
| 0x08205c88 / 0x08205c8c / 0x08205c90 / 0x08205c94 | loaded scripts / per-file function tables / builtin fn cache / builtin method cache | var objects | O |
| 0x08205c98 | canonical slot table | 0x20000 bytes, u16 per string handle | O |
| 0x08205c9c / 0x08205ca0 | lexer source cursor / saved text pointer | | O |
| 0x08205ca4 | scrCompileDebuggerMode (= "compiling script files" flag; chunk 16: scrCompilePub_developer_statement) | 1 during `GScr_LoadScripts`, 0 otherwise; **inverted naming**, see 12.4 | O |
| 0x08205ca5 | scrOpcodeAtStackBase | | O |
| 0x08205ca8 | scrCompileMode | 0 / 1 / 2 | O |
| 0x08205cac | scrLastOpcodePtr (= "source key" in chunk 13) | | O |
| 0x08205cb0 | code length | | O |
| 0x08205cb4 / 0x08205cb8 | scrFuncTableCount / scrFuncTable[0x400] (chunk 16: scrBuiltinTable) | builtin handler pointers | O |
| 0x08206cc0 / 0x08206cc4 | scrLastEmitPtr / scrPrevOpcodePtr | | O |
| 0x08206cc8 | current file symtab | | O |
| 0x08206ccc | current function var id | **write-only** in this subsystem (`Scr_CompileFunctionBody`), separate dword (the chunk 12 "overlap" remark is wrong) | O |
| 0x08206cd0 / cd4 / cd8 | stack depth / max / max-at-call | | O |
| 0x08206cdc | scrCompileKeepStringRefs | | O |
| 0x08206cdd / 0x08206ce0 | in `/#` pre-scan / location of open `/#` | | O |
| 0x08206ce4 | per-function-kind "open frame" flags `[2]` | | I |
| 0x08206ce6 / ce8 / cec / ced | switch-active flag / case list / breakable / breakable-in-dev | | I/O |
| 0x08206cf0 / cf4 / cf5 / cf8 | break list / continuable / continuable-in-dev / continue list | | O |
| 0x08206cfc / d00 / d04 / d08 / d0c | scope-collection list (`ScrLocalBlock**`) / its counter / **current case scope block** / secondary list / counter | chunk 12 types `d04` as "case label slot" (wrong) | O |
| 0x08206d10 | scrCompileUninitIsError | | O |
| 0x08206d14 | scrFuncRecordPtr | | O |
| 0x08206d18 | include-record block chain | | O |
| 0x08206d1c | scrValueStack[0x20] | 12-byte entries | O |
| 0x08287300..0x08287340 | debug tables (`scr_file_table`, `scr_line_table`, `scr_frame_table` = `g_08287334`, ...) | section 5.1 | O |
| 0x0828733c | current source name | pointer; written around nested loads | O |
| 0x08297484 | ScrBuf running offset | | O |
| 0x0839750a / 0x0839750b | `scr_debug_enabled` = `scrVarPub_developer` / `developer_script` | | O |
| 0x0839750c | `scrVarPub_evaluate` (= `scr_compile_error_deferred`) | | O |
| 0x08397510 | script error message (= `scr_compile_error_msg`, `g_08397510`) | | O |
| 0x0839753c | scrCompileCodeHash | | O |
| 0x08397548 / 0x0839754c | code base / global-window end (`scr_code_base`) | | O |
| 0x083deea0..0x083deee4, 0x083def00, 0x08a6f4c0..0x08a6f4d4 | flex/bison state | section 4 | O |

## 10. Dvars and commands

* The compiler registers **no dvars and no commands itself** **[O]**. It reads `developer`, `developer_script` and `logfile` indirectly (`Com_InitDvars` 0x08061d90 registers them: `developer` int 0..2 flags 0x1000, `developer_script` bool 0x1000, `logfile` int 0..2) through `Com_ApplyDeveloperOptions` 0x08061cdc -> `Scr_Settings` 0x0807ffd8.
  `GScr_LoadLevelScript` reads dvar `mapname` (registered there with flags 0x1044) and `GScr_LoadGametypeScript` reads `g_gametype`.
* Related console commands (outside this subsystem, registered in chunk 101): `scriptUsage` (`SV_ScriptUsage_f` 0x0808cc8a) and `stringUsage` (`SV_StringUsage_f` 0x0808cc98) **[O]**.
* Cvar read timing: `Com_ApplyDeveloperOptions` must run before `GScr_LoadScripts` for debug tables to exist; call order not verified **[U]**.

## 11. Behaviours a native port must reproduce

1. Compile all needed GSC at level load (not at restart), strictly in this order: `codescripts/delete`, `codescripts/struct`, gametype, `_callbacksetup`, level script; resolve entry points by `(script,label)` with fatal errors for the required ones. **[O]**
2. Lex exactly as the DFA does (section 4.2): no hex literals, no multi-line strings, `\n \r \t` escapes only, `&"..."` istrings, `\` allowed inside path identifiers, `[[`/`]]` as two tokens, `.size` as one token, case-insensitive identifiers (lower-cased) but case-sensitive string literals. **[O]**
3. The first token fed to the parser is a synthetic `+`; error offsets are byte offsets into the file excluding that prefix. **[O]**
4. AST and scope semantics: local-variable stack layout and lazy creation of locals (`CreateLocalVariable` emitted at first use), initialisation analysis across branches, `"uninitialised variable"`/`"unreachable code"` errors, max 64 locals per function. **[O]**
5. Peephole fusion in `EmitOpcode` and the exact opcode numbering/encodings of section 5.3 (the VM interprets the fused forms). **[O]**
6. Jump encodings: u16 displacement for if/while-cond/short-circuit/jumpback, i32 for `jump`/break/continue/switch table; `(short)` truncation without range check. **[O]**
7. Constant folding rules and error texts; deferred constant stack depth 0x20; the vector component reversal. **[O]**
8. Function resolution: same-file function shadows builtin, included functions do not; `/# #/` semantics under `developer_script`; dev-only builtins; imported functions tagged 0xd; recursive dependency loading; forward-reference patching and `"unknown function"` errors. **[O]**
9. Switch table format and descending sort; case/default scope handling. **[O]**
10. Error banner format, and that any compile error aborts the load via `Com_Error(5,...)`. **[O]**
11. Dependence on `scr_debug_enabled`: with `developer == 0 && logfile == 0` no source-position tables are built and errors print without file/line. **[O]**
12. If a port keeps absolute-address bytecode it must place the image at its final address (low hunk base `0x08397548` equivalent) before emitting. A port that uses relative code addresses must remap `OP_GetFunction`, call, switch-table and `OP_GetAnimation` operands. **[I]**

## 12. Naming problems found (wrong or inconsistent names across chunks)

### 12.1 Types
1. **AST node struct**: `ScrNode` (chunk 10, 5 words), `ScriptNode` (chunk 11, 6 words), `cod2_scr_node` (chunk 12, kind + 8 ops). Evidence: nodes are variable-size `ScrBox_Make1..9` allocations (section 4.4); none of the fixed sizes is correct. Proposal: `ScrAstNode { int kind; int op[]; }` plus per-kind size table.
2. **Scope block**: `ScrLocalBlock` (chunk 10), `ScriptScope` (chunk 11, only two leading fields: `state`, `field4` = `emittedCount`), `cod2_fn_ctx` (chunk 12). All the same 0x118 object (every constructor allocates 0x118: `Hunk_AllocateTempMemoryHigh(0x118)` in 0x0806ece2 and 0x08075520, copy size 0x118).
   Chunk 10's `inactive` is the control-flow `state`. The `ctx`/`label_state` parameters in chunk 12 (`Scr_CompileStmt(node, flag, ctx, label_state)`) are `(node, isTailPosition, functionEndPos, scope)`; `label_state` is the scope block (it is the 4th argument forwarded to `ScrLocalBlock_*`).
3. **Lists**: `ScrNodeList/Link`, `ScriptList/ListItem/NodeRef`, `cod2_scr_list`, `cod2_scr_seq`, `cod2_scr_pair`, `scr_list_s/scr_list_node_s` are views of the same two list shapes (4.4). `cod2_scr_seq` is the dummy head node.
4. **Value structs**: `ScrDeferredValue` (chunk 10) and `ScriptValue` (chunk 11) are identical (12 bytes `{value,type,srcPos}`, types = VAR_*); `ScrVariableValue`, `ScriptTableValue {value,tag}`, `ScrVarValue` (chunk 14), `VariableValue` (chunks 15/16) are the same 8-byte `{u,type}`.
5. `ScrFuncRecord` (chunk 10) = `cod2_include_rec` (chunk 12); `ScriptFileEntry` (chunk 11) = `scr_frame_s` (chunk 13) at `0x08287334`; `ScriptPatchEntry` = `cod2_loop_jump`; `ScrStrSlot` (chunk 14) = `sl_hash_node_s` (chunk 13).
6. Globals typed inconsistently: `0x0820564c` `scrAnimTreeObjects` is `int[2][0x80]` (chunk 10) vs `uint8[]` (chunk 12); `0x08205c84` `int` vs "counter bumped by reductions"; `0x08397548` `int` vs `const char *`.

### 12.2 Functions: wrong or misleading names (with evidence)
| Current name (address) | Problem | Proposed |
| --- | --- | --- |
| `Scr_CompileLoop_Single` 0x08073aa6 | not a loop: pushes event name + object, emits OP 0x78 then 0x33 per variable, then 0x34 clearparams; opcode enum has `OP_waittill77/78`; tokens `waittill`/`waittillmatch` exist | `Scr_CompileWaittill` (kind 0x37) |
| `Scr_CompileLoop_Multi` 0x08073ba8 | pushes N match arguments, emits 0x77 + count byte | `Scr_CompileWaittillmatch` (kind 0x38) |
| `Scr_ScanLoop_Single` 0x08073b80 | scan helper of waittill (`ForEachPairEmit_2` = add variables) | `Scr_ScanWaittill` |
| `Scr_CompileListStmt_3a` 0x08073cf4 | emits 0x7b voidCodepos, args, 0x79 notify | `Scr_CompileNotify` (kind 0x3a) |
| `Scr_CompileSimple_3b` 0x08073dea | emits 0x7a endon | `Scr_CompileEndon` (kind 0x3b) |
| `Scr_CompileExprStmt_Op100/_Op101` 0x08073902 / 0x0807397e | opcodes 0x64/0x65 = `OP_inc`/`OP_dec` (kinds 0x29/0x2a) | `Scr_CompileIncrement` / `Scr_CompileDecrement` |
| `Scr_CompileMarker_85/_86` 0x080748d6 / 0x080748f8 | `OP_prof_begin/end`, only with developer_script | `Scr_CompileProfBegin/End` |
| `Scr_CompileGuardedBlock` 0x080751c4 | `/# ... #/` block inside function (kind 0x2d) | `Scr_CompileDevBlock` |
| `Scr_ForEachPairEmit_1/2/3` | param-list SafeSet (0x31/0x32), AddVar, waittill SafeSetWaittill (0x33) | `Scr_EmitParamSets`, `Scr_AddVarList`, `Scr_EmitWaittillSets` |
| `Scr_CompileParamList` 0x080752b2 | fine; emits 0x35 | ok |
| `EmitOp_00/01` (0x0806e50e / 0x0806e532) | OP_End / OP_Return | `EmitEnd`, `EmitReturn` |
| `EmitOp_39` 0x0806e936 | OP_SetVariableField | `EmitSetVariableField` |
| `EmitOp_58/59` | OP_DecTop / OP_CastFieldObject | `EmitDecTop`, `EmitCastFieldObject` |
| `EmitOp_5b/5c/5d` | CastBool / BoolNot / BoolComplement | `EmitCastBool`, `EmitBoolNot`, `EmitBoolComplement` |
| `EmitOp_76` | OP_size | `EmitSize` |
| `EmitOp_14/20/23/24/25/26` | GetGameRef / EvalArray / EvalArrayRef / ClearArray / EmptyArray / GetSelfObject | correspondingly named |
| `EmitOp_2a_Expr/2e_Expr/2f_Expr` | EvalFieldVariable / EvalFieldVariableRef / ClearFieldVariable | |
| `EmitAssign_20/23/24` | compile `a[i]` as eval / ref / clear | `ScrCompile_ArrayEval/Ref/Clear` |
| `EmitEvalLocalVariableRefCached` 0x0806f1dc | emits 0x31/0x32 = SafeSetVariableFieldCached | `EmitSafeSetVariableFieldCached` |
| `EmitSetLocalVariableFieldCached` 0x0806f878 | emits 0x36/0x37 = EvalLocalVariableRefCached | `EmitEvalLocalVariableRefCached` |
| `EmitClearLocalVariableFieldCached` 0x0806f280 | emits 0x33 = SafeSetWaittillVariableFieldCached | `EmitSafeSetWaittillVariableFieldCached` |
| `EmitCallFunction` 0x0807053e (3rd param `threaded`) | param selects 0x50 vs 0x52, i.e. "has self object", not "threaded" | param `hasSelf` |
| `EmitCallFunctionMethod`, `EmitCallFunctionPtrMethod`, `EmitCallMethodDispatch` | "Method" here means **thread** call (0x54..0x57), reached for kind 0x1e | `EmitThreadCall*` |
| `ScrCompile_FunctionCall` param `resultUsed` | inverted: 1 for statements (kind 0x19) and then `EmitOp_58`(DecTop) is emitted | `discardResult` |
| `ScrCompile_CallStatement` 0x0807138e | used for call as object (`call().field`), emits CastFieldObject | `ScrCompile_CallAsObject` |
| `ScrCompile_EmitCallTarget(.., isMethod, ..)` 0x0807088a | same inversion as above | `hasSelf` |
| `ScrCompile_ExprThenOp532`, `ScrCompile_Op50e`, `ScrCompile_Op4C`, `ScrCompile_Op4D` | named by *addresses*: `EmitOp_01`/`EmitOp_00` | `ScrCompile_Return`, `ScrCompile_ReturnVoid`, `ScrCompile_Wait`, `ScrCompile_WaittillFrameEnd` |
| `ScrCompile_ShortCircuit61/60` | 0x2f=`\|\|`, 0x30=`&&` | `ScrCompile_OrExpr`, `ScrCompile_AndExpr` |
| `ScrNode_IsType1F`, `ScrNode_IsWrappedType1F` | kind 0x1f = `undefined` | `ScrNode_IsUndefined` |
| `ScrCompile_TryFastLvalue` | emits clear for `x.f = undefined` / `a[i] = undefined` | `ScrCompile_TryClearAssign` |
| `Scr_ReleaseSymbolRef` 0x08075dd4 | calls `ScrVar_AddRefValue` (adds a ref) | `Scr_AddRefToValue` |
| `Scr_ReleaseSymbolRefAlt` 0x08075df4 | calls `ScrVar_ReleaseValue` (this is the real release) | `Scr_RemoveRefToValue` |
| `Scr_InitTableSlot/ShutdownTableSlot/ForEachTableSlot` 0x080761b4 / 0x08076550 / 0x08076510 | anim-tree group init/shutdown/load-all | `Scr_InitAnimTrees`, `Scr_ShutdownAnimTrees`, `Scr_LoadAnimTrees(group)` |
| `Scr_LoadScriptFile(int name_id)`, `Scr_ResolveFunctionOffset(int, int)`, `Scr_RegisterIncludeRef` | parameters are `const char *` (callers pass `ScrStr_TextFromHandle(...)` / C strings) | fix prototypes |
| `Scr_FindOrAddFuncTableEntry(fn, name)` in chunk 11 | real function takes 1 argument (0x080704c6 reads only `8(%ebp)`) | 1-arg prototype |
| `Scr_GetFunction_08117cb2` 0x08117cb2 | builtin-function lookup | `Scr_GetFunction` |
| `ScrDbg_*` (0x080771dc..0x080785ac) | tables are an opcode-address -> source-position map plus source-frame store, not "file/line" tables | `ScrPos_*` / `ScrSrc_*` (suggested) |
| `Parse_ReportSyntaxError` | chunk 18 prototypes `(const char*)`, chunk 19 `(void)`; code ignores the argument | `yyerror(const char*)` |
| `Lex_RunParser(out, param)` | `param` = string-intern flags; not "mode" | `Scr_ParseSource` |
| `Scr_AssignSlot`, `Scr_GetOrAssignSlot` | canonical slot allocation | `Scr_GetCanonicalString*` (suggested) |
| `Compile_ReleaseStringRef` | conditional release (`scrCompileKeepStringRefs == 0`) | ok |

### 12.3 Node-kind enum (chunk 11 `ScriptNodeType`)
`SCR_NODE_FUNC_REF = 0x1a` (plain call target, matches), `SCR_NODE_FILE_FUNC_REF = 0x1e` is **wrong** (0x1e is the `thread` call target; the file-qualified name is kind 0x15 under a 0x12 wrapper);
`SCR_NODE_COND_STMT = 0x41` is the alternative start entry; `SCR_NODE_OBJECT_BASE 0x11`/`IDENT_PATH 0x12`/`CALL_EXPR 0x13`/`CALL 0x17`/`METHOD_CALL 0x18` are consistent with the grammar. Full table: section 4.4.

### 12.4 Globals (same address, different names)
| Address | Names in use | Resolution |
| --- | --- | --- |
| 0x08205ca4 | `scrCompileDebuggerMode` (10, 11), `scrCompilePub_developer_statement` (16) | value 1 while script *files* are compiled (`Scr_InitCompileState`), 0 otherwise; the "script debugger" messages appear when it is **nonzero** and `"not an lvalue"` when zero -> semantics are the opposite of the name. Proposal: `scrCompileFromFile`. |
| 0x083deec0..0x083deed0 | `g_083deec0[5]` (18) vs five scalars (19) | scalars (4.4) |
| 0x08205c84 | `g_08205c84` "include record count" (12), "counter bumped by some reductions" (18) | dependency-record count |
| 0x08205cac | `scrLastOpcodePtr` (10), `g_08205cac` "source key" (13) | same variable |
| 0x08397510 | `g_08397510` const-fold message (11), `scr_compile_error_msg` (13), `scrVarPub_error_message` (16), pending cast error (15) | one variable |
| 0x0839750c | `scr_compile_error_deferred` (13), `scrVarPub_evaluate` (16) | one variable |
| 0x0839753c | `scrCompileCodeHash` (10), `g_0839753c` "saved around guarded blocks"/"counter incremented in if/else" (11/12) | rolling opcode hash |
| 0x08397548 | `scr_code_base` const char* (13), `g_08397548` int (12) | code base pointer |
| 0x08397500 | `g_08397500` int zeroed (12) vs "field table pointer" (15) | field-table pointer; `Scr_InitCompileState` clears it |
| 0x0828733c / 0x08287340 | `g_0828733c`, `scr_cur_file_name`; `scr_last_source_text` (unsigned int in 10, char* in 13) | const char* each |
| 0x0814445f | chunk 12 "empty source literal" | actually `"+"` (section 3) |
| 0x08206ccc | chunk 12 "aliases/overlaps g_08206cc8" | separate dword, write-only here |
| 0x08206d04 | chunk 12 `int *` "current case label slot" | current case scope block (`ScrLocalBlock*`) |
| 0x08205cb8 | `scrFuncTable` (10), `scrBuiltinTable` (16) | one table |

### 12.5 Tag noise (subsystem tags)
* `script/values` (0x080789e0..0x08078e94) contains the **AST builders** (`ScrBox_Make1..9`, `ScrList_*`, `ScrVal_StoreWord*`), i.e. parser support.
* `script/init` (`Scr_InitCompileState`, `Scr_ResetAll`, `Scr_ReleasePools`) belong to this subsystem; `Scr_Init_080844de`/`Scr_Shutdown_08084522` (chunk 17) do not (VM init).
* `script/symbols` mixes compiler (`Scr_AssignSlot`, `Scr_InitSymbolPools`, `Scr_IsIdentifier`, `Scr_ResolveFunctionOffset`) with VM helpers (`Scr_ReleaseSymbolRef*`, `Scr_IsInCodeRange`).
* `alloc/bucket-tree` (0x08076628..0x08076be0) and `script/alloc-mt` (0x08076d26..) are string-pool allocators and **not** compiler; they sit inside the same chunk.
* In chunk 18, `Cursor_*`, `Stack_Peek*` (0x080857be..0x080858b6) are VM bytecode-cursor helpers; `Float_RoundToInt`, `Vec3_CopyBits`, `Sys_ReadTimestampLow` are unrelated utilities; `fn_080858dc/fn_08085902` are parser-action helpers.
* Chunk 19 after 0x0808baf8 (kick/map commands) is the server subsystem.

## 13. Open questions

1. Meaning and users of alternative start token `*` (rules 3-5, node kinds 0x41/0x52): debugger expression evaluator? **[U]** No call site that feeds `"*"` was found.
2. Lexeme of token 0x152 (action 0x54) not identified; kind 0x35 syntax. **[U]**
3. Consumers of `scrCompileCodeHash` (0x0839753c) beyond `Scr_GetStatSnapshot`: is it compared across server/client or used for demo/replay consistency? **[U]**
4. `g_08205cb0` (code length) is overwritten by each nested unit: check what `Scr_IsInCodeRange` is expected to cover after multi-file loads. **[U]**
5. `scr_saved_src` producer (savegame/restart path) and whether it matters for a dedicated server. **[U]**
6. Whether `Com_ApplyDeveloperOptions` runs before `GScr_LoadScripts` at every start, and whether later `developer` changes call `Scr_Settings` again. **[U]**
7. `Scr_ResetAll` is called with an argument in chunk 49 (`Scr_ResetAll(1)`) but defined with none: check calling convention (harmless for cdecl). **[U]**
8. `g_08206ccc` has no reader in the analysed chunks: possibly read by chunk 14/17 debug code. **[U]**
9. Unreachable-code diagnostics: `Compile_LookupLocalVar` reports `"unreachable code"` only when asked for a variable in a dead block; other dead statements compile silently. Port-relevant but not exhaustively verified. **[I]**
10. Integer literal overflow semantics (`sscanf("%d")` on e.g. `99999999999`) and float literal rounding are those of glibc; confirm against the oracle. **[U]**
11. The `include` string at `0x08144450` and its use (not located in the analysed functions). **[U]**
