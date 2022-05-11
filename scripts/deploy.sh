
for x in `ls -a . | grep -v build | grep -v .vscode | grep -v .git`; do
    if [ $x != "." ] && [ $x != ".." ]; then
        scp -r -P 2201 $x root@121.48.165.22:~/sysyc/$x
    fi
done