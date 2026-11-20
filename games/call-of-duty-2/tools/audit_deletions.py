#!/usr/bin/env python3
"""Audit function-level deletion signals across the COD2 link wave.

Uses git show for historical sources and writes reproducible raw JSON/objdump
under .local/cod2/audit. Python standard library only.
"""
import collections, difflib, json, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
BASE, WAVE = "4db502b80", "3c7ef94b4"
CHUNKS = "games/call-of-duty-2/recon/chunks"
BIN = pathlib.Path(".local/cod2/lnxded-1.3/cod2_lnxded")
OUT = pathlib.Path(".local/cod2/audit")
ADDR = re.compile(r"^\s*//\s*(0x08[0-9a-fA-F]{6,8})\b.*?(?:\b(\w+)\s+status=|\b(\w+)\s*$)?")
TOK = re.compile(r"//[^\n]*|/\*.*?\*/|\b(?:if|else|for|while|switch|case|return|goto|break|continue|throw|try|catch)\b|\b[A-Za-z_]\w*\s*(?=\()|(?:\+\+|--|\+=|-=|\*=|/=|=)|\(\s*void\s*\)", re.S)

def git(rev, path):
    p = subprocess.run(["git", "show", f"{rev}:{path}"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    return p.stdout if p.returncode == 0 else ""

def funcs(src):
    marks=list(re.finditer(r"(?m)^\s*//\s*(0x08[0-9a-fA-F]{6,8})\b([^\n]*)",src))
    out={}
    for i,m in enumerate(marks):
        end=marks[i+1].start() if i+1<len(marks) else len(src)
        body=src[m.end():end]
        # discard trailing declarations/comments; function has a brace body
        b=body.find("{")
        if b<0: continue
        depth=0; stop=len(body)
        for j,c in enumerate(body[b:],b):
            depth += (c=="{")-(c=="}")
            if depth==0: stop=j+1; break
        name=re.search(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*$",body[:b].strip())
        out[m.group(1).lower()]={"name":name.group(1) if name else "?", "body":body[b:stop],"chunk":re.search(r"chunk_(\d+)","").group(1) if False else ""}
    return out

def shape(s):
    # Strip comments, literals, casts and declaration-like type identifiers; retain observable ops/control flow.
    s=re.sub(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"])*\"|'(?:\\.|[^'])*'", " ",s,flags=re.S)
    s=re.sub(r"\(\s*(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:char|short|int|long|float|double|void|bool|size_t|uint\w+|int\w+|[A-Z]\w*(?:\s*\*)?)\s*\*?\s*\)"," ",s)
    return re.findall(r"\b(?:if|else|for|while|switch|case|return|goto|break|continue)\b|\(\s*void\s*\)|\b[A-Za-z_]\w*\s*(?=\()|(?:\+\+|--|\+=|-=|\*=|/=|=(?!=))",s)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    paths=subprocess.check_output(["git","ls-tree","-r","--name-only",WAVE,CHUNKS],text=True).splitlines()
    allrows=[]
    for p in paths:
        if not re.search(r"chunk_\d+\.cpp$",p): continue
        before,after,head=funcs(git(BASE,p)),funcs(git(WAVE,p)),funcs(git("HEAD",p))
        for a in before.keys() & after.keys():
            x,y=shape(before[a]["body"]),shape(after[a]["body"])
            sm=difflib.SequenceMatcher(a=x,b=y,autojunk=False)
            lost=[]
            for tag,i,j,k,l in sm.get_opcodes():
                if tag in ("delete","replace"): lost += x[i:j]
            if not lost: continue
            calls=sum(1 for t in lost if t not in ("if","else","for","while","switch","case","return","goto","break","continue","=","+=","-=","*=","/="))
            assigns=sum(t in ("=","+=","-=","*=","/=") for t in lost)
            returns=lost.count("return")
            flags=[]
            if calls or assigns or returns or any(z in after[a]["body"] for z in ("#if 0","(void)","//")): flags.append("loss-suspect")
            row={"path":p,"chunk":re.search(r"chunk_(\d+)",p).group(1),"address":a,"name":before[a]["name"],"lost_tokens":lost,"lost_calls":calls,"lost_assignments":assigns,"lost_returns":returns,"wave_body":after[a]["body"],"head_body":head.get(a,{}).get("body",""),"head_present":a in head,"score":calls+assigns*2+returns*3}
            if flags: allrows.append(row)
    allrows.sort(key=lambda r:(-r["score"],r["chunk"],r["address"]))
    (OUT/"deletion_candidates.json").write_text(json.dumps(allrows,indent=2)+"\n")
    print(f"Candidates: {len(allrows)}; functions common to base/wave scored; raw JSON: {OUT/'deletion_candidates.json'}")
    # Disassemble broad ranges of the top candidates, retaining original output for review.
    if BIN.exists():
        selected=allrows[:40]
        for n,r in enumerate(selected):
            start=int(r["address"],16)
            raw=subprocess.run(["objdump","-d","-M","intel",f"--start-address={start}",f"--stop-address={start+512}",str(BIN)],capture_output=True,text=True).stdout
            (OUT/f"objdump_{n:02d}_{r['address']}.txt").write_text(raw)

if __name__=="__main__": main()
