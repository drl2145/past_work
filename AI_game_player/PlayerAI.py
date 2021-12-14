import numpy as np
import random
import time
import sys
import os
import Utils
from BaseAI import BaseAI
from Grid import Grid

class PlayerAI(BaseAI):
    def __init__(self) -> None:
        super().__init__()
        self.pos = None
        self.player_num = None
    
    def getPosition(self):
        return self.pos

    def setPosition(self, new_position):
        self.pos = new_position 

    def getPlayerNum(self):
        return self.player_num

    def setPlayerNum(self, num):
        self.player_num = num

    # Get a move for the AI to play
    def getMove(self, grid: Grid) -> tuple:
        clone_grid = grid.clone()
        util, move = self.expminimax_move(clone_grid, 5, 1, -sys.maxsize, sys.maxsize)
        return move

    # Get a trap for the AI to play
    def getTrap(self, grid : Grid) -> tuple:
        clone_grid = grid.clone()
        util, move = self.expminimax_trap(clone_grid, 4, 1, -sys.maxsize, sys.maxsize)
        return move

    # Calculate the utility of a game state
    def utility(self, grid: Grid):
        my_neigh = grid.get_neighbors(grid.find(self.getPlayerNum()), only_available=True)
        my_sum = len(my_neigh)
        for x in my_neigh:
            more_neigh = grid.get_neighbors(x, only_available=True)
            for y in more_neigh:
                if y not in my_neigh:
                    my_sum += 1

        opp_neigh = grid.get_neighbors(grid.find(3-self.getPlayerNum()), only_available=True)
        opp_sum = len(opp_neigh)
        for x in opp_neigh:
            more_neigh = grid.get_neighbors(x, only_available=True)
            for y in more_neigh:
                if y not in opp_neigh:
                    opp_sum += 1

        return my_sum-opp_sum

    # Check if a game state is a terminal state
    def is_game_over(self, grid: Grid):
        my_neigh = grid.get_neighbors(grid.find(self.getPlayerNum()), only_available=True)
        opp_neigh = grid.get_neighbors(grid.find(3-self.getPlayerNum()), only_available=True)
        if len(my_neigh) == 0 or len(opp_neigh) == 0:
            return True
        return False

    # Use the ExpectiMinimax algorithm with alpha-beta pruning to determine a trap for the AI to play
    def expminimax_trap(self, grid: Grid, depth, is_max, alpha, beta):
        if depth == 0 or self.is_game_over(grid):
            return self.utility(grid), None

        # max
        if is_max == 1:
            max_util = -sys.maxsize
            opponent = grid.find(3-self.getPlayerNum())
            opp_neigh = grid.get_neighbors(opponent, only_available=True)
            all_neigh = list(opp_neigh)
            for x in opp_neigh:
                more_neigh = grid.get_neighbors(x, only_available=True)
                for y in more_neigh:
                    if y not in all_neigh:
                        all_neigh.append(y)
            for pos in all_neigh:
                grid.setCellValue(pos, 7)
                util, position = self.expminimax_trap(grid, depth, 2, alpha, beta)
                grid.setCellValue(pos, 0)
                if util > max_util:
                    max_pos = pos
                    max_util = util
                if util > alpha:
                    alpha = util
                if beta <= alpha:
                    break
            return max_util, max_pos

        # chance
        elif is_max == 2:
            player = grid.find(self.getPlayerNum())
            intended_throw = tuple(np.argwhere(grid.getMap() == 7)[0])
            throw_chance = 1 - 0.05*(Utils.manhattan_distance(player, intended_throw) - 1)

            grid.trap(intended_throw)
            util, position = self.expminimax_trap(grid, depth-1, 3, alpha, beta)
            grid.setCellValue(intended_throw, 0)
            chance_sum = throw_chance*util

            return chance_sum, None

        # min
        else:
            min_util = sys.maxsize
            opponent = grid.find(3-self.getPlayerNum())
            opp_neigh = grid.get_neighbors(opponent, only_available=True)
            for pos in opp_neigh:
                grid.move(pos, 3-self.getPlayerNum())
                util, position = self.expminimax_trap(grid, depth-1, 1, alpha, beta)
                grid.move(opponent, 3-self.getPlayerNum())
                if util < min_util:
                    min_pos = pos
                    min_util = util
                if util < beta:
                    beta = util
                if beta <= alpha:
                    break
            return min_util, min_pos

    # Use the ExpectiMinimax algorithm with alpha-beta pruning to determine a move for the AI to play
    def expminimax_move(self, grid: Grid, depth, is_max, alpha, beta):
        if depth == 0 or self.is_game_over(grid):
            return self.utility(grid), None

        # max
        if is_max == 1:
            max_util = -sys.maxsize
            player = grid.find(self.getPlayerNum())
            player_neigh = grid.get_neighbors(player, only_available=True)
            for pos in player_neigh:
                grid.move(pos, self.getPlayerNum())
                util, position = self.expminimax_move(grid, depth-1, 2, alpha, beta)
                grid.move(player, self.getPlayerNum())
                if util > max_util:
                    max_pos = pos
                    max_util = util
                if util > alpha:
                    alpha = util
                if beta <= alpha:
                    break
            return max_util, max_pos

        # min
        elif is_max == 2:
            min_util = sys.maxsize
            player = grid.find(self.getPlayerNum())
            player_neigh = grid.get_neighbors(player, only_available=True)
            all_neigh = list(player_neigh)
            for x in player_neigh:
                more_neigh = grid.get_neighbors(x, only_available=True)
                for y in more_neigh:
                    if y not in all_neigh:
                        all_neigh.append(y)
            for pos in all_neigh:
                grid.setCellValue(pos, 7)
                util, position = self.expminimax_move(grid, depth, 3, alpha, beta)
                grid.setCellValue(pos, 0)
                if util < min_util:
                    min_pos = pos
                    min_util = util
                if util < beta:
                    beta = util
                if beta <= alpha:
                    break
            return min_util, min_pos

        # chance
        else:
            opponent = grid.find(3-self.getPlayerNum())
            intended_throw = tuple(np.argwhere(grid.getMap() == 7)[0])
            throw_chance = 1 - 0.05*(Utils.manhattan_distance(opponent, intended_throw) - 1)

            grid.trap(intended_throw)
            util, position = self.expminimax_move(grid, depth-1, 1, alpha, beta)
            grid.setCellValue(intended_throw, 0)
            chance_sum = throw_chance*util

            return chance_sum, None
            