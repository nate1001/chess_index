
make
sudo make uninstall
sudo make install
make installcheck
psql -dchess -c"drop extension if exists chess_index"
psql -dchess -c"create extension chess_index"
