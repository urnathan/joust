
# $exec ROOT-SRC-DIR OUTPUT-NAME
# Generates a string literal encoding the git repo:
# $UpstreamURL($RemoteBranch) $LocalBranch:$Hash [$N ahead]-opt dirty-opt
# UpstreamURL is either a local pathname, or a git repo

TOP_SRC=$1
exec >$2
cd $TOP_SRC || exit 1

HEAD="$(git rev-parse --short=8 HEAD 2>/dev/null)" || exit 1
STATUS="$(git status --porcelain=v1 --branch)" || exit 1
INFO=$(echo "$STATUS" | head -1)
# ## main...origin/main [ahead 3]
UPSTREAM=$(echo "$INFO" | sed 's/.*\.\.\.\([^ ]*\).*/\1/')
if test "$INFO" = "$UPSTREAM" ; then
    OUTPUT="$(hostname):$(pwd)"
    REMOTEBRANCH=""
elif URL=$(git remote get-url "${UPSTREAM%%/*}" 2>/dev/null) ; then
    OUTPUT="$URL"
    REMOTEBRANCH="${UPSTREAM#*/}"
else
    OUTPUT="$(hostname):$(pwd)"
    REMOTEBRANCH=${UPSTREAM}
fi
LOCALBRANCH="$(echo "$INFO" | sed 's/## \([^ ]*\) \?.*/\1/')"
if test "$LOCALBRANCH" != "$INFO" ; then
    LOCALBRANCH="${LOCALBRANCH%%...*}"
else
    LOCALBRANCH=""
fi
if test "$REMOTEBRANCH" && test "$REMOTEBRANCH" != "${LOCALBRANCH}" ; then
    OUTPUT+="($REMOTEBRANCH)"
fi
OUTPUT+=" "
if test "${LOCALBRANCH}"; then
    OUTPUT+="${LOCALBRANCH%}:"
fi
OUTPUT+="$HEAD"

AHEAD=$(echo "$INFO" | sed 's/.*\[ahead \([0-9]\)\+].*/\1/')
if test "$INFO" != "$AHEAD" ; then
    if test "$UPSTREAM" &&
	    ORG=$(git rev-parse --short=8 "$UPSTREAM" 2>/dev/null) ; then
	OUTPUT+=" [$ORG+$AHEAD]"
    else
	OUTPUT+=" [$AHEAD ahead]"
    fi
fi
if test "$INFO" != "$STATUS" ; then
    OUTPUT+=" dirty"
fi
echo \""$OUTPUT"\"
