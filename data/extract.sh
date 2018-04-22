

if [ -z "$1" ]
    then
        (>&2 echo "need file arg")
        exit 1
fi

bzcat $1 | head -n500 | pgn-extract -C -e -V -w100000 -Wxlalg --nomovenumbers
