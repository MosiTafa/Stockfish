/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MOVEPICK_H_INCLUDED
#define MOVEPICK_H_INCLUDED

#include <algorithm> // For std::max
#include <cstring>   // For std::memset

#include "movegen.h"
#include "position.h"
#include "search.h"
#include "types.h"


/// The Stats struct stores moves statistics. According to the template parameter
/// the class can store History and Countermoves. History records how often
/// different moves have been successful or unsuccessful during the current search
/// and is used for reduction and move ordering decisions.
/// Countermoves store the move that refute a previous one. Entries are stored
/// using only the moving piece and destination square, hence two moves with
/// different origin but same destination and piece will be considered identical.
template<typename T, bool CM = false>
struct Stats {

  static const Value Max = Value(1 << 28);

  const T* operator[](Piece pc) const { return table[pc]; }
  T* operator[](Piece pc) { return table[pc]; }
  void clear() { std::memset(table, 0, sizeof(table)); }

  void update(Piece pc, Square to, Move m) { table[pc][to] = m; }

  void update(Piece pc, Square to, Value v) {

    if (abs(int(v)) >= 324)
        return;

    table[pc][to] -= table[pc][to] * abs(int(v)) / (CM ? 936 : 324);
    table[pc][to] += int(v) * 32;
  }

private:
  T table[PIECE_NB][SQUARE_NB];
};

typedef Stats<Move> MoveStats;
typedef Stats<Value, false> HistoryStats;
typedef Stats<Value,  true> CounterMoveStats;
typedef Stats<CounterMoveStats> CounterMoveHistoryStats;

struct FromToStats {

    Value get(Color c, Move m) const { return table[c][from_sq(m)][to_sq(m)]; }
    void clear() { std::memset(table, 0, sizeof(table)); }

    void update(Color c, Move m, Value v)
    {
        if (abs(int(v)) >= 324)
            return;

        Square f = from_sq(m);
        Square t = to_sq(m);

        table[c][f][t] -= table[c][f][t] * abs(int(v)) / 324;
        table[c][f][t] += int(v) * 32;
    }

private:
    Value table[COLOR_NB][SQUARE_NB][SQUARE_NB];
};

enum Stages {
    MAIN_SEARCH, GOOD_CAPTURES, KILLERS, QUIET, BAD_CAPTURES,
    EVASION, ALL_EVASIONS,
    QSEARCH_WITH_CHECKS, QCAPTURES_1, CHECKS,
    QSEARCH_WITHOUT_CHECKS, QCAPTURES_2,
    PROBCUT, PROBCUT_CAPTURES,
    RECAPTURE, RECAPTURES,
    STOP,
    STAGE_NB = STOP + 1
  };


/// MovePicker class is used to pick one pseudo legal move at a time from the
/// current position. The most important method is next_move(), which returns a
/// new pseudo legal move each time it is called, until there are no moves left,
/// when MOVE_NONE is returned. In order to improve the efficiency of the alpha
/// beta algorithm, MovePicker attempts to return the moves which are most likely
/// to get a cut-off first.

class MovePicker {
public:
  MovePicker(const MovePicker&) = delete;
  MovePicker& operator=(const MovePicker&) = delete;

  MovePicker(const Position&, Move, Value);
  MovePicker(const Position&, Move, Depth, Square);
  MovePicker(const Position&, Move, Depth, Search::Stack*);

  Move next_move();

private:
  template<GenType> void score();
  
  ExtMove* begin() { return moves; }
  ExtMove* end() { return endMoves; }

  const Position& pos;
  const Search::Stack* ss;
  Move countermove;
  Depth depth;
  Move ttMove;
  ExtMove killers[3];
  Square recaptureSquare;
  Value threshold;
  int stage;
  ExtMove* endBadCaptures = moves + MAX_MOVES - 1;
  ExtMove moves[MAX_MOVES], *cur = moves, *endMoves = moves;
  
public:  // for the trempolines, do not use directly
  template<Stages> void generate_next_stage();

};

typedef void (MovePicker::*Generator)(void);


constexpr Generator generators[STAGE_NB] =
  
  { &MovePicker::generate_next_stage<MAIN_SEARCH           > ,
    &MovePicker::generate_next_stage<GOOD_CAPTURES         > ,
    &MovePicker::generate_next_stage<KILLERS               > ,
    &MovePicker::generate_next_stage<QUIET                 > ,
    &MovePicker::generate_next_stage<BAD_CAPTURES          > ,
    
    &MovePicker::generate_next_stage<EVASION               > ,
    &MovePicker::generate_next_stage<ALL_EVASIONS          > ,
    
    &MovePicker::generate_next_stage<QSEARCH_WITH_CHECKS   > ,
    &MovePicker::generate_next_stage<QCAPTURES_1           > ,
    &MovePicker::generate_next_stage<CHECKS                > ,
    
    &MovePicker::generate_next_stage<QSEARCH_WITHOUT_CHECKS> ,
    &MovePicker::generate_next_stage<QCAPTURES_2           > ,
    
    &MovePicker::generate_next_stage<PROBCUT               > ,
    &MovePicker::generate_next_stage<PROBCUT_CAPTURES      > ,
    
    &MovePicker::generate_next_stage<RECAPTURE             > ,
    &MovePicker::generate_next_stage<RECAPTURES            > ,
    
    &MovePicker::generate_next_stage<STOP                  >
  };



#endif // #ifndef MOVEPICK_H_INCLUDED
