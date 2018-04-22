#/usr/bin/python3


class PGN:

    TAGS = (
            "termination",
            "timecontrol",
            "opening",
            "eco",
            "blackratingdiff",
            "whiteratingdiff",
            "blackelo",
            "whiteelo",
            "utctime",
            "utcdate",
            "result",
            "black",
            "white",
            "site",
            "event",
            "round",
            "date")

    def __init__(self, buff):

        print(buff)
        buff.pop()
        moves = buff.pop().split()
        buff.pop()
        self.result = moves.pop()

        header = {}
        primary = {}
        while buff:
            line = buff.pop()
            pieces = line.split()
            tag = pieces.pop(0)[1:].lower()
            if tag not in self.TAGS:
                raise ValueError("unknown tag {}".format(tag))
            value = (' '.join(pieces)[1:-2])
            header[tag] = value

        primary['date'] = header.pop('utcdate')
        primary['event'] = header.pop('event')
        primary['site'] = header.pop('site')
        primary['white'] = header.pop('white')
        primary['black'] = header.pop('black')


        self._primary = primary
        self._header = header
        self._moves = moves
    
        for i, move in enumerate(moves):
            move = self._handle_move(i, move)
            print(self._move_str(move))
    
    def _handle_square(self, i, square, find_piece):
        if find_piece:
            if len(square) < 3:
                p = 'p' if i%2 else 'P'
            else:
                p = square[0]
                square = square[1:]
        else:
            p = None
        return p, square

    def _handle_move(self, i, move):

        d = {}
        if move[-1] in ('#', '?'):
            d['check'] = move[-1]
            move = move[:-1]
        else:
            d['check'] = ""
        if move.find("-") > -1:
            source, dest = move.split('-')
            d['capture']= False
        else:
            source, dest = move.split('x')
            d['capture'] = True
        d['iswhite'] = i%2==0
        d['movenum'] = (i // 2) + 1
        d['piece'] , d['source'] = self._handle_square(i, source, True)
        _, d['dest'] = self._handle_square(i, dest, False)

        return d

    def _move_str(self, move):
        return "{}\t{movenum}\t{iswhite}\t{piece}\t{source}\t{dest}\t{capture}\t{check}".format(self._primary_key(), **move)

    def _primary_key(self):
        return "{date}\t{event}\t{site}\t{white}\t{black}".format(**self._primary)

    def __str__(self):
        return "<PGN {}>".format(self._primary_key())


def read_file():

    with open("sample.pgn") as f:
        nl = 0
        buffer = []
        for line in f:
            if line == '\n':
                nl += 1
            buffer.append(line)
            if nl == 2:
                nl = 0
                print (PGN(buffer))
                buffer = []
                break


if __name__ == "__main__":

        read_file()


