#!/bin/sh
# Compile-check D168 emulator C# models without Renode (a model that fails to compile hangs Renode at load).
# usage: tools/cscheck.sh emu/models/A.cs [B.cs ...]   (all models are compiled together)
R=${RENODE_DIR:-$HOME/.local/opt/renode_1.17.0-portable}
refs=""
for d in System.Runtime System.Private.CoreLib netstandard System.Linq System.Collections System.IO System.Runtime.Extensions Infrastructure Renode; do
  f=$(find "$R" -name "$d.dll" | head -1); [ -n "$f" ] && refs="$refs -r:$f"
done
exec mcs -nostdlib -noconfig -target:library -nowarn:1701,1702 $refs -out:/tmp/cscheck_$$.dll "$@"
