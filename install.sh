
set -e

if [ -z "$PGDATABASE" ]
then
    (>&2 echo "please set the env var \$PGDATABASE for the db you want to install in. example... 'export PGDATABASE=chess'")
    exit 1
fi

make clean
make
sudo make uninstall
sudo make install
psql -c"drop extension if exists chess_index cascade"
psql -c"create extension chess_index"

#cd ../chess_games && make
#make installcheck
